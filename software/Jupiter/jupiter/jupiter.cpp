// Neptune Reverb/Delay Pedal

#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"
#include "expressionHandler.h"

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
::daisy::Parameter rdecay, mix, lowfreq, lowshelf, highfreq, highshelf, expression;
bool      bypass;
Led led1, led2;


float dryMix;
float wetMix;

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

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];

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

void updateSwitch1()  // TODO: Tune these settings so that they make 3 ear pleasing stages of "Lushness"
{

    if (toggleValues[0] == 0) {

        reverb->SetParameter(::Parameter2::LateDiffusionEnabled, 0.0);
        reverb->SetParameter(::Parameter2::LateDiffusionStages,  0.4);

        reverb->SetParameter(::Parameter2::LateStageTap, 0.0);
        reverb->SetParameter(::Parameter2::Interpolation, 0.0);

        reverb->SetParameter(::Parameter2::LineCount, 1); 

    } else if (toggleValues[0] == 2) {

        reverb->SetParameter(::Parameter2::LateDiffusionEnabled, 1.0);
        reverb->SetParameter(::Parameter2::LateDiffusionStages, 1.0);

        reverb->SetParameter(::Parameter2::LateStageTap, 1.0);
        reverb->SetParameter(::Parameter2::Interpolation, 1.0);

        reverb->SetParameter(::Parameter2::LineCount, 2); 

    } else {

        reverb->SetParameter(::Parameter2::LateDiffusionEnabled, 1.0);
        reverb->SetParameter(::Parameter2::LateDiffusionStages, 0.2);

        reverb->SetParameter(::Parameter2::LateStageTap, 1.0);
        reverb->SetParameter(::Parameter2::Interpolation, 0.0);

        reverb->SetParameter(::Parameter2::LineCount, 2); 

    }

}


void updateSwitch2() 
{

    if (toggleValues[1] == 0) {
        reverb->SetParameter(::Parameter2::LineModAmount, 0.1); 
        reverb->SetParameter(::Parameter2::LineModRate, 0.1);
    } else if (toggleValues[1] == 2) {
        reverb->SetParameter(::Parameter2::LineModAmount, 0.9); 
        reverb->SetParameter(::Parameter2::LineModRate, 0.6); 
    } else {
        reverb->SetParameter(::Parameter2::LineModAmount, 0.5); 
        reverb->SetParameter(::Parameter2::LineModRate, 0.3); 
    }

}



