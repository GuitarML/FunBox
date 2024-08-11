#include <string.h>
#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"
#include "expressionHandler.h"

#include <cmath>
#include <complex>
#include "shy_fft.h"

#include "fourier.h"
#include "wave.h"

#define PI 3.1415926535897932384626433832795
//#define SR 48000
//typedef float S; // sample type

//
// This is a template for creating a pedal on the GuitarML Funbox_v3/Daisy Seed platform.
// You can start from here to fill out your effects processing and controls.
// Allows for Stereo In/Out, 6 knobs, 3 3-way switches, 4 dipswitches, 2 SPST Footswitches, 2 LEDs.
//
// Keith Bloemer 6/12/2024
//


using namespace daisy;
using namespace daisysp;
using namespace funbox;  // This is important for mapping the correct controls to the Daisy Seed on Funbox PCB
using namespace soundmath;

// Declare a local daisy_petal for hardware access
DaisyPetal hw;
Parameter mod, mix, predelay, delay_time, delay_fdbk, filter, expression;

float samplerate = 48000; // making global

bool            bypass;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];


Led led1, led2;


// Expression
ExpressionHandler expHandler;
bool expression_pressed;

// Midi
bool midi_control[6]; //  just knobs for now

// Control Values
float knobValues[6];
int toggleValues[3];
bool dipValues[4];

float pknobValues[6]; // Used for Midi control logic

// convenient lookup tables
Wave<float> hann([] (float phase) -> float { return 0.5 * (1 - cos(2 * PI * phase)); });
Wave<float> halfhann([] (float phase) -> float { return sin(PI * phase); });

// 4 overlapping windows of size 2^12 = 4096
// `N = 4096` and `laps = 4` (higher frequency resolution, greater latency), or when `N = 2048` and `laps = 8` (higher time resolution, less latency). 
// For Saturn, I'm using N=1024, N=4

const size_t order = 10;      // 1024 
const size_t N = (1 << order);
const float sqrtN = sqrt(N);
const size_t laps = 4;
const size_t buffsize = 2 * laps * N; 

// convenient constant for grabbing imaginary parts
static const size_t offset = N / 2; // equals 512

// DEVELOPMENT NOTES
// array 256, N=2048, laps=8, SR 48000, block size=256    keeps up fine
// array 512, N=2048, laps=8, SR 48000, block size=256    occasional dropouts
// array 512, N=2048, laps=4, SR 48000, block size=256    no dropouts, 4 laps might sound better?
// array 512, N=4096, laps=4, SR 48000, block size=256    no dropouts, lower freq because more bins, 
// array 512, N=1024, laps=4, SR 48000, block size=256    no dropouts, sounds just as good
// array 512, N=1024, laps=4, SR 48000, block size=48    constant dropouts, block size too low
// array 512, N=1024, laps=4, SR 48000, block size=96    constant dropouts, block size too low
// array 512, N=1024, laps=2, SR 48000, block size=96    starts to sound bad with only 2 laps
// array 512, N=512, laps=4, SR 48000, block size=256    still sounds good with N 512, need to compare more to 1024
// array 256, N=1024, laps=4, SR 48000, block size=256    no dropouts, sounds just as good

// array 256, N=1024, laps=4, SR 48000, block size=256 STEREO,   occasional dropouts otherwise sounds good!
// array 256, N=512, laps=4, SR 48000, block size=256 STEREO,   freezes led, constant dropout..why?? N is half 
// array 256, N=1024, laps=4, SR 32000, block size=256 STEREO,   no dropouts


// buffers for STFT processing
// audio --> in --(fft)--> middle --(process)--> out --(ifft)--> in -->
// each of these is a few circular buffers stacked end-to-end.
float in[buffsize]; // buffers for input and output (from / to user audio callback)
float middle[buffsize]; // buffers for unprocessed frequency domain data
float out[buffsize]; // buffers for processed frequency domain data 

float in2[buffsize]; // buffers for input and output (from / to user audio callback)
float middle2[buffsize]; // buffers for unprocessed frequency domain data
float out2[buffsize]; // buffers for processed frequency domain data 

// Using 2 FFT's for Stereo
ShyFFT<float, N, RotationPhasor>* fft; // fft object
Fourier<float, N>* stft; // stft object

