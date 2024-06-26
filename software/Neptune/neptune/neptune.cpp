// Neptune Reverb/Delay Pedal

#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"
#include "delayline_reverse.h"
#include "delayline_oct.h"
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
::daisy::Parameter rsize, modify, mix, filter, delayTime, delayFDBK, expression;
bool      bypass;
Led led1, led2;

//Tone tone;       // Low Pass
Tone toneOctLP;       // Low Pass for octave delay
//ATone toneHP;    // High Pass
float pfilter;

// Alternate functions
bool alternateMode;

float delayMix;   // Mix vs. Delay Level
float reverbMix;
float dryMix;
float wetMix;

float modAlt;  // Secondary modify function
float preDelayAlt;

bool force_reset;

float  del_FDBK_Override;

float ramp;
bool mode_changed;
bool triggerMode;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];

// Initialize "previous" p values
float psize, pmodify, pdelayTime, pmix;


// Expression
ExpressionHandler expHandler;
bool expression_pressed;

// Midi
bool midi_control[6]; //  just knobs for now
float pknobValues[6]; // Used for Midi control logic

float knobValues[6];
int toggleValues[3];
bool dipValues[4];


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
#define MAX_DELAY_REV static_cast<size_t>(48000 * 8.0f) // 4 second max delay (double for reverse)

DelayLineReverse<float, MAX_DELAY_REV> DSY_SDRAM_BSS delayLineRevLeft;
DelayLineOct<float, MAX_DELAY> DSY_SDRAM_BSS delayLineOctLeft;
DelayLineReverse<float, MAX_DELAY_REV> DSY_SDRAM_BSS delayLineRevRight;
DelayLineOct<float, MAX_DELAY> DSY_SDRAM_BSS delayLineOctRight;


struct delay
{
    DelayLineOct<float, MAX_DELAY> *del;
    DelayLineReverse<float, MAX_DELAY_REV> *delreverse;
    float                        currentDelay;
    float                        delayTarget;
    float                        feedback = 0.0;
    float                        active = false;
    bool                         reverseMode = false;
    float                        level = 1.0;      // Level multiplier of output, added for stereo modulation
    float                        level_reverse = 1.0;      // Level multiplier of output, added for stereo modulation
    bool                         dual_delay = false;
    bool                         secondTapOn = false;
    
    float Process(float in)
    {
        //set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        del->SetDelay(currentDelay);
        delreverse->SetDelay1(currentDelay);

        float del_read = del->Read();

        float read_reverse = delreverse->ReadRev(); // REVERSE

        float read = toneOctLP.Process(del_read);  // LP filter, tames harsh high frequencies on octave, has fading effect for normal/reverse

        float secondTap = 0.0;
        if (secondTapOn) {
            secondTap = del->ReadSecondTap();
        }
        //float read2 = delreverse->ReadFwd();
        if (active) {
            del->Write((feedback * read) + in);
            delreverse->Write((feedback * read) + in);  // Writing the read from fwd/oct delay line allows for combining oct and rev for reverse octave!
            //delreverse->Write((feedback * read2) + in); 
        } else {
            del->Write(feedback * read); // if not active, don't write any new sound to buffer
            delreverse->Write(feedback * read);
            //delreverse->Write((feedback * read2));
        }

        if (dual_delay) {
            return read_reverse * level_reverse * 0.5 + (read + secondTap) * level * 0.5; // Half the volume to keep total level consistent
        } else if (reverseMode) {
            return read_reverse * level_reverse;
        } else {
            return (read + secondTap) * level;
        }
    }
};


// Left channel
delay             delayL;
delay             delayR;  


// Setting Struct containing parameters we want to save to flash
// Using the persistent storage example found on the Daisy Forum:
//   https://forum.electro-smith.com/t/saving-values-to-flash-memory-using-persistentstorage-class-on-daisy-pod/4306
struct Settings {

