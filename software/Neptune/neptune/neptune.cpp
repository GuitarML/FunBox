// Neptune Reverb/Delay Pedal

#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"
#include "delayline_reverse.h"

#include "../CloudSeed/Default.h"
#include "../CloudSeed/ReverbController.h"
#include "../CloudSeed/FastSin.h"
#include "../CloudSeed/AudioLib/ValueTables.h"
#include "../CloudSeed/AudioLib/MathDefs.h"

using namespace daisy;
using namespace daisysp;
using namespace funbox; 

// Declare a local daisy_petal for hardware access
DaisyPetal hw;
::daisy::Parameter rsize, modify, mix, filter, delayTime, delayFDBK;
bool      bypass;
Led led1, led2;

//Tone tone;       // Low Pass
Tone toneOctLP;       // Low Pass for octave delay
//ATone toneHP;    // High Pass
float pfilter;

//Balance bal;     // Balance for volume correction in filtering

// Alternate functions
bool alternateMode;

float delayMix;   // Mix vs. Delay Level
float reverbMix;
float dryMix;
float wetMix;

float modAlt;  // Secondary modify function
float preDelayAlt;

bool force_reset;
bool release;
int release_counter;

bool freeze;
float  del_FDBK_Override;

float ramp;
bool mode_changed;
bool triggerMode;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[2];
int             switch1[2], switch2[2], switch3[2], dip[2];

// Initialize "previous" p values
float psize, pmodify, pdelayTime, pmix;

CloudSeed::ReverbController* reverb = 0;

// This is used in the modified CloudSeed code for allocating
// delay line memory to SDRAM (64MB available on Daisy)
#define CUSTOM_POOL_SIZE (48*1024*1024)
DSY_SDRAM_BSS char custom_pool[CUSTOM_POOL_SIZE];
size_t pool_index = 0;
int allocation_count = 0;
void* custom_pool_allocate(size_t size)
{
        if (pool_index + size >= CUSTOM_POOL_SIZE)
        {
                return 0;
        }
        void* ptr = &custom_pool[pool_index];
        pool_index += size;
        return ptr;
}

// Delay
#define MAX_DELAY static_cast<size_t>(48000 * 4.0f) // 4 second max delay

DelayLineReverse<float, MAX_DELAY> DSY_SDRAM_BSS delayLineRev;
DelayLineReverse<float, MAX_DELAY> DSY_SDRAM_BSS delayLine2;
DelayLineReverse<float, MAX_DELAY> DSY_SDRAM_BSS delayLineRev2;
DelayLineReverse<float, MAX_DELAY> DSY_SDRAM_BSS delayLine4;

struct delay
{
    //DelayLine<float, MAX_DELAY> *del;
    DelayLineReverse<float, MAX_DELAY> *del;
    float                        currentDelay;
    float                        delayTarget;
    float                        feedback;
    float                        active = false;
    bool                         reverseMode = false;
    unsigned int                 speed = 1;

    float Process(float in)
    {
        //set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        del->SetDelay1(currentDelay);
        del->SetSpeed(speed);

        float read = del->ReadFwd();
        if (speed == 2) {
            read = toneOctLP.Process(read);  // LP filter on octave delay to tame harsh high pitch tones
        }

        float readRev = del->ReadRev();
        if (active) {
            del->Write((feedback * read) + in);
            //delRev->Write((feedback * read) + in);
        } else {
            del->Write(feedback * read);
            //delRev->Write(feedback * read);
        }

        if (reverseMode)
            return readRev;
        else
            return read;
    }
};
// Left channel
delay             delay1;
delay             delay2;   // Two delay lines is too much for Daisy to keep up with when in reverse mode, try the simpler interpolation method in delayline code
// Right channel
delay             delay3;
delay             delay4;


bool knobMoved(float old_value, float new_value)        //// UPDATE  //// UPDATE  //// UPDATE
{
    float tolerance = 0.005;
    if (new_value > (old_value + tolerance) || new_value < (old_value - tolerance)) {
        return true;
    } else {
        return false;
    }
}