void updateSwitch3() 
{
    if (toggleValues[2] == 0) {
        reverb->SetParameter(::Parameter2::PreDelay, 0.0);
    } else if (toggleValues[2] == 2) {
        reverb->SetParameter(::Parameter2::PreDelay, 0.2); // 200ms
    } else {
        reverb->SetParameter(::Parameter2::PreDelay, 0.1); // 100ms
    }
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
            changed4 = true;
            // Action for dipswitches handled in audio callback
        }
    }
    // Update if preset turned off
    if (changed4 || update_switches) {
        for (int i=0; i<4; i++) {
           dipValues[i] = pdip[i];    // TODO Check logic here
        }
    }

    update_switches = false; // only update once after turning off preset
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


    // Knob and Expression Processing ////////////////////

    // float knobValues[6]; // moved to global
    float newExpressionValues[6];

    if (!use_preset) {  // TODO Do I want to lock out the knobs when using a preset?

        // Knob 1
        if (!midi_control[0])   // If not under midi control, use knob ADC
            pknobValues[0] = knobValues[0] = rdecay.Process();
        else if (knobMoved(pknobValues[0], rdecay.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[0] = false;

        // Knob 2
        if (!midi_control[1])   // If not under midi control, use knob ADC
            pknobValues[1] = knobValues[1] = lowfreq.Process();
        else if (knobMoved(pknobValues[1], lowfreq.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[1] = false;

        // Knob 3
        if (!midi_control[2])   // If not under midi control, use knob ADC
            pknobValues[2] = knobValues[2] = highfreq.Process();
        else if (knobMoved(pknobValues[2], highfreq.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[2] = false;

        // Knob 4
        if (!midi_control[3])   // If not under midi control, use knob ADC
            pknobValues[3] = knobValues[3] = mix.Process();
        else if (knobMoved(pknobValues[3], mix.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[3] = false;
    

        // Knob 5
        if (!midi_control[4])   // If not under midi control, use knob ADC
            pknobValues[4] = knobValues[4] = lowshelf.Process();
        else if (knobMoved(pknobValues[4], lowshelf.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[4] = false;
    

        // Knob 6
        if (!midi_control[5])   // If not under midi control, use knob ADC
            pknobValues[5] = knobValues[5] = highshelf.Process();
        else if (knobMoved(pknobValues[5], highshelf.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[5] = false;

    }

    float vexpression = expression.Process(); // 0 is heel (up), 1 is toe (down)
    expHandler.Process(vexpression, knobValues, newExpressionValues);


    // If in expression set mode, set LEDS accordingly
    if (expHandler.isExpressionSetMode()) {
        led1.Set(expHandler.returnLed1Brightness());
        led2.Set(expHandler.returnLed2Brightness());
    }
  

    float vrdecay = newExpressionValues[0];
    float vlowfreq = newExpressionValues[1];
    float vhighfreq = newExpressionValues[2];
    float vmix = newExpressionValues[3];
    float vlowshelf = newExpressionValues[4];
    float vhighshelf = newExpressionValues[5];


    if (pmix != vmix) {
        if (knobMoved(pmix, vmix)) {
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


    // Set Reverb Parameters ///////////////
    if (knobMoved(prdecay, vrdecay)) {

        reverb->SetParameter(::Parameter2::LineDecay, (vrdecay * 0.65 + 0.35)); // Range 0.35 to 1.0
        prdecay = vrdecay;
    }

    if (knobMoved(plowfreq, vlowfreq)) {

        reverb->SetParameter(::Parameter2::PostLowShelfFrequency, vlowfreq);
        plowfreq = vlowfreq;
    }

    if (knobMoved(plowshelf, vlowshelf)) {

        reverb->SetParameter(::Parameter2::PostLowShelfGain, vlowshelf);
        plowshelf = vlowshelf;
    }

    if (knobMoved(phighfreq, vhighfreq)) {

        reverb->SetParameter(::Parameter2::PostHighShelfFrequency, vhighfreq);
        phighfreq = vhighfreq;
    }

    if (knobMoved(phighshelf, vhighshelf)) {

        reverb->SetParameter(::Parameter2::PostHighShelfGain, vhighshelf * 0.7 + 0.3);
        phighshelf = vhighshelf;
    }

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
            if (dipValues[0]) {  // Stereo
                inputL = in[0][i];
                inputR = in[1][i];
            } else {     //MISO
                inputL = in[0][i];
                inputR = in[0][i];
            }
 

            inL[0] = inputL;
            inR[0] = inputR;


            reverb->Process(inL, inR, outL, outR, 1); 
            // Bears. Beets. Battlestar Galactica.
            out[0][i] = inputL * dryMix + outL[0] * wetMix;
            out[1][i] = inputR * dryMix + outR[0] * wetMix;


        }

    } else {
        for (size_t i = 0; i < size; i++)
        {
            // Stereo or MISO 
            if (dipValues[0]) {  // Stereo
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


// Typical Switch case for Message Type.
void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case NoteOn:
        {
 
            //led2.Set(1.0); // TODO Simple test to see if midi note is detected
            //led2.Update();
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


    hw.SetAudioBlockSize(48);

    rdecay.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, ::daisy::Parameter::LINEAR); // LOG to go high fast?
    lowfreq.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    highfreq.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    mix.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    lowshelf.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    highshelf.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    expression.Init(hw.expression, 0.0f, 1.0f, Parameter::LINEAR); 


    pmix = 0.5;
    plowfreq = 0.0;
    prdecay = 0.0;
    plowshelf = 0.0;
    phighfreq = 0.0;
    phighshelf = 0.0;


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
    // index for midi_control: 0-5 knobs, 6 expression, 7-9 switch1, 10-12 switch2, 13-15 switch 3
    //                         TODO Dipswitches over midi 16-17 Dip1, 17-18 Dip2, 19-20 Dip3, 21-22 Dip4,


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

        hw.midi.Listen();
        // Handle MIDI Events
        while(hw.midi.HasEvents())  // MIDI is not working for some reason, TODO figure out why??
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
}