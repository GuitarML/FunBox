// Ceres - Envelope Filter / Waveshape / Wavefold / Phaser
//
// A synthy distortion with a resonant filter controlled by input amplitude.
// Mode 0 (left):   Waveshape - tanh saturation into an envelope-following resonant SVF.
// Mode 1 (center): Wavefold  - envelope-controlled wavefolding into an envelope-following lowpass.
// Mode 2 (right):  Phaser    - envelope-controlled 8-pole phaser.
//
// Knob mappings:
//   Knob 1 (LEVEL):  Output level (0 - 1) — all modes
//   Knob 2 (FREQ):   Mode 0: Filter cutoff ceiling (100 - 8000 Hz, log)
//                     Mode 1: Post-fold filter cutoff ceiling (100 - 8000 Hz, log)
//                     Mode 2: Phaser allpass frequency ceiling (40 - 5000 Hz, log)
//   Knob 3:          Mode 0 (RES): Resonance / feedback of the filter (0 - 1)
//                    Mode 1 (FOLD): Max fold gain — envelope-driven fold depth (1x - 12x, exp)
//                    Mode 2 (FDBK): Phaser feedback (0 - 0.75)
//   Knob 4 (SENSE):  Envelope sensitivity gain (1x - 20x, logarithmic) — all modes
//
// Switch 1:       Mode select (left=Waveshape, center=Wavefold, right=Phaser)
// Footswitch 1:   Short press = Bypass toggle; Hold 3s = Enter/exit program mode
//
// MIDI PC (Ch 1):  In program mode: saves current settings to that program slot.
//                  Outside program mode: recalls preset from that program slot.
// Expression pedal (v3 board): Controls envelope directly (only active when SENS knob is at minimum)
// MIDI CC19:                   Controls envelope directly (only active when SENS knob is at minimum)
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
float externalEnv    = 0.0f;
bool  midiEnvActive  = false;
float midiEnvValue   = 0.0f;

static const float EXPRESSION_THRESHOLD = 0.05f;

// SENS knob must be at minimum (≤ this value) for expression/MIDI CC19 to control envelope
static const float SENS_MIN_THRESHOLD = 1.05f;

// ============================================================
// MIDI Preset System — 128 presets stored in QSPI flash
// ============================================================
static const uint8_t PRESET_MAGIC = 0xA5;  // Marks a preset slot as initialized

struct Preset {
    uint8_t magic;       // PRESET_MAGIC if this slot has been written
    bool    bypass;
    int     effectMode;
    float   knobs[6];    // Raw 0-1 knob values (pre-parameter-mapping)
};

struct PresetBank {
    Preset presets[128];

    bool operator!=(const PresetBank& a) const {
        for (int i = 0; i < 128; i++) {
            if (a.presets[i].magic != presets[i].magic) return true;
            if (a.presets[i].bypass != presets[i].bypass) return true;
            if (a.presets[i].effectMode != presets[i].effectMode) return true;
            for (int k = 0; k < 6; k++) {
                if (a.presets[i].knobs[k] != presets[i].knobs[k]) return true;
            }
        }
        return false;
    }
};

PersistentStorage<PresetBank> SavedPresets(hw.seed.qspi);

// Program mode state
bool     programMode      = false;
bool     longHoldTriggered = false; // Prevents bypass toggle on release after long hold
bool     triggerSave       = false;
bool     confirmBlinking   = false;
int      confirmBlinkCount = 0;
uint32_t confirmBlinkTimer = 0;
uint32_t blinkTimer        = 0;
bool     led2BlinkState    = false;

// Preset recall state
bool     usePreset         = false;
float    presetKnobs[6]    = {};    // Shadow knob values from recalled preset
int      presetMode        = 0;     // Mode from recalled preset
bool     presetModeActive  = false; // True when preset overrides the physical switch mode

// Knob-moved threshold for exiting preset override
static const float KNOB_MOVE_THRESHOLD = 0.05f;

// ============================================================

// DSP state
Svf filt;
Svf filtWF;
Wavefolder wfold;
Compressor phaserComp;

static const int PHASER_STAGES = 4;
struct BiquadAP {
    float s1, s2;
};
BiquadAP apStages[PHASER_STAGES] = {};
float phaserFeedbackSample = 0.0f;