ShyFFT<float, N, RotationPhasor>* fft2; // fft object
Fourier<float, N>* stft2; // stft object

float fft_size = N / 2;

float vmod, vmix, vpredelay, vdelay_time, vdelay_fdbk, vfilter; // making these global
float pdelay_time, pdelay_fdbk, ppredelay;
unsigned int filter_bin;
int filter_mode;

int delay_time_mode = 0;
int delay_fdbk_mode = 0;

bool switch_flipped = false;
bool first_start = true;

int drift_mode = 1;
Oscillator drift_osc;
float drift_multiplier = 1.0;

//float window_samples = 32768; // This is the buffsize
//float interval_samples = ceil(window_samples/laps);

// Delay
#define MAX_DELAY static_cast<size_t>(188 * 4.f ) // 4 second max delay (4 second spread plus 1 second predelay), delay called 188 times per second
#define MAX_PREDELAY static_cast<size_t>(96000.0f ) 
size_t delay_array_size = 175;  // CHANGE ME TO CHANGE DELAY ARRAY SIZE FOR THE REST OF THE PROGRAM, TODO there are lots of hard coded places (175/350), so would need to change those too

DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayLine_array_real[175];
DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayLine_array_imag[175];

DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayLine_array_real2[175];
DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayLine_array_imag2[175];

DelayLine<float, MAX_PREDELAY> DSY_SDRAM_BSS predelayLineLeft;
DelayLine<float, MAX_PREDELAY> DSY_SDRAM_BSS predelayLineRight;

bool mono_mode = false;

struct delay
{
    DelayLine<float, MAX_DELAY> *del;
    float                        currentDelay;
    float                        delayTarget;
    float                        feedback;
    float                        active = false;
    
    float Process(float in)
    {
        //set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        del->SetDelay(currentDelay);

        float read = del->Read();
        del->Write((feedback * read) + in);
        return read;
    }
};

struct delayPre
{
    DelayLine<float, MAX_PREDELAY> *del;
    float                        currentDelay;
    float                        delayTarget;
    float                        feedback;
    float                        active = false;
    
    float Process(float in)
    {
        fonepole(currentDelay, delayTarget, .0002f); 
        del->SetDelay(currentDelay);

        float read = del->Read();
        del->Write((feedback * read) + in);

        return read;
    }
};



struct delay delay_array_real[175];
struct delay delay_array_imag[175];

struct delay delay_array_real2[175];
struct delay delay_array_imag2[175];

float stereo_field[175];

delayPre predelay_left;
delayPre predelay_right;

//Setting Struct containing parameters we want to save to flash
// Using the persistent storage example found on the Daisy Forum:
//   https://forum.electro-smith.com/t/saving-values-to-flash-memory-using-persistentstorage-class-on-daisy-pod/4306
struct Settings {

        float knobs[6];
        int toggles[3];
        bool dips[4];

	//Overloading the != operator
	//This is necessary as this operator is used in the PersistentStorage source code
	bool operator!=(const Settings& a) const {
        return !(a.knobs[0]==knobs[0] && a.knobs[1]==knobs[1] && a.knobs[2]==knobs[2] && a.knobs[3]==knobs[3] && a.knobs[4]==knobs[4] && a.knobs[5]==knobs[5] && a.toggles[0]==toggles[0] && a.toggles[1]==toggles[1] && a.toggles[2]==toggles[2] && a.dips[0]==dips[0] && a.dips[1]==dips[1] && a.dips[2]==dips[2] && a.dips[3]==dips[3]);

    }
};


//Persistent Storage Declaration. Using type Settings and passed the devices qspi handle
PersistentStorage<Settings> SavedSettings(hw.seed.qspi);
bool use_preset = false;
bool trigger_save = false;
int blink = 100;
bool save_check = false;
bool update_switches = true;

