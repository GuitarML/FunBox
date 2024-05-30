// Neptune Reverb/Delay Pedal

#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"

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
::daisy::Parameter rdecay, predelay, mod, rate, mix, filter;
bool      bypass;
Led led1, led2;


float dryMix;
float wetMix;

bool force_reset;
bool release;
int release_counter;

bool freeze;

float ramp;
bool mode_changed;
bool triggerMode;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[2];
int             switch1[2], switch2[2], switch3[2], dip[2];

// Initialize "previous" p values
float prdecay, pmod, pPreDelay, pmix, pfilter, prate;

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



bool knobMoved(float old_value, float new_value)
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

    // Presets are controlled by left and middle switch

    // Left/Left  Small room
    // Left/Mid   Med space
    // Left/Right Dull Echoes

    // Mid/Left  Factory Chorus
    // Mid/Mid   Noise in Hallway
    // Mid/Right 90s

    // Right/Left  RubiKai
    // Right/Mid   LookingGlass
    // Right/Right Hyperplane

    reverb->ClearBuffers();

    if (pswitch1[0] == true) {                 // Switch 1 Left
        if (pswitch2[0] == true) {
            reverb->initFactorySmallRoom();       // Switch 2 Left
        } else if (pswitch2[1] == true) {
            reverb->initFactoryDullEchos();    // Switch 2 Right
        } else {
            reverb->initFactoryMediumSpace();  // Switch 2 Center
        }

    } else if (pswitch1[1] == true) {           // Switch 1 Right
        if (pswitch2[0] == true) {                  // Switch 2 Left
            reverb->initFactoryRubiKaFields();
        } else if (pswitch2[1] == true) {                  // Switch 2 Right
            reverb->initFactoryHyperplane();
        } else {                                    // Switch 2 Center
            reverb->initFactoryThroughTheLookingGlass();
        }

        reverb->initFactoryMediumSpace();

    } else {                                   // Switch 1 Center
        if (pswitch2[0] == true) {                  // Switch 2 Left
            reverb->initFactoryChorus();
        } else if (pswitch2[1] == true) {          // Switch 2 Right
            reverb->initFactory90sAreBack();
        } else {                                   // Switch 2 Center
            reverb->initFactoryNoiseInTheHallway();
        }          
    } 

    reverb->SetParameter(::Parameter2::LineCount, 2); // TODO Set based on reverb type
    force_reset = true; // This allows the knob controlled params to be reset without moving the knobs, when a new mode is selected
}

void updateSwitch3() 
{
    // Handled in the audio callback
}


void UpdateButtons()
{
    // (De-)Activate bypass and toggle LED when left footswitch is let go
    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {
        bypass = !bypass;
        led1.Set(bypass ? 0.0f : 1.0f);
    }

    // Toggle Alternate mode by holding down the left footswitch, if not already in alternate mode and not in bypass
    //if(hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 500 && !bypass && !freeze) {

    //}


    // If switch2 is pressed, freeze the current delay by not applying new delay and setting feedback to 1.0, set to false by default
    if(hw.switches[Funbox::FOOTSWITCH_2].RisingEdge())
    {
        // Apply freeze
        reverb->SetParameter(::Parameter2::LineDecay, 1.2); // Set to max feedback/rdecay (~60 seconds)  TODO: Try setting greater than 1
        freeze = true;

        led2.Set(1.0);       // Keep LED on if frozen
    }

    // Reset reverb LineDecay when freeze is let go
    if(hw.switches[Funbox::FOOTSWITCH_2].FallingEdge())
    {
        freeze = false;
        release = true;
        led2.Set(0.0);       // Turn off led when footswitch released

        float vrdecay = rdecay.Process();
        reverb->SetParameter(::Parameter2::LineDecay, vrdecay / 4.0); // Set to max feedback/rdecay (~60 seconds)
    }
}