static const float stagger[PHASER_STAGES] = {
    1.0f, 2.2f, 5.0f, 11.0f
};

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


// LED2 target brightness (computed in audio callback, applied in main loop)
float led1Target = 0.0f;

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
    // Physical switch moved — clear preset mode override
    if (presetModeActive) {
        presetModeActive = false;
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
    // Detect 3-second hold on footswitch 1 to enter/exit program mode
    if (hw.switches[Funbox::FOOTSWITCH_1].Pressed()) {
        if (hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 3000 && !longHoldTriggered) {
            longHoldTriggered = true;
            programMode = !programMode;
            if (programMode) {
                // Entering program mode — start LED1 blink
                blinkTimer = System::GetNow();
                led2BlinkState = true;
            }
            // If exiting program mode via hold, no save — just cancel
        }
    }

    // On release: short press = bypass toggle, long hold = already handled above
    if (hw.switches[Funbox::FOOTSWITCH_1].FallingEdge()) {
        if (longHoldTriggered) {
            // Release after long hold — suppress bypass toggle
            longHoldTriggered = false;
        } else {
            // Short press — toggle bypass (works in both normal and program mode)
            bypass = !bypass;
            led2.Set(bypass ? 0.0f : 1.0f);
        }
    }

    led2.Update();
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


// Helper: check if a knob has moved significantly from its preset shadow value
bool knobMoved(int knobIdx) {
    static const int knobMap[6] = {
        Funbox::KNOB_1, Funbox::KNOB_2, Funbox::KNOB_3,
        Funbox::KNOB_4, Funbox::KNOB_5, Funbox::KNOB_6
    };
    float current = hw.knob[knobMap[knobIdx]].Value();
    float diff = current - presetKnobs[knobIdx];
    if (diff < 0.0f) diff = -diff;
    return diff > KNOB_MOVE_THRESHOLD;
}


static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    UpdateButtons();
    UpdateSwitches();

    // If using a preset, check if any knob has moved to exit preset mode
    if (usePreset) {
        for (int k = 0; k < 6; k++) {
            if (knobMoved(k)) {
                usePreset = false;
                presetModeActive = false;
                break;
            }
        }
    }

    // Apply preset mode override (preset's effectMode overrides physical switch)
    if (presetModeActive) {
        effectMode = presetMode;
    }

    // Read knobs — use preset values if active, otherwise live hardware
    if (usePreset) {
        // Map raw 0-1 preset knob values through the same ranges as the Parameter objects
        // Knob 1: LEVEL — linear 0-1
        vLevel    = presetKnobs[0];
        // Knob 2: FREQ — log 100-8000 (mode 0/1), log 40-5000 (mode 2)
        float k2 = presetKnobs[1];
        vFreq     = 100.0f * powf(8000.0f / 100.0f, k2);
        vFreqWF   = 100.0f * powf(8000.0f / 100.0f, k2);
        vPhaserFreq = 40.0f * powf(5000.0f / 40.0f, k2);
        // Knob 3: RES linear 0-0.99, FOLD exp 1-12, FDBK linear 0-0.95
        float k3 = presetKnobs[2];
        vRes      = k3 * 0.99f;
        vFold     = 1.0f + (12.0f - 1.0f) * k3 * k3; // exponential approximation
        vPhaserFb = k3 * 0.95f;
        // Knob 4: SENSE — log 1-20
        float k4 = presetKnobs[3];
        vSense    = 1.0f * powf(20.0f / 1.0f, k4);
    } else {
        vFreq  = pFreq.Process();
        vFreqWF = pFreqWF.Process();
        vRes   = pRes.Process();
        vFold  = pFold.Process();
        vSense = pSense.Process();
        vLevel = pLevel.Process();
        vPhaserFreq = pPhaserFreq.Process();
        vPhaserFb   = pPhaserFb.Process();
    }

    // Read expression pedal and compute external envelope source
    // Expression/MIDI CC19 only active when SENS knob is at minimum position
    bool expressionEnabled = (vSense <= SENS_MIN_THRESHOLD);
    float vexpression = pExpression.Process();
    if (expressionEnabled) {
        if (vexpression > EXPRESSION_THRESHOLD) {
            externalEnv = vexpression;
            midiEnvActive = false;
        } else if (midiEnvActive) {
            externalEnv = midiEnvValue;
        } else {
            externalEnv = 0.0f;
        }
    } else {
        externalEnv = 0.0f;
    }

    // Per-mode setup (constant across block)
    if (effectMode == 0) {
        filt.SetRes(vRes);
        filt.SetDrive(1.0f);
    } else if (effectMode == 1) {
        filtWF.SetRes(WAVEFOLD_FILT_RES);
        filtWF.SetDrive(1.0f);
    } else if (effectMode == 2) {
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
                float inMono = fabsf(inL);
                if (inMono > envFollower)
                    envFollower += envAttack * (inMono - envFollower);
                else
                    envFollower += envRelease * (inMono - envFollower);

                float envScaled = envFollower * vSense * MODE0_SENSE;
                if (envScaled > 1.0f) envScaled = 1.0f;

                if (externalEnv > EXPRESSION_THRESHOLD)
                    envScaled = externalEnv;

                float cutoff = FREQ_FLOOR + envScaled * (vFreq - FREQ_FLOOR);
                if (cutoff < 20.0f)  cutoff = 20.0f;
                if (cutoff > 16000.0f) cutoff = 16000.0f;
                filt.SetFreq(cutoff);

                float sat = tanhf(15.0f * inL);
                sat = tanhf(4.0f * sat);
                sat = tanhf(2.0f * sat);
                if (sat >  0.7f) sat =  0.7f;
                if (sat < -0.7f) sat = -0.7f;

                filt.Process(sat);
                wet = filt.Low() * MODE0_GAIN;
            }
            else if (effectMode == 1)
            {
                // --- Mode 1: Envelope-controlled Wavefold ---
                float inMono = fabsf(inL);
                if (inMono > envFollowerWF)
                    envFollowerWF += envAttack * (inMono - envFollowerWF);
                else
                    envFollowerWF += envReleaseWF * (inMono - envFollowerWF);

                float envScaled = envFollowerWF * vSense * MODE1_SENSE;
                if (envScaled > 1.0f) envScaled = 1.0f;

                envScaled = cbrtf(envScaled);

                if (externalEnv > EXPRESSION_THRESHOLD)
                    envScaled = externalEnv;

                float dynamicGain = 1.0f + envScaled * (vFold - 1.0f);

                wfold.SetOffset(-0.5f);
                wfold.SetGain(dynamicGain);
                float folded = wfold.Process(inL);
                wfold.SetGain(dynamicGain * 0.5f);
                folded = wfold.Process(folded);

                folded = tanhf(folded);

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
                float inMono = fabsf(inL);
                if (inMono > envFollowerPH)
                    envFollowerPH += envAttack * (inMono - envFollowerPH);
                else
                    envFollowerPH += envReleasePH * (inMono - envFollowerPH);


                float envScaled = envFollowerPH * vSense * MODE2_SENSE;
                if (envScaled > 1.0f) envScaled = 1.0f;

                if (externalEnv > EXPRESSION_THRESHOLD)
                    envScaled = externalEnv;

                float baseFreq = PHASER_FREQ_FLOOR + envScaled * (vPhaserFreq - PHASER_FREQ_FLOOR);
                if (baseFreq < 20.0f)    baseFreq = 20.0f;

                // Transparent compression — tames peaks without audible pumping
                float compressed = phaserComp.Process(inL);

                float sig = compressed + phaserFeedbackSample * vPhaserFb;

                float twoPiOverSr = 2.0f * 3.14159265f / srate;
                for (int s = 0; s < PHASER_STAGES; s++)
                {
                    float sf = baseFreq * stagger[s];
                    if (sf > 20000.0f) sf = 20000.0f;

                    float w0 = twoPiOverSr * sf;
                    float sinW = sinf(w0);
                    float cosW = cosf(w0);
                    float alpha = sinW / (2.0f * PHASER_Q);

                    float a0_inv = 1.0f / (1.0f + alpha);
                    float c1 = -2.0f * cosW * a0_inv;
                    float c2 = (1.0f - alpha) * a0_inv;

                    float x = sig;
                    sig = c2 * x + apStages[s].s1;
                    apStages[s].s1 = c1 * x - c1 * sig + apStages[s].s2;
                    apStages[s].s2 = x - c2 * sig;
                }

                phaserFeedbackSample = sig;
                // Safety clamp to prevent feedback runaway
                if (phaserFeedbackSample > 1.0f) phaserFeedbackSample = 1.0f;
                else if (phaserFeedbackSample < -1.0f) phaserFeedbackSample = -1.0f;

                wet = (compressed + sig) * 0.5f * MODE2_GAIN;
            }

            float outSample = wet * vLevel;

            out[0][i] = outSample;
            out[1][i] = outSample;
        }
    }

    // Compute LED1 target brightness (actual hardware update in main loop)
    if (!programMode && !confirmBlinking) {
        if (!bypass) {
            float envLed = 0.0f;
            if (externalEnv > EXPRESSION_THRESHOLD) {
                envLed = externalEnv;
            } else if (effectMode == 0)
                envLed = envFollowerLED * vSense * MODE0_SENSE;
            else if (effectMode == 1)
                envLed = envFollowerLED * vSense * MODE1_SENSE;
            else if (effectMode == 2)
                envLed = envFollowerPH * vSense * MODE2_SENSE * 2.0f;
            if (envLed > 1.0f) envLed = 1.0f;
            if (envLed < 0.01f) envLed = 0.0f;
            led1Target = envLed;
        } else {
            led1Target = 0.0f;
        }
    }
}


