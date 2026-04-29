// Ceres - Envelope Filter / Waveshape / Wavefold / Phaser
//
// A synthy distortion with a resonant filter controlled by input amplitude.
// Mode 0 (left):   Waveshape - tanh saturation into an envelope-following resonant SVF.
// Mode 1 (center): Wavefold  - envelope-controlled wavefolding into an envelope-following lowpass.
// Mode 2 (right):  Phaser    - envelope-controlled 8-pole phaser.
//
// Knob mappings:
//   Knob 1 (FREQ):   Mode 0: Filter cutoff ceiling (100 - 8000 Hz, log)
//                     Mode 1: Post-fold filter cutoff ceiling (100 - 8000 Hz, log)
//                     Mode 2: Phaser allpass frequency ceiling (40 - 5000 Hz, log)
//   Knob 2:          Mode 0 (RES): Resonance / feedback of the filter (0 - 1)
//                    Mode 1 (FOLD): Max fold gain — envelope-driven fold depth (1x - 12x, exp)
//                    Mode 2 (FDBK): Phaser feedback (0 - 0.75)
//   Knob 3 (SENSE):  Envelope sensitivity gain (1x - 20x, logarithmic) — all modes
//   Knob 6 (LEVEL):  Output level (0 - 1)
//
// Switch 1:       Mode select (left=Waveshape, center=Wavefold, right=Phaser)
// Footswitch 1:   Bypass
//
// Expression pedal (v3 board): Directly controls envelope (overrides input envelope follower)
// MIDI CC19:                   Directly controls envelope (overrides input envelope follower)
//

#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"
#include <cmath>

using namespace daisy;
using namespace daisysp;
using namespace funbox;

// Hardware
DaisyPetal hw;

// Parameters — knobs 1-6
Parameter pFreq, pRes, pSense, param4, param5, pLevel;
// Mode 1 re-interprets knob 1 as post-fold FREQ, knob 2 as FOLD
Parameter pFreqWF, pFold;
// Mode 2 re-interprets knob 1 as phaser FREQ, knob 2 as FDBK
Parameter pPhaserFreq, pPhaserFb;
// Expression pedal input (v3 board)
Parameter pExpression;

bool            bypass;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];

Led led1, led2;

// External envelope control (expression pedal / MIDI CC19)
float externalEnv    = 0.0f;  // Combined external envelope value (0-1), computed per block
bool  midiEnvActive  = false; // True when MIDI CC19 is actively controlling envelope
float midiEnvValue   = 0.0f;  // Last received MIDI CC19 value (0-1)

// Threshold for expression pedal detection (above this = pedal connected and moved)
static const float EXPRESSION_THRESHOLD = 0.05f;

// DSP state
Svf filt;
Svf filtWF;   // Post-wavefold envelope-following lowpass (mode 1)
Wavefolder wfold;

// Manual 4x biquad (second-order) allpass phaser — 8 poles total, deep notches
static const int PHASER_STAGES = 4;
struct BiquadAP {
    float s1, s2;  // Direct Form II Transposed state
};
BiquadAP apStages[PHASER_STAGES] = {};
float phaserFeedbackSample = 0.0f;

// Stagger ratios — spread biquad stages so notches cover the guitar range
static const float stagger[PHASER_STAGES] = {
    1.0f, 2.2f, 5.0f, 11.0f
};

// Q controls notch depth/width — higher Q = narrower, deeper, more resonant
static const float PHASER_Q = 0.707f;

// Envelope follower state (mode 0)
float envFollower = 0.0f;

// Envelope follower coefficients (mode 0, fixed)
float envAttack  = 0.0f;
float envRelease = 0.0f;

// Envelope follower state (mode 1, with fixed long decay)
float envFollowerWF = 0.0f;
float envReleaseWF  = 0.0f;

// Envelope follower state (mode 2 — phaser, adjustable decay)
float envFollowerPH = 0.0f;
float envReleasePH  = 0.0f;

// LED envelope follower (fast release, shared across modes)
float envFollowerLED = 0.0f;
float envAttackLED   = 0.0f;
float envReleaseLED  = 0.0f;

// Sample rate (stored for per-block decay recalculation)
float srate = 48000.0f;

// Knob values
float vFreq  = 200.0f;
float vRes   = 0.5f;
float vSense = 0.5f;
float vFold  = 1.0f;
float vFreqWF = 200.0f;
float vLevel = 1.0f;

// Phaser knob values
float vPhaserFreq = 800.0f;
float vPhaserFb   = 0.5f;

// Constant: minimum cutoff frequency (floor of the sweep)
static const float FREQ_FLOOR = 80.0f;

