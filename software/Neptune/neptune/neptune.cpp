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
bool release;
int release_counter;

bool freeze;
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
    reverb->ClearBuffers();

    if (switch1[0] == true) {  
        reverb->initFactoryChorus();

    } else if (switch1[1] == true) {                       

        reverb->initFactoryMediumSpace();

    } else {                    
        reverb->initFactoryRubiKaFields();

    } 
    reverb->SetParameter(::Parameter2::LineCount, 2);
    force_reset = true; // This allows the knob controlled params to be reset without moving the knobs, when a new mode is selected
}

void updateSwitch3() 
{
    // DELAY ///////////////////////
    if (pswitch3[0]) {                // If switch3 is left, Normal FWD mode
        delayL.active = true;
        delayR.active = true;
        delayL.del->setOctave(false); 
        delayR.del->setOctave(false);
        delayL.reverseMode = false;
        delayR.reverseMode = false;


    } else if (pswitch3[1]) {         // If switch3 is right, REV mode
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
        expression_pressed = false; // TODO Verify if the switch holding for expression interferes with Bypass/Freeze/Alternate functionality
    } 

    // Toggle Alternate mode by holding down the left footswitch, if not already in alternate mode and not in bypass
    if(hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 500 && !alternateMode && !bypass && !freeze  && !hw.switches[Funbox::FOOTSWITCH_2].Pressed()) {
        alternateMode = true;
        led1.Set(0.5f);  // Dim LED in alternate mode
    }


    // If switch2 is pressed, freeze the current delay by not applying new delay and setting feedback to 1.0, set to false by default
    if(hw.switches[Funbox::FOOTSWITCH_2].RisingEdge() && !hw.switches[Funbox::FOOTSWITCH_1].Pressed())
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
    //bool changed2 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch2[i]].Pressed() != pswitch2[i]) {
            pswitch2[i] = hw.switches[switch2[i]].Pressed();
            //changed2 = true;
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
    for(int i=0; i<4; i++) {
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


    // Knob and Expression Processing ////////////////////

    float knobValues[6];
    float newExpressionValues[6];

    knobValues[0] = rsize.Process();
    knobValues[1] = mix.Process();
    knobValues[2] = delayTime.Process();

    knobValues[3] = modify.Process();
    knobValues[4] = filter.Process();
    knobValues[5] = delayFDBK.Process();

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


    if (freeze) {
        delayL.feedback = del_FDBK_Override;
        delayR.feedback = del_FDBK_Override;

    } else {
        delayL.feedback = vdelayFDBK;
        delayR.feedback = vdelayFDBK;

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
        if ((!alternateMode && !freeze && !release) || force_reset == true) {

            if (pswitch1[0] == true) {  // Chorus
                reverb->SetParameter(::Parameter2::LineDecay, (vsize)); 

            } else if (pswitch1[1] == true) {  // Hall
                reverb->SetParameter(::Parameter2::LineDecay, (vsize)); 

            } else {  // Cloud
                reverb->SetParameter(::Parameter2::LineDecay, (vsize)); 

            }
            
            psize = vsize;

        } else if (alternateMode && knobMoved(psize, vsize) && !freeze) {  
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

        } else if (alternateMode && knobMoved(pmodify, vmodify)) {
            modAlt = vmodify;
            reverb->SetParameter(::Parameter2::LineModRate, modAlt); // Range 0 to ~1
            pmodify = vmodify;
        }
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

            delay_outL = (delayL.Process(delay_inL)  * delayMix);
            delay_outR = (delayR.Process(delay_inR) * delayMix);


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


            float balanced_outL;
            float balanced_outR;

            if (!pswitch2[1]) {  // If switch3 not down (if either up or middle position) add delay
                balanced_outL =  (outL[0] * (reverbMix * 1.2) + delay_outL) * wetMix; // Apply mix and level controls, slight reverb boost
                balanced_outR =  (outR[0] * (reverbMix * 1.2) + delay_outR) * wetMix;
            } else {
                balanced_outL =  (outL[0] * (reverbMix * 1.2))  * wetMix; // Apply mix and level controls, slight reverb boost
                balanced_outR =  (outR[0] * (reverbMix * 1.2))  * wetMix;
            }


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

    freeze = false;
    release = false;
    release_counter = 0;
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

    // Expression
    expHandler.Init(6);
    expression_pressed = false;

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