void changeMode()
{
    // For the different modes, write a preset in the CloudSeed code, and switch presets here
    reverb->ClearBuffers();

    if (switch1[0] == true) {                            // Hall
        //reverb->initHall();
        reverb->initFactoryChorus();

    } else if (switch1[1] == true) {                            // Cloud
        //reverb->initCloud();
        reverb->initFactoryMediumSpace();

    } else {                            // Spring
        //reverb->initSpring();
        reverb->initFactoryRubiKaFields();

    } 
    reverb->SetParameter(::Parameter2::LineCount, 2);
    force_reset = true; // This allows the knob controlled params to be reset without moving the knobs, when a new mode is selected
}

void updateSwitch3() 
{
    // DELAY ///////////////////////
    if (pswitch3[0]) {                // If switch3 is up, Normal FWD mode
        delay1.active = true;
        delay2.active = false;
        delay1.speed = 1;
        delay2.speed = 1;
        delay2.reverseMode = false;

        delay3.active = true;
        delay4.active = false;
        delay3.speed = 1;
        delay4.speed = 1;
        delay4.reverseMode = false;


    } else if (pswitch3[1]) {         // If switch3 is down, REV mode
        delay1.active = false;
        delay2.active = true;
        delay1.speed = 1;
        delay2.speed = 1;
        delay2.reverseMode = true;

        delay3.active = false;
        delay4.active = true;
        delay3.speed = 1;
        delay4.speed = 1;
        delay4.reverseMode = true;

    } else {                           // If switch3 is middle, Octave mode
        delay1.active = true;
        delay2.active = false;
        delay1.speed = 2;
        delay2.speed = 2;
        delay2.reverseMode = false;

        delay3.active = true;
        delay4.active = false;
        delay3.speed = 2;
        delay4.speed = 2;
        delay4.reverseMode = false;
    }
}


void UpdateButtons()
{
    // (De-)Activate bypass and toggle LED when left footswitch is let go
    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {
        if (alternateMode) {
            alternateMode = false;
            force_reset = true; // force parameter reset to enforce current knob settings when leaving alternate mode
            led1.Set(1.0f);
        } else {
            bypass = !bypass;
            led1.Set(bypass ? 0.0f : 1.0f);
        }
    }

    // Toggle Alternate mode by holding down the left footswitch, if not already in alternate mode and not in bypass
    if(hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 500 && !alternateMode && !bypass && !freeze) {
        alternateMode = true;
        led1.Set(0.5f);  // Dim LED in alternate mode
    }

    // TODO FOR FREEZE : Turn off LATE EQ FILTERS when frozen, but then set to previous setting (on or off) when done

    // If switch2 is pressed, freeze the current delay by not applying new delay and setting feedback to 1.0, set to false by default
    if(hw.switches[Funbox::FOOTSWITCH_2].RisingEdge())
    {
        // Apply freeze
        reverb->SetParameter(::Parameter2::LineDecay, 1.1); // Set to max feedback/decay (~60 seconds)  TODO: Try setting greater than 1
        freeze = true;
        del_FDBK_Override = 1.0;
        led2.Set(1.0);       // Keep LED on if frozen
    }

    // Reset reverb LineDecay when freeze is let go
    if(hw.switches[Funbox::FOOTSWITCH_2].FallingEdge())
    {
        freeze = false;
        release = true;
        led2.Set(0.0);       // Turn off led when footswitch released

        float vsize = rsize.Process();
        reverb->SetParameter(::Parameter2::LineDecay, vsize / 4.0); // Set to max feedback/decay (~60 seconds)
    }
}


void UpdateSwitches()
{

    // Detect any changes in switch positions

    // TODO UPDATE WITH LOGIC FROM MARS CODE

    // 3-way Switch 1
    bool changed1 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch1[i]].Pressed() != pswitch1[i]) {
            pswitch1[i] = hw.switches[switch1[i]].Pressed();
            changed1 = true;
            
        }
    }
    if (changed1) 
        mode_changed = true;



    // 3-way Switch 2
    bool changed2 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch2[i]].Pressed() != pswitch2[i]) {
            pswitch2[i] = hw.switches[switch2[i]].Pressed();
            changed2 = true;
        }
    }
    //if (changed2) 
    //    updateSwitch2();

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
            // Action for dipswitches handled in audio callback
        }
    }


    if (mode_changed) {
        if (ramp > 0.95) { //TODO See if this helps prevent mode from changing too quickly and overloading
            ramp = 0.0;
            triggerMode = true;

        }
    }

}