// Per-mode frequency floor for phaser (higher to avoid extreme low sweeps)
static const float PHASER_FREQ_FLOOR = 200.0f;

// Hardcoded wavefold envelope decay time (400ms — moderate decay with cbrt shaping)
static const float WAVEFOLD_DECAY_S = 0.4f;

// Fixed resonance for the wavefold post-filter (moderate color)
static const float WAVEFOLD_FILT_RES = 0.45f;

// Per-mode output gain normalization (tune these to match perceived loudness)
static const float MODE0_GAIN = 0.65f;  // Waveshape: tame the ±0.7 hard-clipped saturation
static const float MODE1_GAIN = 0.5f;   // Wavefold: two-pass folding can be very loud
static const float MODE2_GAIN = 1.4f;   // Phaser: dry+wet * 0.5 is quiet, boost to match

// Per-mode sensitivity scaling (applied to envScaled before clamping)
static const float MODE0_SENSE = 1.0f;  // Waveshape: baseline
static const float MODE1_SENSE = 1.0f;  // Wavefold: baseline
static const float MODE2_SENSE = 0.25f; // Phaser: tame — allpass sweep is perceptually dramatic

// Effect mode (via Switch 1)
int effectMode = 0; // 0 = Waveshape, 1 = Wavefold, 2 = Phaser


void updateSwitch1() // left=Waveshape, center=Wavefold, right=Phaser
{
    if (pswitch1[0] == true) {        // left
        effectMode = 0; // Waveshape
    } else if (pswitch1[1] == true) { // right
        effectMode = 2; // Phaser
    } else {                          // center
        effectMode = 1; // Wavefold
    }
}

void updateSwitch2() // reserved
{
    if (pswitch2[0] == true) {

    } else if (pswitch2[1] == true) {

    } else {

    }
}

void updateSwitch3() // reserved
{
    if (pswitch3[0] == true) {

    } else if (pswitch3[1] == true) {

    } else {

    }
}


void UpdateButtons()
{
    // Toggle bypass on footswitch 1 falling edge
    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {
        bypass = !bypass;
        led1.Set(bypass ? 0.0f : 1.0f);
    }

    led1.Update();
}


void UpdateSwitches()
{
    // 3-way Switch 1
    bool changed1 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch1[i]].Pressed() != pswitch1[i]) {
            pswitch1[i] = hw.switches[switch1[i]].Pressed();
            changed1 = true;
        }
    }
    if (changed1)
        updateSwitch1();

    // 3-way Switch 2
    bool changed2 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch2[i]].Pressed() != pswitch2[i]) {
            pswitch2[i] = hw.switches[switch2[i]].Pressed();
            changed2 = true;
        }
    }
    if (changed2)
        updateSwitch2();

    // 3-way Switch 3
    bool changed3 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch3[i]].Pressed() != pswitch3[i]) {
            pswitch3[i] = hw.switches[switch3[i]].Pressed();
            changed3 = true;
        }
    }
    if (changed3)
        updateSwitch3();

    // Dip switches
    for(int i=0; i<2; i++) {
        if (hw.switches[dip[i]].Pressed() != pdip[i]) {
            pdip[i] = hw.switches[dip[i]].Pressed();
        }
    }
}


