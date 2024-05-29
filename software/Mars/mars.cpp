#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"
#include <RTNeural/RTNeural.h>  // NOTE: Need to use older version of RTNeural, same as GuitarML/Seed
#include "delayline_2tap.h"
#include "ImpulseResponse/ImpulseResponse.h"


// Model Weights (edit this file to add model weights trained with Colab script)
//    The models must be GRU (gated recurrent unit) with hidden size = 9, snapshot models (not condidtioned on a parameter)
#include "all_model_data_gru9_4count.h"
#include "ImpulseResponse/ir_data.h"


using namespace daisy;
using namespace daisysp;
using namespace funbox;  // This is important for mapping the correct controls to the Daisy Seed on Funbox PCB

// Declare a local daisy_petal for hardware access
DaisyPetal hw;
Parameter Gain, Level, Mix, filter, delayTime, delayFdbk;
bool            bypass;
int             modelInSize;
unsigned int    modelIndex;
bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[2];
int             switch1[2], switch2[2], switch3[2], dip[2];
float           nnLevelAdjust;
int             indexMod;

float dryMix, wetMix;

Led led1, led2;

float           mix_effects;

float vfilter;
Tone tone;       // Low Pass
ATone toneHP;    // High Pass
Balance bal;     // Balance for volume correction in filtering

// Impulse Response
ImpulseResponse mIR;
int   m_currentIRindex;


// Delay Max Definitions (Assumes 48kHz samplerate)
#define MAX_DELAY static_cast<size_t>(48000.0f * 2.f)
DelayLine2Tap<float, MAX_DELAY> DSY_SDRAM_BSS delayLine;
// Delay with dotted eighth and triplett options
struct delay
{
    DelayLine2Tap<float, MAX_DELAY> *del;
    float                        currentDelay;
    float                        delayTarget;
    float                        feedback = 0.0;
    float                        active = false;
    float                        level = 1.0;      // Level multiplier of output
    bool                         secondTapOn = false;
    
    float Process(float in)
    {
        //set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        del->SetDelay(currentDelay);

        float read = del->Read();

        float secondTap = 0.0;
        if (secondTapOn) {
            secondTap = del->ReadSecondTap();
        }

        if (active) {
            del->Write((feedback * read) + in);
        } else {
            del->Write(feedback * read); // if not active, don't write any new sound to buffer
        }

        return (read + secondTap) * level;

    }
};

delay             delay1;


// Neural Network Model
// Currently only using snapshot models, they tend to sound better and 
//   we can use input level as gain.

RTNeural::ModelT<float, 1, 1,
    RTNeural::GRULayerT<float, 1, 9>,
    RTNeural::DenseT<float, 9, 1>> model;

// Notes: With default settings, GRU 10 is max size currently able to run on Daisy Seed
//        - Parameterized 1-knob GRU 10 is max, GRU 8 with effects is max
//        - Parameterized 2-knob/3-knob at GRU 8 is max
//        - With multi effect (reverb, etc.) added GRU 9 is recommended to allow room for processing of other effects
//        - These models should be trained using 48kHz audio data, since Daisy uses 48kHz by default.
//             Models trained with other samplerates, or running Daisy at a different samplerate will sound different.


void updateSwitch1() 
{
    unsigned int modelIndex = 0;

    if (pswitch1[0] == true) {
        modelIndex = 1;
    } else if (pswitch1[1] == true) {
        modelIndex = 3;
    } else {
        modelIndex = 2;
    }

    auto& gru = (model).template get<0>();
    auto& dense = (model).template get<1>();
    modelInSize = 1;
    gru.setWVals(model_collection[modelIndex].rec_weight_ih_l0);
    gru.setUVals(model_collection[modelIndex].rec_weight_hh_l0);
    gru.setBVals(model_collection[modelIndex].rec_bias);
    dense.setWeights(model_collection[modelIndex].lin_weight);
    dense.setBias(model_collection[modelIndex].lin_bias.data());
    model.reset();

    nnLevelAdjust = model_collection[modelIndex].levelAdjust;

}

void updateSwitch2() 
{
    unsigned int irIndex = 0;

    if (pswitch2[0] == true) {
        irIndex = 0;
    } else if (pswitch2[1] == true) {
        irIndex = 2;
    } else {
        irIndex = 1;
    }

    mIR.Init(ir_collection[irIndex]);  // ir_data is from ir_data.h
}


void updateSwitch3() 
{
    if (pswitch3[0] == true) {
        delay1.secondTapOn = false;
    } else if (pswitch3[1] == true) {
        delay1.secondTapOn = true;
        delay1.del->set2ndTapFraction(0.6666667); // triplett
    } else {
        delay1.secondTapOn = true;
        delay1.del->set2ndTapFraction(0.75); // dotted eighth
    }
}