void Load() {

    //Reference to local copy of settings stored in flash
    Settings &LocalSettings = SavedSettings.GetSettings();
	
    knobValues[0] = LocalSettings.knobs[0];
    knobValues[1] = LocalSettings.knobs[1];
    knobValues[2] = LocalSettings.knobs[2];
    knobValues[3] = LocalSettings.knobs[3];
    knobValues[4] = LocalSettings.knobs[4];
    knobValues[5] = LocalSettings.knobs[5];

    toggleValues[0] = LocalSettings.toggles[0];
    toggleValues[1] = LocalSettings.toggles[1];
    toggleValues[2] = LocalSettings.toggles[2];

    dipValues[0] = LocalSettings.dips[0];
    dipValues[1] = LocalSettings.dips[1];
    dipValues[2] = LocalSettings.dips[2];
    dipValues[3] = LocalSettings.dips[3];

    use_preset = true;

}

void Save() {
    //Reference to local copy of settings stored in flash
    Settings &LocalSettings = SavedSettings.GetSettings();

    LocalSettings.knobs[0] = knobValues[0];
    LocalSettings.knobs[1] = knobValues[1];
    LocalSettings.knobs[2] = knobValues[2];
    LocalSettings.knobs[3] = knobValues[3];
    LocalSettings.knobs[4] = knobValues[4];
    LocalSettings.knobs[5] = knobValues[5];

    LocalSettings.toggles[0] = toggleValues[0];
    LocalSettings.toggles[1] = toggleValues[1];
    LocalSettings.toggles[2] = toggleValues[2];

    LocalSettings.dips[0] = dipValues[0];
    LocalSettings.dips[1] = dipValues[1];
    LocalSettings.dips[2] = dipValues[2];
    LocalSettings.dips[3] = dipValues[3];

    trigger_save = true;
}



bool knobMoved(float old_value, float new_value)
{
    float tolerance = 0.005;
    if (new_value > (old_value + tolerance) || new_value < (old_value - tolerance)) {
        return true;
    } else {
        return false;
    }
}

void updateSwitch1() // left=, center=, right=
{

    if (toggleValues[0] == 0) {  
        delay_time_mode = 0;
    } else if (toggleValues[0] == 2) {  
        delay_time_mode = 2;

    } else if (toggleValues[0] == 1) {  
        delay_time_mode = 1;
    }     
    switch_flipped = true; 
}

void updateSwitch2() // left=, center=, right=
{
    if (toggleValues[1] == 0) {  // left
        delay_fdbk_mode = 0;
    } else if (toggleValues[1] == 2) {  // right
        delay_fdbk_mode = 2;

    } else if (toggleValues[1] == 1) {   // center
        delay_fdbk_mode = 1;
    }   
    switch_flipped = true;  
}


void updateSwitch3() // left=, center=, right=
{
    if (toggleValues[2] == 0) {  // left  drift without predelay
        drift_mode = 0; 

    } else if (toggleValues[2] == 2) {  // right drift including predelay
        drift_mode = 2;

    } else if (toggleValues[2] == 1) {   // center  no drift
        drift_mode = 1;
    }    
    switch_flipped = true;  
}


void UpdateButtons()
{

    // (De-)Activate bypass and toggle LED when left footswitch is let go
    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {
        if (!expression_pressed) { // This keeps the pedal from switching bypass when entering/leaving Set Expression mode
            bypass = !bypass;
            led1.Set(bypass ? 0.0f : 1.0f);

        }
        expression_pressed = false;
    }


    // Toggle Expression mode by holding down both footswitches for half a second
    if(hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 500 && hw.switches[Funbox::FOOTSWITCH_2].TimeHeldMs() >= 500 && !expression_pressed ) {
        expHandler.ToggleExpressionSetMode();

        if (expHandler.isExpressionSetMode()) {
            led1.Set(expHandler.returnLed1Brightness());  // Dim LEDs in expression set mode
            led2.Set(expHandler.returnLed2Brightness());  // Dim LEDs in expression set mode

        } else {
            led1.Set(bypass ? 0.0f : 1.0f); 
            led2.Set(0.0f);  
        }
        expression_pressed = true; // Keeps it from switching over and over while held

    }

    // Clear Expression settings by holding down both footswitches for 2 seconds
    if(hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 2000 && hw.switches[Funbox::FOOTSWITCH_2].TimeHeldMs() >= 2000) {
        expHandler.Reset();
        led1.Set(bypass ? 0.0f : 1.0f); 
        led2.Set(0.0f); 

    }

    // Save Preset  - Either raise the hold time for save check, or instruct user to hold left then right, let go right then left for Set Expression mode
    if(hw.switches[Funbox::FOOTSWITCH_2].TimeHeldMs() >= 700 && !save_check && !expression_pressed && hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() <= 50) 
    {
        Save();
        save_check = true;
    }

    // Load Preset
    if(hw.switches[Funbox::FOOTSWITCH_2].FallingEdge() && !expression_pressed)
    {
        if (save_check) {
            save_check = false;
        } else {
            use_preset = !use_preset;
            if (use_preset) {
                Load();
            } else {
                update_switches = true; // Need to update switches based on current switch position after turning off preset
            }
            led2.Set(use_preset ? 1.0f : 0.0f); 

        }
        // Need to update switches based on preset
        updateSwitch1();
        updateSwitch2();
        updateSwitch3();
    }

    // Handle blink for saving a preset
    if (blink < 100) {
        blink += 1;
        led2.Set(1.0f); 
    } else {
        if (!expHandler.isExpressionSetMode())
            led2.Set(use_preset ? 1.0f : 0.0f); 
    }

    led1.Update(); 
    led2.Update();

}