static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    UpdateButtons();
    UpdateSwitches();

    // Read knobs
    vFreq  = pFreq.Process();
    vFreqWF = pFreqWF.Process();
    vRes   = pRes.Process();
    vFold  = pFold.Process();
    vSense = pSense.Process();
    vLevel = pLevel.Process();
    vPhaserFreq = pPhaserFreq.Process();
    vPhaserFb   = pPhaserFb.Process();

    // Read expression pedal and compute external envelope source
    float vexpression = pExpression.Process();
    if (vexpression > EXPRESSION_THRESHOLD) {
        // Expression pedal is connected and moved — use it, override MIDI
        externalEnv = vexpression;
        midiEnvActive = false;
    } else if (midiEnvActive) {
        // MIDI CC19 is controlling envelope
        externalEnv = midiEnvValue;
    } else {
        // No external source active
        externalEnv = 0.0f;
    }

    // Per-mode setup (constant across block)
    if (effectMode == 0) {
        filt.SetRes(vRes);
        filt.SetDrive(1.0f);
    } else if (effectMode == 1) {
        // Wavefold post-filter: fixed resonance, drive
        filtWF.SetRes(WAVEFOLD_FILT_RES);
        filtWF.SetDrive(1.0f);
    } else if (effectMode == 2) {
        // Phaser envelope release — fixed moderate decay (~80ms)
        envReleasePH = 1.0f - expf(-1.0f / (0.08f * srate));
    }

    for(size_t i = 0; i < size; i++)
    {
        if(bypass)
        {
            out[0][i] = in[0][i];
            out[1][i] = in[0][i];
        }
        else
        {
            float inL = in[0][i];

            // LED envelope follower (fast attack/release, independent of effect envelope)
            float inAbs = fabsf(inL);
            if (inAbs > envFollowerLED)
                envFollowerLED += envAttackLED * (inAbs - envFollowerLED);
            else
                envFollowerLED += envReleaseLED * (inAbs - envFollowerLED);

            float wet = 0.0f;

            if (effectMode == 0)
            {
                // --- Mode 0: Waveshape ---

                // Envelope follower (fixed attack/release)
                float inMono = fabsf(inL);
                if (inMono > envFollower)
                    envFollower += envAttack * (inMono - envFollower);
                else
                    envFollower += envRelease * (inMono - envFollower);

                // Scale envelope by sensitivity gain, clamp to 0-1
                float envScaled = envFollower * vSense * MODE0_SENSE;
                if (envScaled > 1.0f) envScaled = 1.0f;

                // External envelope override (expression pedal or MIDI CC19)
                if (externalEnv > EXPRESSION_THRESHOLD)
                    envScaled = externalEnv;

                // Compute dynamic cutoff: floor + scaled envelope * (knob ceiling - floor)
                float cutoff = FREQ_FLOOR + envScaled * (vFreq - FREQ_FLOOR);
                if (cutoff < 20.0f)  cutoff = 20.0f;
                if (cutoff > 16000.0f) cutoff = 16000.0f;
                filt.SetFreq(cutoff);

                // Triple-cascaded tanh saturation
                float sat = tanhf(15.0f * inL);
                sat = tanhf(4.0f * sat);
                sat = tanhf(2.0f * sat);
                // Hard clip to flatten peaks
                if (sat >  0.7f) sat =  0.7f;
                if (sat < -0.7f) sat = -0.7f;

                // Process through resonant lowpass
                filt.Process(sat);
                wet = filt.Low() * MODE0_GAIN;
            }
            else if (effectMode == 1)
            {
                // --- Mode 1: Envelope-controlled Wavefold ---

                // Envelope follower with fixed long decay (500ms)
                float inMono = fabsf(inL);
                if (inMono > envFollowerWF)
                    envFollowerWF += envAttack * (inMono - envFollowerWF);
                else
                    envFollowerWF += envReleaseWF * (inMono - envFollowerWF);

                // Scale envelope by sensitivity, clamp to 0-1
                float envScaled = envFollowerWF * vSense * MODE1_SENSE;
                if (envScaled > 1.0f) envScaled = 1.0f;

                // Cube-root shaping: envelope lingers near peak longer, keeping fold
                // engaged during note sustain before dropping off at the tail
                envScaled = cbrtf(envScaled);

                // External envelope override (expression pedal or MIDI CC19)
                if (externalEnv > EXPRESSION_THRESHOLD)
                    envScaled = externalEnv;

                // Dynamic fold gain: at rest ~1 (no fold), at peak envelope = vFold
                float dynamicGain = 1.0f + envScaled * (vFold - 1.0f);

                // Two-pass wavefolding
                wfold.SetOffset(-0.5f);
                wfold.SetGain(dynamicGain);
                float folded = wfold.Process(inL);
                // Second pass at half gain for extra harmonic complexity
                wfold.SetGain(dynamicGain * 0.5f);
                folded = wfold.Process(folded);

                // Soft-clip to tame loud folded output
                folded = tanhf(folded);

                // Envelope-driven post-fold lowpass filter (same pattern as mode 0)
                float cutoffWF = FREQ_FLOOR + envScaled * (vFreqWF - FREQ_FLOOR);
                if (cutoffWF < 20.0f)    cutoffWF = 20.0f;
                if (cutoffWF > 16000.0f) cutoffWF = 16000.0f;
                filtWF.SetFreq(cutoffWF);

                filtWF.Process(folded);
                wet = filtWF.Low() * MODE1_GAIN;
            }
            else if (effectMode == 2)
            {
                // --- Mode 2: Envelope-controlled 8-pole Phaser ---

                // Envelope follower (fixed attack, moderate release)
                float inMono = fabsf(inL);
                if (inMono > envFollowerPH)
                    envFollowerPH += envAttack * (inMono - envFollowerPH);
                else
                    envFollowerPH += envReleasePH * (inMono - envFollowerPH);

                // Scale envelope by sensitivity, clamp to 0-1
                float envScaled = envFollowerPH * vSense * MODE2_SENSE;
                if (envScaled > 1.0f) envScaled = 1.0f;

                // External envelope override (expression pedal or MIDI CC19)
                if (externalEnv > EXPRESSION_THRESHOLD)
                    envScaled = externalEnv;

                // Dynamic base allpass frequency: floor + envelope * (knob ceiling - floor)
                float baseFreq = PHASER_FREQ_FLOOR + envScaled * (vPhaserFreq - PHASER_FREQ_FLOOR);
                if (baseFreq < 20.0f)    baseFreq = 20.0f;

                // Mild saturation for consistency with modes 0/1
                float sat = tanhf(3.0f * inL);

                // Mix in feedback from previous output
                float sig = sat + phaserFeedbackSample * vPhaserFb;

                // Cascade through 4 second-order (biquad) allpass stages
                float twoPiOverSr = 2.0f * 3.14159265f / srate;
                for (int s = 0; s < PHASER_STAGES; s++)
                {
                    float sf = baseFreq * stagger[s];
                    if (sf > 20000.0f) sf = 20000.0f;

                    // Biquad allpass coefficients (Audio EQ Cookbook)
                    float w0 = twoPiOverSr * sf;
                    float sinW = sinf(w0);
                    float cosW = cosf(w0);
                    float alpha = sinW / (2.0f * PHASER_Q);

                    // Normalized coefficients: b0=a2=(1-alpha)/(1+alpha), b1=a1=-2cos(w0)/(1+alpha)
                    float a0_inv = 1.0f / (1.0f + alpha);
                    float c1 = -2.0f * cosW * a0_inv;
                    float c2 = (1.0f - alpha) * a0_inv;

                    // Direct Form II Transposed
                    float x = sig;
                    sig = c2 * x + apStages[s].s1;
                    apStages[s].s1 = c1 * x - c1 * sig + apStages[s].s2;
                    apStages[s].s2 = x - c2 * sig;
                }

                // Store feedback sample (before dry/wet mix)
                phaserFeedbackSample = sig;

                // Mix dry + wet (notch-style phaser)
                wet = (sat + sig) * 0.5f * MODE2_GAIN;
            }

            // Output level (100% wet)
            float outSample = wet * vLevel;

            out[0][i] = outSample;
            out[1][i] = outSample;
        }
    }

    // Drive LED2 from the active envelope (brightness tracks playing dynamics)
    if (!bypass) {
        float envLed = 0.0f;
        if (externalEnv > EXPRESSION_THRESHOLD) {
            // When external envelope is active, LED tracks the expression/MIDI value
            envLed = externalEnv;
        } else if (effectMode == 0)
            envLed = envFollowerLED * vSense * MODE0_SENSE;
        else if (effectMode == 1)
            envLed = envFollowerLED * vSense * MODE1_SENSE;
        else if (effectMode == 2)
            envLed = envFollowerPH * vSense * MODE2_SENSE * 2.0f;
        if (envLed > 1.0f) envLed = 1.0f;
        if (envLed < 0.01f) envLed = 0.0f;  // Gate: snap off to avoid dim tail
        led2.Set(envLed);
    } else {
        led2.Set(0.0f);
    }
    led2.Update();
}


