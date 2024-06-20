#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"
#include <RTNeural/RTNeural.h>  // NOTE: Need to use older version of RTNeural, same as GuitarML/Seed
#include "delayline_2tap.h"
#include "ImpulseResponse/ImpulseResponse.h"
#include "expressionHandler.h"


// Model Weights (edit this file to add model weights trained with Colab script)
//    The models must be GRU (gated recurrent unit) with hidden size = 9, snapshot models (not condidtioned on a parameter)
#include "all_model_data_gru9_4count.h"
#include "ImpulseResponse/ir_data.h"


using namespace daisy;
using namespace daisysp;
using namespace funbox;  // This is important for mapping the correct controls to the Daisy Seed on Funbox PCB

// Declare a local daisy_petal for hardware access
DaisyPetal hw;
Parameter Gain, Level, Mix, filter, delayTime, delayFdbk, expression;
bool            bypass;
int             modelInSize;
unsigned int    modelIndex;
bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];
float           nnLevelAdjust;
int             indexMod;

float knobValues[6]; // Moved to global
int toggleValues[3];
bool dipValues[4];

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

// Expression
ExpressionHandler expHandler;
bool expression_pressed;


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

//TODO for preset, currently the pedal freezes if the preset hasnt been saved to QPSI before on the Daisy Seed, fix
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
        //return !(a.p1==p1 && a.p2==p2 && a.p3==p3 && a.p4==p4 && a.p5==p5 && a.p6==p6);
    }
};

//Persistent Storage Declaration. Using type Settings and passed the devices qspi handle
PersistentStorage<Settings> SavedSettings(hw.seed.qspi);
bool use_preset = false;
bool trigger_save = false;
int blink = 100;
bool save_check = false;
bool update_switches = false;



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



void updateSwitch1() 
{
    //int modelIndex = 0;

    /*
    if (pswitch1[0] == true) {
        modelIndex = 1;
    } else if (pswitch1[1] == true) {
        modelIndex = 3;
    } else {
        modelIndex = 2;
    }
    */

    int modelIndex = toggleValues[0] + 1;

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
    int irIndex = toggleValues[1];
    mIR.Init(ir_collection[irIndex]);  // ir_data is from ir_data.h
}


void updateSwitch3() 
{
    if (toggleValues[2] == 0) {
        delay1.secondTapOn = false;

    } else if (toggleValues[2] == 2) {
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
        if (!expression_pressed) {
            //  Don't ever, for any reason, do anything to anyone for any reason ever, no matter what, no matter where, or who, or who you are with, or where you are going, or where you've been... ever, for any reason whatsoever...
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
    if(hw.switches[Funbox::FOOTSWITCH_2].TimeHeldMs() >= 700 && !save_check && !expression_pressed && hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() <= 50)  // TODO Check that this logic keeps peset separate from expression
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
    // Detect any changes in switch positions

    // 3-way Switch 1
    bool changed1 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch1[i]].Pressed() != pswitch1[i]) {
            pswitch1[i] = hw.switches[switch1[i]].Pressed();
            changed1 = true;
        }
    }
    if (changed1 || update_switches) { // update_switches is for turning off preset
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
    if (changed2 || update_switches) {
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
    if (changed3 || update_switches) {
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
            dipValues[i] = pdip[i]; // TODO Look into consolidating logic for dipValues, pdip, etc (this is for preset saving)
            bool changed4 = true;
            // Action for dipswitches handled in audio callback
        }
    }
    // Update if preset turned off  
    if (changed4 || update_switches) {
        for (int i=0; i<4; i++) {
           dipValues[i] = pdip[i];   // TODO Check logic here
        }
    }

    update_switches = false; // only update once after turning off preset
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


    // Knob and Expression Processing ////////////////////

    float newExpressionValues[6];

    if (!use_preset) {  // TODO Do I want to lock out the knobs when using a preset?
        knobValues[0] = Gain.Process();
        knobValues[1] = Mix.Process();
        knobValues[2] = Level.Process(); 

        knobValues[3] = filter.Process();
        knobValues[4] = delayTime.Process();
        knobValues[5] = delayFdbk.Process();
    }

    float vexpression = expression.Process(); // 0 is heel (up), 1 is toe (down)
    expHandler.Process(vexpression, knobValues, newExpressionValues);


    // If in expression set mode, set LEDS accordingly
    if (expHandler.isExpressionSetMode()) {
        led1.Set(expHandler.returnLed1Brightness());
        led2.Set(expHandler.returnLed2Brightness());
    }
  
    float vgain = newExpressionValues[0];
    float vmix = newExpressionValues[1];
    float vlevel = newExpressionValues[2];
    float vfilter = newExpressionValues[3];
    float vdelayTime = newExpressionValues[4];
    float vdelayFdbk = newExpressionValues[5];

    //float vlevel = vexpression; // TESTING

    // Mix and tone control

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
    //           Gain -> Neural Model -> Tone -> Delay -> IR ->
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
            if (dipValues[0]) {   // Enable/Disable neural model
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

            delay_out = delay1.Process(balanced_out);   // Moved delay prior to IR

            // IMPULSE RESPONSE //
            float impulse_out = 0.0;
            
            if (dipValues[1]) // If IR is enabled by dip switch
            {
                impulse_out = mIR.Process(balanced_out * dryMix + delay_out * wetMix) * 0.2;  
            } else {
                impulse_out = balanced_out * dryMix + delay_out * wetMix; 
            }
            
            // Process Delay
            float output = impulse_out * vlevel * 0.4; // 0.4 for level adjust
            out[0][i] = output; 
            out[1][i] = output;

        }
    }
}



int main(void)
{
    float samplerate;

    hw.Init();
    samplerate = hw.AudioSampleRate();

    setupWeights();
    hw.SetAudioBlockSize(48); // Up to 256 reduces stuttering when using 2 tap delay (due to processing)

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

    pswitch1[0]= true; // TODO I think by setting all these to true, will force loading the correct model/ir on booting the pedal - verify
    pswitch1[1]= true;
    pswitch2[0]= true;
    pswitch2[1]= true;
    pswitch3[0]= true;
    pswitch3[1]= true;
    pdip[0]= true;
    pdip[1]= true;
    pdip[2]= true;
    pdip[3]= true;


    Gain.Init(hw.knob[Funbox::KNOB_1], 0.1f, 2.5f, Parameter::LINEAR);
    Mix.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    Level.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR); // lower range for quieter level
    filter.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::CUBE);
    delayTime.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    delayFdbk.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR); 
    expression.Init(hw.expression, 0.0f, 1.0f, Parameter::LINEAR); // TODO Make sure this is the correct way to reference expression

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

    // Expression
    expHandler.Init(6);
    expression_pressed = false;

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

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while(1)
    {
        if(trigger_save) {
			
	    SavedSettings.Save(); // Writing locally stored settings to the external flash
	    trigger_save = false;
            blink = 0;
	}
	System::Delay(100);

    }
}