        float knobs[6]; // TODO Add values for alternate parameters
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
bool update_switches;

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

void updateSwitch1()
{
    // For the different modes, write a preset in the CloudSeed code, and switch presets here
    reverb->ClearBuffers();

    if (toggleValues[0] == 0) { 
        reverb->initFactoryChorus();

    } else if (toggleValues[0] == 2) {
        reverb->initFactoryRubiKaFields();

    } else {                    
        reverb->initFactoryMediumSpace();

    } 
    reverb->SetParameter(::Parameter2::LineCount, 2);
    force_reset = true; // This allows the knob controlled params to be reset without moving the knobs, when a new mode is selected
}

void updateSwitch3() 
{
    // DELAY ///////////////////////
    if (toggleValues[2] == 0) {                // If switch3 is left, Normal FWD mode
        delayL.active = true;
        delayR.active = true;
        delayL.del->setOctave(false); 
        delayR.del->setOctave(false);
        delayL.reverseMode = false;
        delayR.reverseMode = false;


    } else if (toggleValues[2] == 2) {        // If switch3 is right, REV mode
        delayL.active = true;
        delayR.active = true;
        delayL.del->setOctave(false); 
        delayR.del->setOctave(false);
        delayL.reverseMode = true;
        delayR.reverseMode = true;

    } else {                           // If switch3 is middle, Octave mode
        delayL.active = true;
        delayR.active = true;
        delayL.del->setOctave(true); 
        delayR.del->setOctave(true);
        delayL.reverseMode = false;
        delayR.reverseMode = false;
    }
}


void UpdateButtons()
{
    // (De-)Activate bypass and toggle LED when left footswitch is let go
    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {
        if(!expression_pressed) {
            if (alternateMode) {
                alternateMode = false;
                force_reset = true; // force parameter reset to enforce current knob settings when leaving alternate mode
                led1.Set(1.0f);
            } else {
                bypass = !bypass;
                led1.Set(bypass ? 0.0f : 1.0f);
            }
        }
        expression_pressed = false; // TODO Verify if the switch holding for expression interferes with Bypass/Alternate functionality
    } 

    // Toggle Alternate mode by holding down the left footswitch, if not already in alternate mode and not in bypass
    if(hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 600 && !alternateMode && !bypass && !hw.switches[Funbox::FOOTSWITCH_2].Pressed() && !expression_pressed) { // TODO Check logic here
        alternateMode = true;
        led1.Set(0.5f);  // Dim LED in alternate mode
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
        //updateSwitch2();
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
        mode_changed = true; // This will trigger calling updateSwitch1() after a waiting time
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
        //updateSwitch2(); // action for routing toggle handled in the audio callback
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

    if (mode_changed) {
        if (ramp > 0.95) { // Helps prevent mode from changing too quickly and overloading
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


    // Knob and Expression Processing ////////////////////

    //float knobValues[6];
    float newExpressionValues[6];


    if (!use_preset) {  // TODO Do I want to lock out the knobs when using a preset?

        // Knob 1
        if (!midi_control[0])   // If not under midi control, use knob ADC
            pknobValues[0] = knobValues[0] = rsize.Process();
        else if (knobMoved(pknobValues[0], rsize.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[0] = false;

        // Knob 2
        if (!midi_control[1])   // If not under midi control, use knob ADC
            pknobValues[1] = knobValues[1] = mix.Process();
        else if (knobMoved(pknobValues[1], mix.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[1] = false;

        // Knob 3
        if (!midi_control[2])   // If not under midi control, use knob ADC
            pknobValues[2] = knobValues[2] = delayTime.Process();
        else if (knobMoved(pknobValues[2], delayTime.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[2] = false;

        // Knob 4
        if (!midi_control[3])   // If not under midi control, use knob ADC
            pknobValues[3] = knobValues[3] = modify.Process();
        else if (knobMoved(pknobValues[3], modify.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[3] = false;
    

        // Knob 5
        if (!midi_control[4])   // If not under midi control, use knob ADC
            pknobValues[4] = knobValues[4] = filter.Process();
        else if (knobMoved(pknobValues[4], filter.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[4] = false;
    

        // Knob 6
        if (!midi_control[5])   // If not under midi control, use knob ADC
            pknobValues[5] = knobValues[5] = delayFDBK.Process();
        else if (knobMoved(pknobValues[5], delayFDBK.Process()))  // If midi controlled, watch for knob movement to end Midi control
            midi_control[5] = false;

    }


    float vexpression = expression.Process(); // 0 is heel (up), 1 is toe (down)
    expHandler.Process(vexpression, knobValues, newExpressionValues);


    // If in expression set mode, set LEDS accordingly
    if (expHandler.isExpressionSetMode()) {
        led1.Set(expHandler.returnLed1Brightness());
        led2.Set(expHandler.returnLed2Brightness());
    }
  

    float vsize = newExpressionValues[0];
    float vmix = newExpressionValues[1];
    float vdelayTime = newExpressionValues[2];
    float vmodify = newExpressionValues[3];
    float vfilter = newExpressionValues[4];
    float vdelayFDBK = newExpressionValues[5];


    if (knobMoved(pmix, vmix) || force_reset == true) {
        // Handle Normal or Alternate Mode Mix Controls
        //    A cheap mostly energy constant crossfade from SignalSmith Blog
        float x2 = 1.0 - vmix;
        float A = vmix*x2;
        float B = A * (1.0 + 1.4186 * A);
        float C = B + vmix;
        float D = B + x2;

        if (!alternateMode) {
            wetMix = C * C;
            dryMix = D * D;

        } else {    //// UPDATE   //// UPDATE   //// UPDATE
            delayMix = C * C;
            reverbMix = D * D;

        }
        pmix = vmix;
    }

    if (knobMoved(pfilter, vfilter)) {
        reverb->SetParameter(::Parameter2::PostCutoffFrequency, vfilter); // TODO maybe do something with the shelf filters as well
        pfilter = vfilter;
    }


    //float fvdelayTime = 1.0 - vdelayTime; //Invert parameter for correct knob operation

    if (vdelayTime <= 0.01) {   // if knob < 1%, set delay to inactive
        delayL.active = false;
        delayR.active = false;

    } else {
        delayL.active = true;
        delayR.active = true;
    }


    delayL.delayTarget = 2400 + vdelayTime * 189600; 
    delayR.delayTarget = 2400 + vdelayTime * 189600;   

    delayL.feedback = vdelayFDBK;
    delayR.feedback = vdelayFDBK;


    // Set Reverb Parameters ///////////////
    if (knobMoved(psize, vsize) || force_reset == true)
    {
        if (!alternateMode) {

            reverb->SetParameter(::Parameter2::LineDecay, (vsize));      

        } else {  
            preDelayAlt = vsize;
            reverb->SetParameter(::Parameter2::PreDelay, preDelayAlt);
        }
        psize = vsize;
    }

    if ((knobMoved(pmodify, vmodify) || force_reset == true)) // Force reset to apply knob params when switching reverb modes, or when alternate mode released
    {
        if (!alternateMode) {
            reverb->SetParameter(::Parameter2::LineModAmount, vmodify); // Range 0 to 1

        } else {
            modAlt = vmodify;
            reverb->SetParameter(::Parameter2::LineModRate, modAlt); // Range 0 to ~1

        }
        pmodify = vmodify;
    }

    force_reset = false;

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
 
            delay_inL = inputL;  // Process any samplerate reduction for input to delay and reverb
            delay_inR = inputR;

            delay_outL = (delayL.Process(delay_inL)  * delayMix);
            delay_outR = (delayR.Process(delay_inR) * delayMix);


            // Delay and Reverb Routing based on Switch3 Position
            //        Up=Delay + and into Reverb     Middle= Delay and Reverb in Parallel  Down= Delay into Reverb

            if ((toggleValues[1] == 0) || (toggleValues[1] == 2)) {  // Up or Down Delay into reverb
                inL[0] = delay_inL + delay_outL;
                inR[0] = delay_inR + delay_outR;

            } else { // Middle position process R and D in parallel
                inL[0] = inputL;
                inR[0] = inputR;
            }


            reverb->Process(inL, inR, outL, outR, 1); // Try doing the 1 sample like in effects suite, then just have one sample loop


            float balanced_outL;
            float balanced_outR;

            if (!(toggleValues[1] == 2)) {  // If switch3 not down (if either up or middle position) add delay
                balanced_outL =  (outL[0] * (reverbMix * 1.2) + delay_outL) * wetMix; // Apply mix and level controls, slight reverb boost
                balanced_outR =  (outR[0] * (reverbMix * 1.2) + delay_outR) * wetMix;
            } else {
                balanced_outL =  (outL[0] * (reverbMix * 1.2))  * wetMix; // Apply mix and level controls, slight reverb boost
                balanced_outR =  (outR[0] * (reverbMix * 1.2))  * wetMix;
            }


            out[0][i] = inputL * dryMix + balanced_outL * wetMix;
            out[1][i] = inputR * dryMix + balanced_outR * wetMix;

            if (ramp < 1.0) {  // Volume ramp when changing reverb modes (to prevent audible click)
                ramp += 0.001;
                out[0][i] = 0.0;
                if (ramp > 0.1 && triggerMode == true) {
                    triggerMode = false;
                    updateSwitch1();
                    mode_changed = false;
                }

            }
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
    updateSwitch1();
    using namespace CloudSeed;
    reverb->SetParameter(::Parameter2::PreDelay, 0.0);  // Set initial predelay to 0.0 (controlled by Alt param)

    hw.SetAudioBlockSize(48);

    rsize.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    modify.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);

    delayTime.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    delayFDBK.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);

    mix.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    filter.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, ::daisy::Parameter::CUBE);
    expression.Init(hw.expression, 0.0f, 1.0f, Parameter::LINEAR); 

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


    delayLineRevLeft.Init();
    delayLineOctLeft.Init();
    delayL.del = &delayLineOctLeft;
    delayL.delreverse = &delayLineRevLeft;
    delayL.delayTarget = 2400; // in samples
    delayL.feedback = 0.0;
    delayL.active = true;     // Default to no delay

    delayLineRevRight.Init();
    delayLineOctRight.Init();
    delayR.del = &delayLineOctRight;
    delayR.delreverse = &delayLineRevRight;
    delayR.delayTarget = 2400; // in samples
    delayR.feedback = 0.0;
    delayR.active = true;     // Default to no delay

    toneOctLP.Init(samplerate);
    toneOctLP.SetFreq(3000.0);

    pfilter = 0.5;

    force_reset = false;

    ramp = 0.0;
    triggerMode = false;

    updateSwitch3();

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

    update_switches = true;

    // Midi
    for( int i = 0; i < 6; ++i ) 
        midi_control[i] = false;  

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

    hw.InitMidi();
    hw.midi.StartReceive();

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