void UpdateButtons()
{

    // (De-)Activate bypass and toggle LED when left footswitch is let go, or enable/disable amp if held for greater than 1 second //
    // Can only disable/enable amp when not in bypass mode
    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {

        bypass = !bypass;
        led1.Set(bypass ? 0.0f : 1.0f);
    }


    // Toggle Alternate mode by holding down the left footswitch, if not already in alternate mode and not in bypass
    //if(hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 500 && !alternateMode && !bypass) {
    //    alternateMode = true;
    //    led1.Set(0.5f);  // Dim LED in alternate mode
    //}


    led1.Update();
    led2.Update();

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
    //hw.ProcessAllControls();
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    UpdateButtons();
    UpdateSwitches();

    float input_arr[1] = { 0.0 };    // Neural Net Input

    float delay_out;

    // Get current knob parameters ////////////////////
    float vgain = Gain.Process();
    float vlevel = Level.Process(); 
    float vmix = Mix.Process();

    float vfilter = filter.Process();
    float vdelayTime = delayTime.Process();
    float vdelayFdbk = delayFdbk.Process();

    // Mix or tone control (Tone is alternate)


        // Set Filter Controls
    if (vfilter <= 0.5) {
        float filter_value = (vfilter * 39800.0f) + 100.0f;
        tone.SetFreq(filter_value);
    } else {
        float filter_value = (vfilter - 0.5) * 800.0f + 40.0f;
        toneHP.SetFreq(filter_value);
    }


        // Calculate mix parameters
        //    A cheap mostly energy constant crossfade from SignalSmith Blog
        //    https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/
    float x2 = 1.0 - vmix;
    float A = vmix*x2;
    float B = A * (1.0 + 1.4186 * A);
    float C = B + vmix;
    float D = B + x2;

    wetMix = C * C;
    dryMix = D * D;


    // DELAY //

    if (vdelayTime < 0.01) {   // if knob < 1%, set delay to inactive
        delay1.active = false;
    } else {
        delay1.active = true;
    }

    // From 0 to 75% knob is 0 to 1 second, 75% to 100% knob is 1 to 2 seconds (for more control over 1 second range)
    if (vdelayTime <= 0.75) {
        delay1.delayTarget = 2400 + vdelayTime * 60800; // in samples 50ms to 1 second range  // Note: changing delay time with heavy reverb creates a cool modulation effect
    } else {
        delay1.delayTarget = 48000 + (vdelayTime - 0.75) * 192000; // 1 second to 2 second range
    }
    delay1.feedback = vdelayFdbk;


    // Order of effects is:
    //           Gain -> Neural Model -> Tone -> IR -> Delay ->
    //

    for(size_t i = 0; i < size; i++)
    {

        // Process your signal here
        if(bypass)
        {
            // Could do a trails mode here
            out[0][i] = in[0][i];  
            out[1][i] = in[0][i]; 

        }
        else
        {   
            float ampOut = 0.0;
            input_arr[0] = in[0][i] * vgain;

            // Process Neural Net Model //
            if (pdip[0]) {   // Enable/Disable neural model
                ampOut = model.forward (input_arr) + input_arr[0];  
                ampOut *= nnLevelAdjust;
            } else {
                ampOut = input_arr[0];
            }

            // Process Tone
            float filter_in =  ampOut;
            float filter_out;
            float balanced_out;
            
            if (vfilter <= 0.5) {
                filter_out = tone.Process(filter_in);
                balanced_out = bal.Process(filter_out, filter_in);

            } else {
                filter_out = toneHP.Process(filter_in);
                balanced_out = bal.Process(filter_out, filter_in);
            }


            // IMPULSE RESPONSE //
            float impulse_out = 0.0;
            
            if (pdip[1]) // If IR is enabled by dip switch
            {
                impulse_out = mIR.Process(balanced_out) * 0.2;  
            } else {
                impulse_out = balanced_out; 
            }
            
            // Process Delay
            delay_out = delay1.Process(impulse_out);  
            float output = (impulse_out * dryMix + delay_out * wetMix) * vlevel *0.4; // 0.2 for level adjust
            out[0][i] = output; // Mix amp out with delay/reverb;
            out[1][i] = output; // Mix amp out with delay/reverb;

        }
    }
}

    
            





int main(void)
{
    float samplerate;

    hw.Init();
    samplerate = hw.AudioSampleRate();

    //verb.Init(samplerate);

    setupWeights();
    hw.SetAudioBlockSize(48); 

    switch1[0]= Funbox::SWITCH_1_LEFT;
    switch1[1]= Funbox::SWITCH_1_RIGHT;
    switch2[0]= Funbox::SWITCH_2_LEFT;
    switch2[1]= Funbox::SWITCH_2_RIGHT;
    switch3[0]= Funbox::SWITCH_3_LEFT;
    switch3[1]= Funbox::SWITCH_3_RIGHT;
    dip[0]= Funbox::SWITCH_DIP_1;
    dip[1]= Funbox::SWITCH_DIP_2;

    pswitch1[0]= true; // TODO I think by setting all these to true, will force loading the correct model/ir on booting the pedal - verify
    pswitch1[1]= true;
    pswitch2[0]= true;
    pswitch2[1]= true;
    pswitch3[0]= true;
    pswitch3[1]= true;
    pdip[0]= true;
    pdip[1]= true;

    //updateSwitch1();
    //updateSwitch2();
    //updateSwitch3();

    Gain.Init(hw.knob[Funbox::KNOB_1], 0.1f, 2.5f, Parameter::LINEAR);
    Mix.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    Level.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR); // lower range for quieter level
    filter.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::CUBE);
    delayTime.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    delayFdbk.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR); 

    // Initialize the correct model
    modelIndex = 0;
    nnLevelAdjust = 1.0;

    indexMod = 0;

    // Initialize & set params for mixers 
    mix_effects = 0.5;


    tone.Init(samplerate);
    toneHP.Init(samplerate);
    bal.Init(samplerate);
    vfilter = 0.5;


    delayLine.Init();
    delay1.del = &delayLine;
    delay1.delayTarget = 2400; // in samples
    delay1.feedback = 0.0;
    delay1.active = true;    

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