// MIDI message handler — CC19 controls envelope directly
void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case ControlChange:
        {
            ControlChangeEvent p = m.AsControlChange();
            switch(p.control_number)
            {
                case 19:
                    midiEnvActive = true;
                    midiEnvValue = (float)p.value / 127.0f;
                    break;
                default: break;
            }
            break;
        }
        default: break;
    }
}


int main(void)
{
    hw.Init();
    srate = hw.AudioSampleRate();

    hw.SetAudioBlockSize(2);

    // Map hardware switches
    switch1[0] = Funbox::SWITCH_1_LEFT;
    switch1[1] = Funbox::SWITCH_1_RIGHT;
    switch2[0] = Funbox::SWITCH_2_LEFT;
    switch2[1] = Funbox::SWITCH_2_RIGHT;
    switch3[0] = Funbox::SWITCH_3_LEFT;
    switch3[1] = Funbox::SWITCH_3_RIGHT;
    dip[0] = Funbox::SWITCH_DIP_1;
    dip[1] = Funbox::SWITCH_DIP_2;
    dip[2] = Funbox::SWITCH_DIP_3;
    dip[3] = Funbox::SWITCH_DIP_4;

    pswitch1[0] = false;
    pswitch1[1] = false;
    pswitch2[0] = false;
    pswitch2[1] = false;
    pswitch3[0] = false;
    pswitch3[1] = false;
    pdip[0] = false;
    pdip[1] = false;
    pdip[2] = false;
    pdip[3] = false;

    // Knob 1: FREQ (mode 0) / FREQ (mode 1) / FREQ (mode 2) — all frequency ceilings
    pFreq.Init(hw.knob[Funbox::KNOB_1], 100.0f, 8000.0f, Parameter::LOGARITHMIC);
    pFreqWF.Init(hw.knob[Funbox::KNOB_1], 100.0f, 8000.0f, Parameter::LOGARITHMIC);
    pPhaserFreq.Init(hw.knob[Funbox::KNOB_1], 40.0f, 5000.0f, Parameter::LOGARITHMIC);
    // Knob 2: RES (mode 0) / FOLD (mode 1) — dual-mapped
    pRes.Init(hw.knob[Funbox::KNOB_2], 0.0f, 0.99f, Parameter::LINEAR);
    pFold.Init(hw.knob[Funbox::KNOB_2], 1.0f, 12.0f, Parameter::EXPONENTIAL);
    pPhaserFb.Init(hw.knob[Funbox::KNOB_2], 0.0f, 0.95f, Parameter::LINEAR);
    // Knob 3: SENSE — envelope sensitivity (shared, logarithmic 1-20x)
    pSense.Init(hw.knob[Funbox::KNOB_3], 1.0f, 20.0f, Parameter::LOGARITHMIC);
    // Knob 4: reserved
    param4.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::LINEAR);
    // Knob 5: reserved
    param5.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    // Knob 6: LEVEL (shared)
    pLevel.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR);

    // Expression pedal input (v3 board, 0-1 linear)
    pExpression.Init(hw.expression, 0.0f, 1.0f, Parameter::LINEAR);

    // Init filter
    filt.Init(srate);
    filt.SetFreq(1000.0f);
    filt.SetRes(0.5f);

    // Init wavefold post-filter
    filtWF.Init(srate);
    filtWF.SetFreq(1000.0f);
    filtWF.SetRes(WAVEFOLD_FILT_RES);

    // Init wavefolder
    wfold.Init();

    // Init manual phaser biquad allpass state
    for (int s = 0; s < PHASER_STAGES; s++) {
        apStages[s].s1 = 0.0f;
        apStages[s].s2 = 0.0f;
    }
    phaserFeedbackSample = 0.0f;

    // Envelope follower coefficients (mode 0 uses fixed attack/release)
    envAttack  = 1.0f - expf(-1.0f / (0.005f * srate));
    envRelease = 1.0f - expf(-1.0f / (0.050f * srate));
    // Mode 1 release — hardcoded long decay (500ms)
    envReleaseWF = 1.0f - expf(-1.0f / (WAVEFOLD_DECAY_S * srate));
    // Mode 2 phaser release — init to moderate 80ms decay
    envReleasePH = 1.0f - expf(-1.0f / (0.08f * srate));

    // LED envelope follower — fast attack/release so LED drops quickly on mute
    envAttackLED  = 1.0f - expf(-1.0f / (0.005f * srate));
    envReleaseLED = 1.0f - expf(-1.0f / (0.08f * srate));

    // Init LEDs and start in bypass
    led1.Init(hw.seed.GetPin(Funbox::LED_1), false);
    led1.Update();
    bypass = true;

    led2.Init(hw.seed.GetPin(Funbox::LED_2), false);
    led2.Update();

    // Init MIDI
    hw.InitMidi();
    hw.midi.StartReceive();

    hw.StartAdc();

    // Read initial hardware state so we start in the correct mode/knob positions
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();
    for(int i=0; i<2; i++) {
        pswitch1[i] = hw.switches[switch1[i]].Pressed();
        pswitch2[i] = hw.switches[switch2[i]].Pressed();
        pswitch3[i] = hw.switches[switch3[i]].Pressed();
    }
    updateSwitch1();
    updateSwitch2();
    updateSwitch3();

    vFreq  = pFreq.Process();
    vFreqWF = pFreqWF.Process();
    vRes   = pRes.Process();
    vFold  = pFold.Process();
    vSense = pSense.Process();
    vLevel = pLevel.Process();
    vPhaserFreq = pPhaserFreq.Process();
    vPhaserFb   = pPhaserFb.Process();

    hw.StartAudio(AudioCallback);
    while(1)
    {
        hw.midi.Listen();
        // Handle MIDI Events
        while(hw.midi.HasEvents())
        {
            HandleMidiMessage(hw.midi.PopEvent());
        }
    }
}