// This runs at a fixed rate, to prepare audio samples
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{

    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();
    led1.Update();
    led2.Update();

    UpdateButtons();
    UpdateSwitches();

    float vsize = rsize.Process();
    float vmodify = modify.Process();

    float vmix = mix.Process();
    float vfilter = filter.Process();

    float vdelayTime = delayTime.Process();
    float vdelayFDBK = delayFDBK.Process();


    if (pmix != vmix || force_reset == true) {
        // Handle Normal or Alternate Mode Mix Controls
        //    A cheap mostly energy constant crossfade from SignalSmith Blog
        float x2 = 1.0 - vmix;
        float A = vmix*x2;
        float B = A * (1.0 + 1.4186 * A);
        float C = B + vmix;
        float D = B + x2;

        if (alternateMode == false) {
            wetMix = C * C;
            dryMix = D * D;
            pmix = vmix;
        //} else if (alternateMode == true && pmix != vmix) {
        } else if (alternateMode == true && knobMoved(pmix, vmix)) {    //// UPDATE   //// UPDATE   //// UPDATE
            delayMix = C * C;
            reverbMix = D * D;
            pmix = vmix;
        }
        //pmix = vmix;
    }


    // Set Filter Controls
    /*
    if (vfilter <= 0.5) {
        //float filter_value = (vfilter * 39800.0f) + 100.0f;
        //tone.SetFreq(filter_value);
        reverb->SetParameter(::Parameter2::LineDecay, (vsize * .38 + 0.295)); // Range .295 to .675
    } else {
        //float filter_value = (vfilter - 0.5) * 800.0f + 40.0f;
        //toneHP.SetFreq(filter_value);
    }
    */

    if (knobMoved(pfilter, vfilter)) {
        reverb->SetParameter(::Parameter2::PostCutoffFrequency, vfilter); // TODO maybe do something with the shelf filters as well
        pfilter = vfilter;
    }


    float fvdelayTime = 1.0 - vdelayTime; //Invert parameter for correct knob operation

    if (vdelayTime <= 0.01) {   // if knob < 1%, set delay to inactive
        delay1.active = false;
        delay2.active = false;
        delay3.active = false;
        delay4.active = false;
    }


    if (vdelayTime <= 0.5) {
        delay1.delayTarget = 144000 + (fvdelayTime - 0.5) * 48000;   // should be a range 192000 -> 144000 samples as knob goes from 0 to midpoint
        delay3.delayTarget = 144000 + (fvdelayTime - 0.5) * 48000;   // should be a range 192000 -> 144000 samples as knob goes from 0 to midpoint

    } else {
        delay1.delayTarget = 2400 + fvdelayTime * 283200;  // should be range 144000 -> 2400 samples as knob goes from midpoint to max
        delay3.delayTarget = 2400 + fvdelayTime * 283200;  // should be range 144000 -> 2400 samples as knob goes from midpoint to max

    }

    delay2.delayTarget = 96000 - (delay1.delayTarget / 2.0);
    delay4.delayTarget = 96000 - (delay1.delayTarget / 2.0);

    if (freeze) {
        delay1.feedback = del_FDBK_Override;
        delay2.feedback = del_FDBK_Override;
        delay3.feedback = del_FDBK_Override;
        delay4.feedback = del_FDBK_Override;
    } else {
        delay1.feedback = vdelayFDBK;
        delay2.feedback = vdelayFDBK;
        delay3.feedback = vdelayFDBK;
        delay4.feedback = vdelayFDBK;
    }

    if (release) {
        release_counter += 1;
        if (release_counter > 600) {  // Wait 100ms then reset reverb decay    TODO: How long should decay be held at 0? Should Decay be ramped down/up?
            release_counter = 0;
            release = false;
            force_reset = true; // Forces reverb LineDecay (size) to be reset to current knob setting
        }
    }


    // Set Reverb Parameters ///////////////
    if ((psize != vsize) || force_reset == true)
    {
        if (!alternateMode && !freeze && !release) {

            if (pswitch1[0] == true) {  // Hall
                reverb->SetParameter(::Parameter2::LineDecay, (vsize * .38 + 0.295)); // Range .295 to .675

            } else if (pswitch1[1] == true) {  // Cloud
                reverb->SetParameter(::Parameter2::LineDecay, (vsize * 0.58 + 0.42)); // Range 0.42 to 1.0

            } else {  // Spring
                reverb->SetParameter(::Parameter2::LineDecay, (vsize * 0.35 + 0.35)); // Range 0.35 to 0.7

            }
            
            psize = vsize;

        //} else if (alternateMode && !freeze) {
        } else if (alternateMode && knobMoved(psize, vsize) && !freeze) {    //// UPDATE   //// UPDATE   //// UPDATE
            preDelayAlt = vsize;
            reverb->SetParameter(::Parameter2::PreDelay, preDelayAlt);
            psize = vsize;
        }
    }

    if ((pmodify != vmodify || force_reset == true)) // Force reset to apply knob params when switching reverb modes, or when alternate mode released
    {
        if (!alternateMode) {
            reverb->SetParameter(::Parameter2::LineModAmount, vmodify); // Range 0 to 1
            pmodify = vmodify;
        //} else {
        } else if (alternateMode && knobMoved(pmodify, vmodify)) {
            modAlt = vmodify;
            reverb->SetParameter(::Parameter2::LineModRate, modAlt); // Range 0 to ~1
            pmodify = vmodify;
        }
    }

    force_reset = false;

    //float inL[48];
    //float outL[48];
    //float inR[48];
    //float outR[48];

    //float delay_out[48];

    float inL[1];
    float outL[1];
    float inR[1];
    float outR[1];

    float delay_outL;
    float delay_inL;
    float delay_outR;
    float delay_inR;

    float inputL;
    float inputR;

    // Order of effects
    //   Delay -> Reverb -> 

    // Note: If I need to reduce processing somewhere, try removing the Filter/Balance and use only CloudSeed's internal tone controls
    //        Will need to double the delay's for stereo

    if(!bypass) {
        for (size_t i = 0; i < size; i++)
        {
            // Stereo or MISO 
            if (pdip[0]) {  // Stereo
                inputL = in[0][i];
                inputR = in[1][i];
            } else {     //MISO
                inputL = in[0][i];
                inputR = in[0][i];
            }
 
            if (freeze) {
                delay_inL = 0.0;
                delay_inR = 0.0;
            } else {
                delay_inL = inputL;  // Process any samplerate reduction for input to delay and reverb
                delay_inR = inputR;
            }

            delay_outL = ((delay1.Process(delay_inL) + delay2.Process(delay_inL)) * delayMix);
            delay_outR = ((delay3.Process(delay_inR) + delay4.Process(delay_inR)) * delayMix);

            //delay_outL = delay_inL;
    
            //delay_outR = delay_inR;

            // Delay and Reverb Routing based on Switch3 Position
            //        Up=Delay + and into Reverb     Middle= Delay and Reverb in Parallel  Down= Delay into Reverb

            if (pswitch2[0] || pswitch2[1]) {  // Up or Down Delay into reverb
                inL[0] = delay_inL + delay_outL;
                inR[0] = delay_inR + delay_outR;

            } else if (!pswitch2[0] && !pswitch2[1]) { // Middle position process R and D in parallel
                inL[0] = delay_inL;
                inR[0] = delay_inR;
            }


            reverb->Process(inL, inR, outL, outR, 1); // Try doing the 1 sample like in effects suite, then just have one sample loop
            //outL[0] = inL[0];
            //outR[0] = inR[0];

            //float filter_in;
            //float filter_out;

            float balanced_outL;
            float balanced_outR;

            if (!pswitch2[1]) {  // If switch3 not down (if either up or middle position) add delay
                balanced_outL =  (outL[0] * (reverbMix * 1.2) + delay_outL) * wetMix; // Apply mix and level controls, slight reverb boost
                balanced_outR =  (outR[0] * (reverbMix * 1.2) + delay_outR) * wetMix;
            } else {
                balanced_outL =  (outL[0] * (reverbMix * 1.2))  * wetMix; // Apply mix and level controls, slight reverb boost
                balanced_outR =  (outR[0] * (reverbMix * 1.2))  * wetMix;
            }

            /*
            if (vfilter <= 0.5) {
                filter_out = tone.Process(filter_in);
                balanced_out = bal.Process(filter_out, filter_in);

            } else {
                filter_out = toneHP.Process(filter_in);
                balanced_out = bal.Process(filter_out, filter_in);
            }
            */

            out[0][i] = inputL * dryMix + balanced_outL * wetMix;
            out[1][i] = inputR * dryMix + balanced_outR * wetMix;

            if (ramp < 1.0) {
                ramp += 0.001;
                out[0][i] = 0.0;
                if (ramp > 0.1 && triggerMode == true) {
                    triggerMode = false;
                    changeMode();
                    mode_changed = false;
                }

            }
        }

    } else {
        for (size_t i = 0; i < size; i++)
        {
            // Stereo or MISO 
            if (pdip) {  // Stereo
                inputL = in[0][i];
                inputR = in[1][i];
            } else {     //MISO
                inputL = in[0][i];
                inputR = in[0][i];
            }
            out[0][i] = inputL;
            out[1][i] = inputR;
        }
    }
}