void UpdateSwitches()
{
    // Detect any changes in switch positions (3 On-Off-On switches and Dip switches)

    // 3-way Switch 1
    bool changed1 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch1[i]].Pressed() != pswitch1[i]) {
            pswitch1[i] = hw.switches[switch1[i]].Pressed();
            changed1 = true;
        }
    }
    if (changed1 || update_switches || first_start) { // update_switches is for turning off preset
        if (pswitch1[0] == true) {
            toggleValues[0] = 0;
        } else if (pswitch1[1] == true) {
            toggleValues[0] = 2;
        } else {
            toggleValues[0] = 1;
        }
        updateSwitch1();
    }
    

    // 3-way Switch 2
    bool changed2 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch2[i]].Pressed() != pswitch2[i]) {
            pswitch2[i] = hw.switches[switch2[i]].Pressed();
            changed2 = true;
        }
    }
    if (changed2 || update_switches || first_start) {
        if (pswitch2[0] == true) {
            toggleValues[1] = 0;
        } else if (pswitch2[1] == true) {
            toggleValues[1] = 2;
        } else {
            toggleValues[1] = 1;
        }
        updateSwitch2();

    }

    // 3-way Switch 3
    bool changed3 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch3[i]].Pressed() != pswitch3[i]) {
            pswitch3[i] = hw.switches[switch3[i]].Pressed();
            changed3 = true;
        }
    }
    if (changed3 || update_switches || first_start) {
        if (pswitch3[0] == true) {
            toggleValues[2] = 0;
        } else if (pswitch3[1] == true) {
            toggleValues[2] = 2;
        } else {
            toggleValues[2] = 1;
        }
        updateSwitch3();
    }

    // Dip switches
    bool changed4 = false;
    for(int i=0; i<4; i++) {
        if (hw.switches[dip[i]].Pressed() != pdip[i]) {
            pdip[i] = hw.switches[dip[i]].Pressed();
            changed4 = true;
            // Action for dipswitches handled in audio callback
        }
    }
    // Update if preset turned off
    if (changed4 || update_switches) {
        for (int i=0; i<4; i++) {
           dipValues[i] = pdip[i];
        }
    }

    update_switches = false; // only update once after turning off preset

    if (!dipValues[0]) { // If third dipswitch in off position, use mono mode, which makes use of the right hand delay lines to process double the frequency bins
        mono_mode = true;
        delay_array_size = 350;  // TODO Note, currently the time and fdbk settings in mono mode are duplicated for bins 1-175 vs. bins 176 to 350, maybe make unique for all 350 bins
    } else {
        mono_mode = false;
        delay_array_size = 175;
    }

}