void UpdateSwitches()
{

    // Detect any changes in switch positions

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
    if (changed2) 
        mode_changed = true;
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

    float vrdecay = rdecay.Process();
    float vpredelay = predelay.Process();

    float vmod = mod.Process();
    float vrate = rate.Process();

    float vmix = mix.Process();
    float vfilter = filter.Process();


    if (pmix != vmix || force_reset == true) {
        if (knobMoved(pmix, vmix) || force_reset == true) {
            //    A cheap mostly energy constant crossfade from SignalSmith Blog
            float x2 = 1.0 - vmix;
            float A = vmix*x2;
            float B = A * (1.0 + 1.4186 * A);
            float C = B + vmix;
            float D = B + x2;

            wetMix = C * C;
            dryMix = D * D;
            pmix = vmix;
        }
    }


    if (release) {
        release_counter += 1;
        if (release_counter > 600) {  // Wait 100ms then reset reverb rdecay    TODO: How long should rdecay be held at 0? Should Decay be ramped down/up?
            release_counter = 0;
            release = false;
            force_reset = true; // Forces reverb LineDecay (size) to be reset to current knob setting
        }
    }


    // Set Reverb Parameters ///////////////


    if ((prdecay != vrdecay || force_reset == true)) // Force reset to apply knob params when switching reverb modes
    {

        if (knobMoved(prdecay, vrdecay) && !freeze && !release || force_reset == true) {

            reverb->SetParameter(::Parameter2::LineDecay, (vrdecay)); // Range 0 to 1
            prdecay = vrdecay;
        }
    }


    if ((pPreDelay != vpredelay || force_reset == true)) // Force reset to apply knob params when switching reverb modes
    {
        if (knobMoved(pPreDelay, vpredelay) || force_reset == true) {

            reverb->SetParameter(::Parameter2::PreDelay, vpredelay);
            pPreDelay = vpredelay;
        }
    }


    if ((pmod != vmod || force_reset == true)) // Force reset to apply knob params when switching reverb modes
    {

        if (knobMoved(pmod, vmod) || force_reset == true) {

            reverb->SetParameter(::Parameter2::LineModAmount, vmod); // Range 0 to ~1
            pmod = vmod;
        }
    }

    if ((prate != vrate || force_reset == true)) // Force reset to apply knob params when switching reverb modes
    {
        if (knobMoved(prate, vrate) || force_reset == true) {

            reverb->SetParameter(::Parameter2::LineModRate, vrate); // Range 0 to ~1
            prate = vrate;
        }
    }

    if ((pfilter != vfilter || force_reset == true)) // Force reset to apply knob params when switching reverb modes
    {
        if (knobMoved(pfilter, vfilter) || force_reset == true) {      // TODO: Right now these are reset for each reverb preset, do I want them to persist?
        // Filter modes///////////////////////
            if (pswitch3[0]) {                // If switch3 is left, Lowpass filter

                reverb->SetParameter(::Parameter2::PostCutoffFrequency, vfilter); 

            } else if (pswitch3[1]) {         // If switch3 is right, High Shelf

                reverb->SetParameter(::Parameter2::PostHighShelfGain, vfilter); 

            } else {                           // If switch3 is middle, Low Shelf

                reverb->SetParameter(::Parameter2::PostLowShelfGain, vfilter); 

            }

            pfilter = vfilter;
        }
    }

    force_reset = false;

    float inL[1];
    float outL[1];
    float inR[1];
    float outR[1];

    float inputL;
    float inputR;

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
                inL[0] = 0.0;
                inR[0] = 0.0;
            } else {
                inL[0] = inputL;
                inR[0] = inputR;
            }


            reverb->Process(inL, inR, outL, outR, 1); 

            out[0][i] = inputL * dryMix + outL[0] * wetMix;
            out[1][i] = inputR * dryMix + outR[0] * wetMix;

            // Silence and then ramp volume when switching reverb presets
            if (ramp < 1.0) {
                ramp += 0.001;
                out[0][i] = 0.0;
                out[1][i] = 0.0;
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
            if (pdip[0]) {  // Stereo
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
    //reverb->SetParameter(::Parameter2::PreDelay, 0.0);  // Set initial predelay to 0.0 (controlled by Alt param)

    hw.SetAudioBlockSize(48);

    rdecay.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    mod.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    mix.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    predelay.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    rate.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, ::daisy::Parameter::CUBE);
    filter.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);


    pmix = 0.5;
    pPreDelay = 0.0;
    prdecay = 0.0;
    pmod = 0.0;
    prate = 0.0;
    pfilter = 0.0;

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
    //dip[2]= Funbox::SWITCH_DIP_3;
    //dip[3]= Funbox::SWITCH_DIP_4;

    pswitch1[0]= false;
    pswitch1[1]= false;
    pswitch2[0]= false;
    pswitch2[1]= false;
    pswitch3[0]= false;
    pswitch3[1]= false;
    pdip[0]= false;
    pdip[1]= false;
    //pdip[2]= false;
    //pdip[3]= false;

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