int main(void)
{
    float samplerate;

    hw.Init();
    samplerate = hw.AudioSampleRate();

    AudioLib::ValueTables::Init();
    CloudSeed::FastSin::Init();

    reverb = new CloudSeed::ReverbController(samplerate);
    changeMode();
    using namespace CloudSeed;
    reverb->SetParameter(::Parameter2::PreDelay, 0.0);  // Set initial predelay to 0.0 (controlled by Alt param)

    hw.SetAudioBlockSize(48);

    rsize.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    modify.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);

    delayTime.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    delayFDBK.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);

    mix.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    filter.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, ::daisy::Parameter::CUBE);


    // Alternate Parameters
    alternateMode = false;

    delayMix = 0.703;   // Reverb vs. Delay Level
    reverbMix = 0.703;

    pmix = 0.5;
    modAlt = 0.0;
    preDelayAlt = 0.0;

    psize = 0.0;
    pmodify = 0.0;
    pdelayTime = 0.0;

    delayLineRev.Init();
    delay1.del = &delayLineRev;
    delay1.delayTarget = 2400; // in samples
    delay1.feedback = 0.0;
    delay1.active = true;     // Default to no delay

    delayLine2.Init();
    delay2.del = &delayLine2;
    delay2.delayTarget = 2400; // in samples
    delay2.feedback = 0.0;
    delay2.active = false;     // Default to no delay

    delayLineRev2.Init();
    delay3.del = &delayLineRev2;
    delay3.delayTarget = 2400; // in samples
    delay3.feedback = 0.0;
    delay3.active = true;     // Default to no delay

    delayLine4.Init();
    delay4.del = &delayLine4;
    delay4.delayTarget = 2400; // in samples
    delay4.feedback = 0.0;
    delay4.active = false;     // Default to no delay

    //tone.Init(samplerate);
    //toneHP.Init(samplerate);

    toneOctLP.Init(samplerate);
    toneOctLP.SetFreq(3000.0);

    //bal.Init(samplerate);
    pfilter = 0.5;

    freeze = false;
    release = false;
    release_counter = 0;
    force_reset = false;

    ramp = 0.0;
    triggerMode = false;

    switch1[0]= Funbox::SWITCH_1_LEFT;
    switch1[1]= Funbox::SWITCH_1_RIGHT;
    switch2[0]= Funbox::SWITCH_2_LEFT;
    switch2[1]= Funbox::SWITCH_2_RIGHT;
    switch3[0]= Funbox::SWITCH_3_LEFT;
    switch3[1]= Funbox::SWITCH_3_RIGHT;
    dip[0]= Funbox::SWITCH_DIP_1;
    dip[1]= Funbox::SWITCH_DIP_2;

    pswitch1[0]= false;
    pswitch1[1]= false;
    pswitch2[0]= false;
    pswitch2[1]= false;
    pswitch3[0]= false;
    pswitch3[1]= false;
    pdip[0]= false;
    pdip[1]= false;

    // Init the LEDs and set activate bypass
    led1.Init(hw.seed.GetPin(Funbox::LED_1),false);
    led1.Update();
    bypass = true;

    led2.Init(hw.seed.GetPin(Funbox::LED_2),false);
    led2.Update();

    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    while(1)
    {
        // Do Stuff Infinitely Here
        System::Delay(10);
    }
}