// This runs at a fixed rate, to prepare audio samples
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    UpdateButtons();
    UpdateSwitches();



    // Knob and Expression Processing ////////////////////

    float newExpressionValues[6];

    if (!use_preset) {  // TODO Do I want to lock out the knobs when using a preset?

        // Knob 1
        if (!midi_control[0])   // If not under midi control, use knob ADC
            pknobValues[0] = knobValues[0] = predelay.Process();
        else if (knobMoved(pknobValues[0], predelay.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[0] = false;

        // Knob 2
        if (!midi_control[1])   // If not under midi control, use knob ADC
            pknobValues[1] = knobValues[1] = mix.Process();
        else if (knobMoved(pknobValues[1], mix.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[1] = false;

        // Knob 3
        if (!midi_control[2])   // If not under midi control, use knob ADC
            pknobValues[2] = knobValues[2] = filter.Process();
        else if (knobMoved(pknobValues[2], filter.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[2] = false;

        // Knob 4
        if (!midi_control[3])   // If not under midi control, use knob ADC
            pknobValues[3] = knobValues[3] = delay_time.Process();
        else if (knobMoved(pknobValues[3], delay_time.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[3] = false;
    

        // Knob 5
        if (!midi_control[4])   // If not under midi control, use knob ADC
            pknobValues[4] = knobValues[4] = delay_fdbk.Process();
        else if (knobMoved(pknobValues[4], delay_fdbk.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[4] = false;
    

        // Knob 6
        if (!midi_control[5])   // If not under midi control, use knob ADC
            pknobValues[5] = knobValues[5] = mod.Process();
        else if (knobMoved(pknobValues[5], mod.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[5] = false;

    }

    float vexpression = expression.Process(); // 0 is heel (up), 1 is toe (down)
    expHandler.Process(vexpression, knobValues, newExpressionValues);


    // If in expression set mode, set LEDS accordingly
    if (expHandler.isExpressionSetMode()) {
        led1.Set(expHandler.returnLed1Brightness());
        led2.Set(expHandler.returnLed2Brightness());
    }
  
    vpredelay = newExpressionValues[0];
    vmix = newExpressionValues[1];
    vfilter = newExpressionValues[2];
    vdelay_time = newExpressionValues[3];
    vdelay_fdbk = newExpressionValues[4];
    vmod = newExpressionValues[5];

    // Drift automation 
    if (drift_mode != 1) {
        if (drift_mode == 2) // if drift toggle right, apply drift to predelay as well
            vpredelay *= drift_multiplier;
        //vfilter *= drift_multiplier; // leaving filter out of drift control
        vdelay_time *= drift_multiplier;
        vdelay_fdbk *= (1.0 - drift_multiplier); // invert feedback drift
    } 


    if (vfilter < 0.45) {  // lowpass
        filter_mode = 0;
        filter_bin = floor(vfilter * 2.22 * (delay_array_size-5) + 4); 
    } else if (vfilter > 0.55) {  // highpass
        filter_mode = 1;
        filter_bin = floor((vfilter - 0.55) * 0.5 * delay_array_size);  
    } else {
        filter_mode = 2;
        filter_bin = 0;
    }

    drift_osc.SetFreq(vmod);
    predelay_left.delayTarget = predelay_right.delayTarget = 2400 + 93600.0 * vpredelay;

    // Dipswitch on to add feedback to pre-delay
    if (dipValues[3]) {
        predelay_left.feedback = 0.75;
        predelay_right.feedback = 0.75;
    } else {
        predelay_left.feedback = 0.0;
        predelay_right.feedback = 0.0;
    }

    float cycles = 4.0; // when time mode is center for sine wave, this changes the frequency of the sine wave across frequency bins

    if (knobMoved(vdelay_time, pdelay_time) || switch_flipped || first_start) {
        for(int i=0; i<175; i++) {
            if (delay_time_mode == 2) {
                float r = (static_cast <float> (rand()) / static_cast <float> (RAND_MAX));  
                //float r2 = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
                delay_array_real[i].delayTarget = delay_array_imag[i].delayTarget = delay_array_real2[i].delayTarget = delay_array_imag2[i].delayTarget =  r * 4 * 188 * vdelay_time;  // random delay time for each bin up to 4 seconds, mod 
              
            } else if (delay_time_mode == 1) {
                delay_array_real[i].delayTarget = delay_array_imag[i].delayTarget = delay_array_real2[i].delayTarget = delay_array_imag2[i].delayTarget =  (sin((i*cycles/175) * 2 * PI) +1.0) * vdelay_time *188*2;  // sin wave scaled from 0 up to 4 seconds
              
            } else {
                delay_array_real[i].delayTarget = delay_array_imag[i].delayTarget = delay_array_real2[i].delayTarget = delay_array_imag2[i].delayTarget =  vdelay_time * 4 * 188 * i / 175;  // linear delay time increase from low to high freq, 0 to 4 seconds, mod determines steepness of slope
                
            }
        }
        pdelay_time = vdelay_time;
    }


    if (knobMoved(vdelay_fdbk, pdelay_fdbk) || switch_flipped || first_start) {
        for(int i=0; i<175; i++) {
            if (delay_fdbk_mode == 2) {
                float r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
                //float r2 = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
                delay_array_real[i].feedback = delay_array_imag[i].feedback = r;
                delay_array_real2[i].feedback = delay_array_imag2[i].feedback = r;
            } else if (delay_fdbk_mode == 1) {
                delay_array_real[175 - 1- i].feedback = delay_array_imag[175 - 1- i].feedback = vdelay_fdbk * i / 175;
                delay_array_real2[175 - 1- i].feedback = delay_array_imag2[175 - 1- i].feedback = vdelay_fdbk * i / 175;
            } else {
                delay_array_real[i].feedback = delay_array_imag[i].feedback = vdelay_fdbk;
                delay_array_real2[i].feedback = delay_array_imag2[i].feedback = vdelay_fdbk;
            }
                
        }
        pdelay_fdbk = vdelay_fdbk;
    }

    switch_flipped = false;
    first_start = false;

    float inputL;
    float inputR;

    float delaygain = 3.0; // constant gain for delay

    for(size_t i = 0; i < size; i++)
    {
        drift_multiplier = drift_osc.Process() + 0.5; // process the oscillator for drift control

        if (dipValues[0] && dipValues[1]) {          // If Stereo Mode and not mono mode (Mono dipswitch overrides Stereo Dipswitch)
            inputL = in[0][i];
            inputR = in[1][i];
        } else {                // Else MISO mode or Mono mode
            inputL = in[0][i];
            inputR = in[0][i];
        } 

        // Process your signal here
        if(bypass)
        {
            out[0][i] = inputL; 
            out[1][i] = inputR;
        }
        else
        {   
            float preout_left = predelay_left.Process(inputL);
            float preout_right = predelay_right.Process(inputR);      

            if (!mono_mode) {
                stft->write(preout_left); // put a new sample in the STFT
                stft2->write(preout_right); // put a new sample in the STFT
                out[0][i] = stft->read() * vmix * delaygain + inputL * (1.0 - vmix); // read the next sample from the STFT
                out[1][i] = stft2->read() * vmix * delaygain + inputR * (1.0 - vmix); // read the next sample from the STFT
            } else {
                stft->write(preout_left); // put a new sample in the STFT
                out[0][i] = stft->read() * vmix * delaygain + inputL * (1.0 - vmix); // read the next sample from the STFT
                out[1][i] = out[0][i]; 
            }

        }
    }
}




// shy_fft packs arrays as [real, real, real, ..., imag, imag, imag, ...]
inline void spectraldelay(const float* in, float* out)
{
	for (size_t i = 0; i < N / 2; i++)  // loop i from 0 to 511
	{

            //float fft_bin = i + 1;

            float real = 0.0;
            float imag = 0.0;

            // each delayline affects 1 bin. 512 total bins. only using first 175 bins in more audible frequencies, more causes dropouts
            if (i < delay_array_size && ((filter_mode == 0 && i < filter_bin) || (filter_mode == 1 && i > filter_bin) || filter_mode == 2)) {

                real = in[i];
                imag = in[i + offset];

                if (!mono_mode) {
                    if (dipValues[2]) { //If Stereo Spread dipswitch is on
                        real = delay_array_real[i].Process(real) * (1.0 - stereo_field[i]);   // spectraldelay() called every N/laps audio samples, 1024/4 = 256, about 188 times per second
                        imag = delay_array_imag[i].Process(imag) * (1.0 - stereo_field[i]);
                    } else {  // else Stereo Spread is Off
                        real = delay_array_real[i].Process(real); 
                        imag = delay_array_imag[i].Process(imag);
                    }
                } else { // use the right delay array to process more bins for mono mode
                    if (i < delay_array_size / 2 - 1) { // should be index 0 to 174
                        real = delay_array_real[i].Process(real);  
                        imag = delay_array_imag[i].Process(imag);  
                    } else {    
                        real = delay_array_real2[i - 175].Process(real);  // TODO Note: When switching from stereo to mono, the data in these delay lines get moved to high frequency bins, making a ringing noise. Zero out delaylines before switching?
                        imag = delay_array_imag2[i - 175].Process(imag);  
                    }
                }
            }

            out[i] = real;
            out[i + offset] = imag;
    }
}

// shy_fft packs arrays as [real, real, real, ..., imag, imag, imag, ...]
inline void spectraldelay2(const float* in, float* out)
{
	for (size_t i = 0; i < N / 2; i++)  // loop i from 0 to 511
	{

            //float fft_bin = i + 1;

            float real = 0.0;
            float imag = 0.0;

            // each delayline affects 1 bin. 512 total bins. only using first 175 bins in more audible frequencies, and more causes dropouts
            if (i < delay_array_size && ((filter_mode == 0 && i < filter_bin) || (filter_mode == 1 && i > filter_bin) || filter_mode == 2)) {

                real = in[i];
                imag = in[i + offset];

                if (dipValues[2]) { //If Stereo Spread dipswitch is on
                    real = delay_array_real2[i].Process(real) * (1.0 - stereo_field[i]);   // spectraldelay() called every N/laps audio samples, 1024/4 = 256, about 188 times per second
                    imag = delay_array_imag2[i].Process(imag) * (1.0 - stereo_field[i]);
                } else {  // else Stereo Spread is Off
                    real = delay_array_real2[i].Process(real); 
                    imag = delay_array_imag2[i].Process(imag);
                }
            }

            out[i] = real;
            out[i + offset] = imag;
    }
}


// Typical Switch case for Message Type.
void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case NoteOn:
        {

            NoteOnEvent p = m.AsNoteOn();
            // This is to avoid Max/MSP Note outs for now..
            if(m.data[1] != 0)
            {
                p = m.AsNoteOn();
                // Do stuff with the midi Note/Velocity info here
                //osc.SetFreq(mtof(p.note));
                //osc.SetAmp((p.velocity / 127.0f));
            }
        }
        break;
        case ControlChange:
        {

            ControlChangeEvent p = m.AsControlChange();
            switch(p.control_number)
            {
                case 14:
                    midi_control[0] = true;
                    knobValues[0] = ((float)p.value / 127.0f);
                    break;
                case 15:
                    midi_control[1] = true;
                    knobValues[1] = ((float)p.value / 127.0f);
                    break;
                case 16:
                    midi_control[2] = true;
                    knobValues[2] = ((float)p.value / 127.0f);
                    break;
                case 17:
                    midi_control[3] = true;
                    knobValues[3] = ((float)p.value / 127.0f);
                    break;
                case 18:
                    midi_control[4] = true;
                    knobValues[4] = ((float)p.value / 127.0f);
                    break;
                case 19:
                    midi_control[5] = true;
                    knobValues[5] = ((float)p.value / 127.0f);
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
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_32KHZ); // Can run at 32kHz to handle more delaylines,
    samplerate = hw.AudioSampleRate();

    hw.SetAudioBlockSize(256); // less than 256 causes audio dropouts
           

    switch1[0]= Funbox::SWITCH_1_LEFT;
    switch1[1]= Funbox::SWITCH_1_RIGHT;
    switch2[0]= Funbox::SWITCH_2_LEFT;
    switch2[1]= Funbox::SWITCH_2_RIGHT;
    switch3[0]= Funbox::SWITCH_3_LEFT;
    switch3[1]= Funbox::SWITCH_3_RIGHT;
    dip[0]= Funbox::SWITCH_DIP_1;
    dip[1]= Funbox::SWITCH_DIP_2;
    dip[2]= Funbox::SWITCH_DIP_3;
    dip[3]= Funbox::SWITCH_DIP_4;

    pswitch1[0]= false;
    pswitch1[1]= false;
    pswitch2[0]= false;
    pswitch2[1]= false;
    pswitch3[0]= false;
    pswitch3[1]= false;
    pdip[0]= false;
    pdip[1]= false;
    pdip[2]= false;
    pdip[3]= false;

    // Expression
    expHandler.Init(6);
    expression_pressed = false;

    // Midi
    for( int i = 0; i < 6; ++i ) 
        midi_control[i] = false;  // Is this needed? or does it default to false

    predelay.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    mix.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    filter.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR); 
    delay_time.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::LINEAR);
    delay_fdbk.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    mod.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR);
    expression.Init(hw.expression, 0.0f, 1.0f, Parameter::LINEAR); 

    // initialize FFT and STFT objects    
    fft = new ShyFFT<float, N, RotationPhasor>();
    fft->Init();
    stft = new Fourier<float, N>(spectraldelay, fft, &hann, laps, in, middle, out);

    // initialize FFT and STFT objects    
    fft2 = new ShyFFT<float, N, RotationPhasor>();
    fft2->Init();
    stft2 = new Fourier<float, N>(spectraldelay2, fft2, &hann, laps, in2, middle2, out2);

    // Initialize delay array settings
    for(int i=0; i<175; i++) {
        delayLine_array_real[i].Init();
        delay_array_real[i].del = &delayLine_array_real[i];
        delay_array_real[i].delayTarget = 100; // in samples
        delay_array_real[i].feedback = 0.0;
        delay_array_real[i].active = true;

        delayLine_array_imag[i].Init();
        delay_array_imag[i].del = &delayLine_array_imag[i];
        delay_array_imag[i].delayTarget = 100; // in samples
        delay_array_imag[i].feedback = 0.0;
        delay_array_imag[i].active = true;

        delayLine_array_real2[i].Init();
        delay_array_real2[i].del = &delayLine_array_real2[i];
        delay_array_real2[i].delayTarget = 100; // in samples
        delay_array_real2[i].feedback = 0.0;
        delay_array_real2[i].active = true;

        delayLine_array_imag2[i].Init();
        delay_array_imag2[i].del = &delayLine_array_imag2[i];
        delay_array_imag2[i].delayTarget = 100; // in samples
        delay_array_imag2[i].feedback = 0.0;
        delay_array_imag2[i].active = true;
    }

    
    predelayLineLeft.Init();
    predelay_left.del = &predelayLineLeft;
    predelay_left.delayTarget = 2400; // in samples
    predelay_left.feedback = 0.0;
    predelay_left.active = true;

    predelayLineRight.Init();
    predelay_right.del = &predelayLineRight;
    predelay_right.delayTarget = 2400; // in samples
    predelay_right.feedback = 0.0;
    predelay_right.active = true;

    vmod = vmix = vpredelay = vdelay_time = vdelay_fdbk = 0.0; // making these global
    pdelay_time = pdelay_fdbk = ppredelay = 0.0;

    for(int i=0; i<175; i++){
        stereo_field[i] = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    }

    // Drift
    drift_osc.Init(samplerate);
    drift_osc.SetFreq(0.1);
    drift_osc.SetWaveform(0); // WAVE_SIN = 0, WAVE_TRI = 1, WAVE_SAW = 2, WAVE_RAMP = 3, WAVE_SQUARE = 4
    drift_osc.SetAmp(0.5);

    // Init the LEDs and set activate bypass
    led1.Init(hw.seed.GetPin(Funbox::LED_1),false);
    led1.Update();
    bypass = true;

    led2.Init(hw.seed.GetPin(Funbox::LED_2),false);
    led2.Update();


    //Initilize the PersistentStorage Object with default values.
    //Defaults will be the first values stored in flash when the device is first turned on. They can also be restored at a later date using the RestoreDefaults method
    Settings DefaultSettings = {0.0f, 0.0f};
    SavedSettings.Init(DefaultSettings);

    hw.InitMidi();
    hw.midi.StartReceive();

    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    while(1)
    {
        hw.midi.Listen();
        // Handle MIDI Events
        while(hw.midi.HasEvents())
        {
            HandleMidiMessage(hw.midi.PopEvent());
        }

        if(trigger_save) {
	    SavedSettings.Save(); // Writing locally stored settings to the external flash
	    trigger_save = false;
            blink = 0;
	}

	System::Delay(100);

    }

    delete stft;
    delete fft;

    delete stft2;
    delete fft2;

}