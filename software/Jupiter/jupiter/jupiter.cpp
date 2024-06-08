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
::daisy::Parameter rdecay, mix, lowfreq, lowshelf, highfreq, highshelf;
bool      bypass;
Led led1, led2;


float dryMix;
float wetMix;

bool force_reset;
bool release;
int release_counter;

bool freeze;



bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[2];
int             switch1[2], switch2[2], switch3[2], dip[2];

// Initialize "previous" p values
float prdecay, plowfreq, plowshelf, pmix, phighfreq, phighshelf;

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

void updateSwitch1()  // TODO: Tune these settings so that they make 3 ear pleasing stages of "Lushness"
{

    if (pswitch1[0] == true) {

        reverb->SetParameter(::Parameter2::LateDiffusionEnabled, 0.0);
        reverb->SetParameter(::Parameter2::LateDiffusionStages,  0.4);

        reverb->SetParameter(::Parameter2::LateStageTap, 1.0);
        reverb->SetParameter(::Parameter2::Interpolation, 0.0);

        reverb->SetParameter(::Parameter2::LineCount, 1); 

    } else if (pswitch1[1] == true) {

        reverb->SetParameter(::Parameter2::LateDiffusionEnabled, 1.0);
        reverb->SetParameter(::Parameter2::LateDiffusionStages, 0.2);

        reverb->SetParameter(::Parameter2::LateStageTap, 0.0);
        reverb->SetParameter(::Parameter2::Interpolation, 1.0);

        reverb->SetParameter(::Parameter2::LineCount, 2); 

    } else {

        reverb->SetParameter(::Parameter2::LateDiffusionEnabled, 1.0);
        reverb->SetParameter(::Parameter2::LateDiffusionStages, 0.4);

        reverb->SetParameter(::Parameter2::LateStageTap, 1.0);
        reverb->SetParameter(::Parameter2::Interpolation, 1.0);

        reverb->SetParameter(::Parameter2::LineCount, 2); 

    }

}


void updateSwitch2() 
{
    unsigned int irIndex = 0;

    if (pswitch2[0] == true) {
        reverb->SetParameter(::Parameter2::LineModAmount, 0.0); // Range 0 to ~1
        reverb->SetParameter(::Parameter2::LineModRate, 0.0); // Range 0 to ~1
    } else if (pswitch2[1] == true) {
        reverb->SetParameter(::Parameter2::LineModAmount, 1.0); // Range 0 to ~1
        reverb->SetParameter(::Parameter2::LineModRate, 1.0); // Range 0 to ~1
    } else {
        reverb->SetParameter(::Parameter2::LineModAmount, 0.5); // Range 0 to ~1
        reverb->SetParameter(::Parameter2::LineModRate, 0.3); // Range 0 to ~1
    }

}



void updateSwitch3() 
{
    if (pswitch3[0] == true) {
        reverb->SetParameter(::Parameter2::PreDelay, 0.0);
    } else if (pswitch3[1] == true) {
        reverb->SetParameter(::Parameter2::PreDelay, 0.5);
    } else {
        reverb->SetParameter(::Parameter2::PreDelay, 0.2);
    }
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
        reverb->SetParameter(::Parameter2::LineDecay, 1.1); // Set to max feedback/rdecay (~60 seconds)  TODO: Try setting greater than 1
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
            // Action for dipswitches handled in audio callback
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
    float vmix = mix.Process();
    
    float vlowfreq = lowfreq.Process();
    float vlowshelf = lowshelf.Process();

    float vhighfreq = highfreq.Process();
    float vhighshelf = highshelf.Process();


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


    if ((plowfreq != vlowfreq || force_reset == true)) // Force reset to apply knob params when switching reverb modes
    {
        if (knobMoved(plowfreq, vlowfreq) || force_reset == true) {

            reverb->SetParameter(::Parameter2::PostLowShelfFrequency, vlowfreq);
            plowfreq = vlowfreq;
        }
    }

    if ((plowshelf != vlowshelf || force_reset == true)) // Force reset to apply knob params when switching reverb modes
    {
        if (knobMoved(plowshelf, vlowshelf) || force_reset == true) {

            reverb->SetParameter(::Parameter2::PostLowShelfGain, vlowshelf);
            plowshelf = vlowshelf;
        }
    }

    if ((phighfreq != vhighfreq || force_reset == true)) // Force reset to apply knob params when switching reverb modes
    {
        if (knobMoved(phighfreq, vhighfreq) || force_reset == true) {

            reverb->SetParameter(::Parameter2::PostHighShelfFrequency, vhighfreq);
            phighfreq = vhighfreq;
        }
    }

    if ((phighshelf != vhighshelf || force_reset == true)) // Force reset to apply knob params when switching reverb modes
    {
        if (knobMoved(phighshelf, vhighshelf) || force_reset == true) {

            reverb->SetParameter(::Parameter2::PostHighShelfGain, vhighshelf);
            phighshelf = vhighshelf;
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

    using namespace CloudSeed;
    reverb->ClearBuffers();
    reverb->initFactoryRubiKaFields();
    reverb->SetParameter(::Parameter2::LineCount, 2); // TODO Set based on reverb type
    force_reset = true; // This allows the knob controlled params to be reset without moving the knobs, when a new mode is selected


    hw.SetAudioBlockSize(48);

    rdecay.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, ::daisy::Parameter::LINEAR); // LOG to go high fast?
    lowfreq.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    highfreq.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    mix.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    lowshelf.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    highshelf.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);


    pmix = 0.5;
    plowfreq = 0.0;
    prdecay = 0.0;
    plowshelf = 0.0;
    phighfreq = 0.0;
    phighshelf = 0.0;

    freeze = false;
    release = false;
    release_counter = 0;
    force_reset = false;

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