// MIDI message handler — CC19 controls envelope; PC handles presets
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
        case ProgramChange:
        {
            ProgramChangeEvent pc = m.AsProgramChange();
            uint8_t prog = pc.program;  // 0-127

            if (programMode) {
                // ---- SAVE current settings to this program slot ----
                PresetBank &bank = SavedPresets.GetSettings();
                Preset &slot = bank.presets[prog];

                slot.magic      = PRESET_MAGIC;
                slot.bypass     = bypass;
                slot.effectMode = effectMode;
                slot.knobs[0]   = hw.knob[Funbox::KNOB_1].Value();
                slot.knobs[1]   = hw.knob[Funbox::KNOB_2].Value();
                slot.knobs[2]   = hw.knob[Funbox::KNOB_3].Value();
                slot.knobs[3]   = hw.knob[Funbox::KNOB_4].Value();
                slot.knobs[4]   = hw.knob[Funbox::KNOB_5].Value();
                slot.knobs[5]   = hw.knob[Funbox::KNOB_6].Value();

                triggerSave = true;
                programMode = false;

                // Start confirm blink sequence (3 fast blinks at 100ms)
                confirmBlinking   = true;
                confirmBlinkCount = 0;
                confirmBlinkTimer = System::GetNow();
                led2BlinkState    = true;
            }
            else
            {
                // ---- RECALL preset from this program slot ----
                PresetBank &bank = SavedPresets.GetSettings();
                Preset &slot = bank.presets[prog];

                if (slot.magic != PRESET_MAGIC)
                    break;  // Uninitialized slot — ignore

                bypass      = slot.bypass;
                presetMode  = slot.effectMode;
                effectMode  = slot.effectMode;
                presetModeActive = true;

                for (int k = 0; k < 6; k++)
                    presetKnobs[k] = slot.knobs[k];

                usePreset = true;

                led2.Set(bypass ? 0.0f : 1.0f);
                led2.Update();
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

    // Knob 1: LEVEL
    pLevel.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    // Knob 2: FREQ
    pFreq.Init(hw.knob[Funbox::KNOB_2], 100.0f, 8000.0f, Parameter::LOGARITHMIC);
    pFreqWF.Init(hw.knob[Funbox::KNOB_2], 100.0f, 8000.0f, Parameter::LOGARITHMIC);
    pPhaserFreq.Init(hw.knob[Funbox::KNOB_2], 40.0f, 5000.0f, Parameter::LOGARITHMIC);
    // Knob 3: RES / FOLD / FDBK
    pRes.Init(hw.knob[Funbox::KNOB_3], 0.0f, 0.99f, Parameter::LINEAR);
    pFold.Init(hw.knob[Funbox::KNOB_3], 1.0f, 12.0f, Parameter::EXPONENTIAL);
    pPhaserFb.Init(hw.knob[Funbox::KNOB_3], 0.0f, 0.95f, Parameter::LINEAR);
    // Knob 4: SENSE
    pSense.Init(hw.knob[Funbox::KNOB_4], 1.0f, 20.0f, Parameter::LOGARITHMIC);
    // Knob 5: reserved
    param4.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    // Knob 6: reserved
    param5.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR);

    // Expression pedal input (v3 board)
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

    // Init phaser allpass state
    for (int s = 0; s < PHASER_STAGES; s++) {
        apStages[s].s1 = 0.0f;
        apStages[s].s2 = 0.0f;
    }
    phaserFeedbackSample = 0.0f;

    // Init phaser compressor — transparent peak control, no audible pumping
    phaserComp.Init(srate);
    phaserComp.SetThreshold(-12.0f);  // Only acts on loud peaks
    phaserComp.SetRatio(2.0f);        // Gentle reduction
    phaserComp.SetAttack(0.003f);     // Fast enough for transients, won't distort
    phaserComp.SetRelease(0.1f);      // Medium release avoids pumping
    phaserComp.AutoMakeup(true);      // Compensate for any level loss

    // Envelope follower coefficients
    envAttack  = 1.0f - expf(-1.0f / (0.005f * srate));
    envRelease = 1.0f - expf(-1.0f / (0.050f * srate));
    envReleaseWF = 1.0f - expf(-1.0f / (WAVEFOLD_DECAY_S * srate));
    envReleasePH = 1.0f - expf(-1.0f / (0.08f * srate));

    envAttackLED  = 1.0f - expf(-1.0f / (0.005f * srate));
    envReleaseLED = 1.0f - expf(-1.0f / (0.08f * srate));

    // Init LEDs and start in bypass
    led2.Init(hw.seed.GetPin(Funbox::LED_2), false);
    led2.SetSampleRate(srate);
    led2.Update();
    bypass = true;

    led1.Init(hw.seed.GetPin(Funbox::LED_1), false);
    led1.SetSampleRate(200.0f);  // LED1 updated at ~200 Hz from main loop
    led1.Update();

    // Initialize preset storage with empty defaults (all magic=0 = uninitialized)
    PresetBank defaultBank = {};
    SavedPresets.Init(defaultBank);

    // Init MIDI
    hw.InitMidi();
    hw.midi.StartReceive();

    hw.StartAdc();

    // Read initial hardware state
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

    uint32_t led1UpdateTimer = System::GetNow();

    while(1)
    {
        hw.midi.Listen();
        // Handle MIDI Events
        while(hw.midi.HasEvents())
        {
            HandleMidiMessage(hw.midi.PopEvent());
        }

        // Persist preset bank to QSPI flash after save
        if (triggerSave) {
            SavedPresets.Save();
            triggerSave = false;
        }

        // LED1 update at ~200 Hz (every 5ms) — decoupled from audio ISR to prevent PWM coupling
        uint32_t now = System::GetNow();
        if (now - led1UpdateTimer >= 5) {
            led1UpdateTimer = now;

            if (programMode && !confirmBlinking) {
                // Program mode blink (500ms toggle)
                if (now - blinkTimer >= 500) {
                    blinkTimer = now;
                    led2BlinkState = !led2BlinkState;
                }
                led1.Set(led2BlinkState ? 1.0f : 0.0f);
            } else if (confirmBlinking) {
                // Confirm blink after save (3 blinks at 100ms on/off)
                if (now - confirmBlinkTimer >= 100) {
                    confirmBlinkTimer = now;
                    led2BlinkState = !led2BlinkState;
                    if (!led2BlinkState) {
                        confirmBlinkCount++;
                        if (confirmBlinkCount >= 3) {
                            confirmBlinking = false;
                        }
                    }
                }
                led1.Set(led2BlinkState ? 1.0f : 0.0f);
            } else {
                // Normal envelope indicator (value computed in audio callback)
                led1.Set(led1Target);
            }
            led1.Update();
        }

    }
}
