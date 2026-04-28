// Ceres - Envelope Filter / Waveshape / Wavefold
//
// A synthy distortion with a resonant filter controlled by input amplitude.
// Mode 0 (left):   Waveshape - tanh saturation into an envelope-following resonant SVF.
// Mode 1 (center): Wavefold  - envelope-controlled wavefolding with adjustable decay.
//
// Knob mappings:
//   Knob 1:          Mode 0 (FREQ): Filter cutoff ceiling (100 - 8000 Hz, log)
//                    Mode 1 (DECAY): Envelope decay/release time (20ms - 1000ms)
//   Knob 2:          Mode 0 (RES): Resonance / feedback of the filter (0 - 1)
//                    Mode 1 (FOLD): Max fold gain — envelope-driven fold depth (1x - 12x, exp)
//   Knob 3 (SENSE):  Envelope sensitivity gain (1x - 50x, exponential) — both modes
//   Knob 6 (LEVEL):  Output level (0 - 1)
//
// Switch 1:       Mode select (left=Waveshape, center=Wavefold, right=reserved)
// Footswitch 1:   Bypass
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
// Mode 1 re-interprets knob 1 as DECAY, knob 2 as FOLD
Parameter pDecay, pFold;

bool            bypass;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];

Led led1, led2;

// DSP state
Svf filt;
Wavefolder wfold;

// Envelope follower state (mode 0)
float envFollower = 0.0f;

// Envelope follower coefficients (mode 0, fixed)
float envAttack  = 0.0f;
float envRelease = 0.0f;

// Envelope follower state (mode 1, with adjustable decay)
float envFollowerWF = 0.0f;
float envReleaseWF  = 0.0f;

// Sample rate (stored for per-block decay recalculation)
float srate = 48000.0f;

// Knob values
float vFreq  = 200.0f;
float vRes   = 0.5f;
float vSense = 0.5f;
float vFold  = 1.0f;
float vDecay = 0.05f;
float vLevel = 1.0f;

// Constant: minimum cutoff frequency (floor of the sweep)
static const float FREQ_FLOOR = 80.0f;

// Effect mode (via Switch 1)
int effectMode = 0; // 0 = Waveshape, 1 = Wavefold


void updateSwitch1() // left=Waveshape, center=Wavefold, right=reserved
{
    if (pswitch1[0] == true) {        // left
        effectMode = 0; // Waveshape
    } else if (pswitch1[1] == true) { // right
        effectMode = 2; // reserved
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
    vDecay = pDecay.Process();
    vRes   = pRes.Process();
    vFold  = pFold.Process();
    vSense = pSense.Process();
    vLevel = pLevel.Process();

    // Per-mode setup (constant across block)
    if (effectMode == 0) {
        filt.SetRes(vRes);
        filt.SetDrive(1.0f);
    } else if (effectMode == 1) {
        // Recompute wavefold envelope release coefficient from DECAY knob
        envReleaseWF = 1.0f - expf(-1.0f / (vDecay * srate));
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
                float envScaled = envFollower * vSense;
                if (envScaled > 1.0f) envScaled = 1.0f;

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
                wet = filt.Low();
            }
            else if (effectMode == 1)
            {
                // --- Mode 1: Envelope-controlled Wavefold ---

                // Envelope follower with adjustable decay/release
                float inMono = fabsf(inL);
                if (inMono > envFollowerWF)
                    envFollowerWF += envAttack * (inMono - envFollowerWF);
                else
                    envFollowerWF += envReleaseWF * (inMono - envFollowerWF);

                // Scale envelope by sensitivity, clamp to 0-1
                float envScaled = envFollowerWF * vSense;
                if (envScaled > 1.0f) envScaled = 1.0f;

                // Dynamic fold gain: at rest ~1 (no fold), at peak envelope = vFold
                float dynamicGain = 1.0f + envScaled * (vFold - 1.0f);

                // Two-pass wavefolding
                wfold.SetOffset(-0.5f);
                wfold.SetGain(dynamicGain);
                float folded = wfold.Process(inL);
                // Second pass at half gain for extra harmonic complexity
                wfold.SetGain(dynamicGain * 0.5f);
                folded = wfold.Process(folded);

                wet = folded;
            }

            // Output level (100% wet)
            float outSample = wet * vLevel;

            out[0][i] = outSample;
            out[1][i] = outSample;
        }
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

    // Knob 1: FREQ (mode 0) / DECAY (mode 1) — dual-mapped
    pFreq.Init(hw.knob[Funbox::KNOB_1], 100.0f, 8000.0f, Parameter::LOGARITHMIC);
    pDecay.Init(hw.knob[Funbox::KNOB_1], 0.02f, 1.0f, Parameter::LOGARITHMIC);  // 20ms - 1000ms
    // Knob 2: RES (mode 0) / FOLD (mode 1) — dual-mapped
    pRes.Init(hw.knob[Funbox::KNOB_2], 0.0f, 0.99f, Parameter::LINEAR);
    pFold.Init(hw.knob[Funbox::KNOB_2], 1.0f, 12.0f, Parameter::EXPONENTIAL);
    // Knob 3: SENSE — envelope sensitivity (shared, exponential 1-50x)
    pSense.Init(hw.knob[Funbox::KNOB_3], 1.0f, 50.0f, Parameter::EXPONENTIAL);
    // Knob 4: reserved
    param4.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::LINEAR);
    // Knob 5: reserved
    param5.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    // Knob 6: LEVEL (shared)
    pLevel.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR);

    // Init filter
    filt.Init(srate);
    filt.SetFreq(1000.0f);
    filt.SetRes(0.5f);

    // Init wavefolder
    wfold.Init();

    // Envelope follower coefficients (mode 0 uses fixed attack/release)
    envAttack  = 1.0f - expf(-1.0f / (0.005f * srate));
    envRelease = 1.0f - expf(-1.0f / (0.050f * srate));
    // Mode 1 release is set per-block from DECAY knob; init to same as mode 0
    envReleaseWF = envRelease;

    // Init LEDs and start in bypass
    led1.Init(hw.seed.GetPin(Funbox::LED_1), false);
    led1.Update();
    bypass = true;

    led2.Init(hw.seed.GetPin(Funbox::LED_2), false);
    led2.Update();

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
    vDecay = pDecay.Process();
    vRes   = pRes.Process();
    vFold  = pFold.Process();
    vSense = pSense.Process();
    vLevel = pLevel.Process();

    hw.StartAudio(AudioCallback);
    while(1)
    {
        System::Delay(10);
    }
}

