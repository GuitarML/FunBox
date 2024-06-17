#include "daisy_petal.h"
#include "daisysp.h"
#include "varSpeedLooper.h"
#include "funbox.h"
#include "reverbsc96.h"
#include "expressionHandler.h"

//
// This is a template for creating a pedal on the GuitarML Funbox_v1/Daisy Seed platform.
// You can start from here to fill out your effects processing and controls.
// Allows for Stereo In/Out, 6 knobs, 3 3-way switches, 2 dipswitches, 2 SPST Footswitches, 2 LEDs.
//
// Keith Bloemer 5/20/2024
//

using namespace daisy;
using namespace daisysp;
using namespace funbox;  // This is important for mapping the correct controls to the Daisy Seed on Funbox PCB

// Declare a local daisy_petal for hardware access
DaisyPetal hw;
Parameter levelA, modA, levelB, speedA, modB, speedB, expression;

// Looper Parameters
#define MAX_SIZE (96000 * 60) // 1 minute of floats at 96 khz
float DSY_SDRAM_BSS bufA[MAX_SIZE];
varSpeedLooper looperA;
Oscillator      led_oscA; // For pulsing the led when recording / paused playback
float           ledBrightnessA;
int             doubleTapCounterA;
bool            checkDoubleTapA;
bool            pausePlaybackA;
float           currentSpeedA;

float DSY_SDRAM_BSS bufB[MAX_SIZE];
varSpeedLooper looperB;
Oscillator      led_oscB; // For pulsing the led when recording / paused playback
float           ledBrightnessB;
int             doubleTapCounterB;
bool            checkDoubleTapB;
bool            pausePlaybackB;
float           currentSpeedB;

// Reverb
ReverbSc96        verb;  // Minor change to ReverbSc to use 96kHz default samplerate

Tone toneA;       // Low Pass
ATone toneHPA;    // High Pass
Balance balA;     // Balance for volume correction in filtering

Tone toneB;       // Low Pass
ATone toneHPB;    // High Pass
Balance balB;     // Balance for volume correction in filtering

SmoothRandomGenerator smoothRandA;
SmoothRandomGenerator smoothRandB;

bool            bypass;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];


Led led1, led2;


// Expression
ExpressionHandler expHandler;
bool expression_pressed;


void updateSwitch1() // left=, center=, right=
{
    if (pswitch1[0] == true) {  // left

    } else if (pswitch1[1] == true) {  // right


    } else {   // center

    }      
}

void updateSwitch2() // left=, center=, right=
{
    if (pswitch2[0] == true) {  // left

    } else if (pswitch2[1] == true) {  // right


    } else {   // center

    }    
}


void updateSwitch3() // left=, center=, right=
{
    if (pswitch3[0] == true) {  // left
        looperA.SetMode(varSpeedLooper::Mode::NORMAL);
        looperB.SetMode(varSpeedLooper::Mode::NORMAL);

    } else if (pswitch3[1] == true) {  // right

        looperA.SetMode(varSpeedLooper::Mode::FRIPPERTRONICS);
        looperB.SetMode(varSpeedLooper::Mode::FRIPPERTRONICS);

    } else {   // center
        looperA.SetMode(varSpeedLooper::Mode::ONETIME_DUB);
        looperB.SetMode(varSpeedLooper::Mode::ONETIME_DUB);

    }    
}


void UpdateButtons()
{

    // LOOPER A //
    // Looper footswitch pressed (start/stop recording, doubletap to pause/unpause playback)
    if (hw.switches[Funbox::FOOTSWITCH_1].RisingEdge())
    {
        if (!pausePlaybackA) {
            looperA.TrigRecord();
            if (!looperA.Recording()) {  // Turn on LED if not recording and in playback
                led1.Set(1.0f);
            }
         
        }

        // Start or end double tap timer
        if (checkDoubleTapA) {
            // if second press comes before 1.0 seconds, pause playback
            if (doubleTapCounterA <= 1000) {
                if (looperA.Recording()) {  // Ensure looper is not recording when double tapped (in case it gets double tapped while recording)
                    looperA.TrigRecord();
                }
                pausePlaybackA = !pausePlaybackA;
                if (pausePlaybackA) {        // Blink LED if paused, otherwise set to triangle wave for pulsing while recording
                    led_oscA.SetWaveform(4); // WAVE_SIN = 0, WAVE_TRI = 1, WAVE_SAW = 2, WAVE_RAMP = 3, WAVE_SQUARE = 4
                } else {
                    led_oscA.SetWaveform(1); 
                }
                doubleTapCounterA = 0;    // reset double tap here also to prevent weird behaviour when triple clicked
                checkDoubleTapA = false;
                led1.Set(1.0f);
            }
        } else {
            checkDoubleTapA = true;
        }
    }

    if (checkDoubleTapA) {
        doubleTapCounterA += 1;          // Increment by 1 (48000 * 0.75)/blocksize = 1000   (blocksize is 48)
        if (doubleTapCounterA > 1000) {  // If timer goes beyond 1.0 seconds, stop double tap checking
            doubleTapCounterA = 0;
            checkDoubleTapA = false;
        }
    }

    // If switch1 is held, clear the looper and turn off LED
    if(hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 1000)
    {
        pausePlaybackA = false;
        led_oscA.SetWaveform(1); 
        looperA.Clear();
        led1.Set(0.0f);
    } 




    // LOOPER B //
    // Looper footswitch pressed (start/stop recording, doubletap to pause/unpause playback)


    if ((hw.switches[Funbox::FOOTSWITCH_2].RisingEdge() && !pdip[0]) || (hw.switches[Funbox::FOOTSWITCH_1].RisingEdge() && pdip[0]) ) // TODO, verify this logic runs both loopers in the same way when in stereo mode
    {
        if (!pausePlaybackB) {
            looperB.TrigRecord();
            if (!looperB.Recording()) {  // Turn on LED if not recording and in playback
                led2.Set(1.0f);
            }
         
        }

        // Start or end double tap timer
        if (checkDoubleTapB) {
            // if second press comes before 1.0 seconds, pause playback
            if (doubleTapCounterB <= 1000) {
                if (looperB.Recording()) {  // Ensure looper is not recording when double tapped (in case it gets double tapped while recording)
                    looperB.TrigRecord();
                }
                pausePlaybackB = !pausePlaybackB;
                if (pausePlaybackB) {        // Blink LED if paused, otherwise set to triangle wave for pulsing while recording
                    led_oscB.SetWaveform(4); // WAVE_SIN = 0, WAVE_TRI = 1, WAVE_SAW = 2, WAVE_RAMP = 3, WAVE_SQUARE = 4
                } else {
                    led_oscB.SetWaveform(1); 
                }
                doubleTapCounterB = 0;    // reset double tap here also to prevent weird behaviour when triple clicked
                checkDoubleTapB = false;
                led2.Set(1.0f);
            }
        } else {
            checkDoubleTapB = true;
        }
    }

    if (checkDoubleTapB) {
        doubleTapCounterB += 1;          // Increment by 1 (48000 * 0.75)/blocksize = 1000   (blocksize is 48)
        if (doubleTapCounterB > 1000) {  // If timer goes beyond 1.0 seconds, stop double tap checking
            doubleTapCounterB = 0;
            checkDoubleTapB = false;
        }
    }

    // If switch2 is held, clear the looper and turn off LED
    if((hw.switches[Funbox::FOOTSWITCH_2].TimeHeldMs() >= 1000 && !pdip[0]) || (hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 1000  && pdip[0]))
    {
        pausePlaybackB = false;
        led_oscB.SetWaveform(1); 
        looperB.Clear();
        led2.Set(0.0f);
    } 


}

void setExpressionMode()  // Verify this only triggers when dipswitch changed
{
    if (pdip[2] == true) { 
        if (!expHandler.isExpressionSetMode())
            expHandler.ToggleExpressionSetMode();

        led1.Set(expHandler.returnLed1Brightness());  // Dim LEDs in expression set mode
        led2.Set(expHandler.returnLed2Brightness());  // Dim LEDs in expression set mode

        expression_pressed = true; 

    } else {       
        if (expHandler.isExpressionSetMode())
            expHandler.ToggleExpressionSetMode(); 

        led1.Set(bypass ? 0.0f : 1.0f); 
        led2.Set(0.0f);  
    }
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
    for(int i=0; i<4; i++) {
        if (hw.switches[dip[i]].Pressed() != pdip[i]) {
            pdip[i] = hw.switches[dip[i]].Pressed();
            // Action for dipswitches handled in audio callback
            setExpressionMode();
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

    // Knob and Expression Processing ////////////////////

    float knobValues[6];
    float newExpressionValues[6];

    knobValues[0] = levelA.Process();
    knobValues[1] = modA.Process(); 
    knobValues[2] = levelB.Process();

    knobValues[3] = speedA.Process(); 
    knobValues[4] = modB.Process();
    knobValues[5] = speedB.Process();

    float vexpression = expression.Process(); // 0 is heel (up), 1 is toe (down)
    expHandler.Process(vexpression, knobValues, newExpressionValues);


    // If in expression set mode, set LEDS accordingly
    if (expHandler.isExpressionSetMode()) {
        led1.Set(expHandler.returnLed1Brightness());
        led2.Set(expHandler.returnLed2Brightness());
    }
  

    float vlevelA = newExpressionValues[0];
    float vmodA = newExpressionValues[1];
    float vlevelB = newExpressionValues[2];

    float vspeedA = newExpressionValues[3];
    float vmodB = newExpressionValues[4];
    float vspeedB = newExpressionValues[5];

    // Handle Knob Changes Here


    // LOOPER A
    float speed_inputA = 0.0;
    float speed_inputAabs = 0.0;

    if (pswitch1[0] == true) { // Switch1 left = smooth

        if (vspeedA <= 0.5) {
            speed_inputA = vspeedA * 6.0 - 2.0; // maps 0 to 0.5 control to -2x to 1x speed
            //speed_inputAabs = abs(speed_inputA);
        } else {
            speed_inputA = vspeedA * 2.0;  // maps 0.5 to 1.0 control to 1x to 2x speed
            //speed_inputAabs = abs(speed_inputA);

        }


        //if (speed_inputA < 0.0) {
        //    looperA.SetReverse(true);
        //} else {
        //    looperA.SetReverse(false);
        //}
    } else if (pswitch1[1] == true) { // Switch1 right = random TODO

    } else {                        // Switch1 center = stepped
        if (vspeedA < 0.05) {
            speed_inputA = -2.0;
        } else if (vspeedA >= 0.05 && vspeedA <= 0.15) {
            speed_inputA = -1.5;
        } else if (vspeedA >= 0.15 && vspeedA <= 0.25) {
            speed_inputA = -1.0;
        } else if (vspeedA >= 0.25 && vspeedA <= 0.35) {
            speed_inputA = -0.5;
        } else if (vspeedA >= 0.35 && vspeedA <= 0.45) {
            speed_inputA = 0.5;
        } else if (vspeedA >= 0.45 && vspeedA <= 0.55) {  // Noon is 1x speed
            speed_inputA = 1.0;
        } else if (vspeedA >= 0.55 && vspeedA <= 0.7) {
            speed_inputA = 1.5;
        } else if (vspeedA >= 0.7 && vspeedA <= 0.8) {
            speed_inputA = 2.0;
        } else if (vspeedA >= 0.8 && vspeedA <= 0.9) {
            speed_inputA = 2.5;
        } else {
            speed_inputA = 3.0;
        }

        if (speed_inputA < 0.0) {
            looperA.SetReverse(true);
        } else {
            looperA.SetReverse(false);
        }
        speed_inputAabs = abs(speed_inputA);

        looperA.SetIncrementSize(speed_inputAabs);
    }



    // LOOPER B
    float speed_inputB = 0.0;
    float speed_inputBabs = 0.0;

    // TODO At some point combine the logic of using pswitch1 for both loopers
    if (pswitch1[0] == true) { // Switch3 left = smooth
        if (vspeedB <= 0.5) {
            speed_inputB = vspeedB * 6.0 - 2.0; // maps 0 to 0.5 control to -2x to 1x speed
            //speed_inputBabs = abs(speed_inputB);
        } else {
            speed_inputB = vspeedB * 2.0;  // maps 0.5 to 1.0 control to 1x to 2x speed
            //speed_inputBabs = abs(speed_inputB);
        }


        //if (speed_inputB < 0.0) {
        //    looperB.SetReverse(true);
        //} else {
        //    looperB.SetReverse(false);
        //}
    } else if (pswitch1[1] == true) { // Switch3 right = random TODO

    } else {                        // Switch3 center = stepped
        if (vspeedB < 0.05) {
            speed_inputB = -2.0;
        } else if (vspeedB >= 0.05 && vspeedB <= 0.15) {
            speed_inputB = -1.5;
        } else if (vspeedB >= 0.15 && vspeedB <= 0.25) {
            speed_inputB = -1.0;
        } else if (vspeedB >= 0.25 && vspeedB <= 0.35) {
            speed_inputB = -0.5;
        } else if (vspeedB >= 0.35 && vspeedB <= 0.45) {
            speed_inputB = 0.5;
        } else if (vspeedB >= 0.45 && vspeedB <= 0.55) {  // Noon is 1x speed
            speed_inputB = 1.0;
        } else if (vspeedB >= 0.55 && vspeedB <= 0.7) {
            speed_inputB = 1.5;
        } else if (vspeedB >= 0.7 && vspeedB <= 0.8) {
            speed_inputB = 2.0;
        } else if (vspeedB >= 0.8 && vspeedB <= 0.9) {
            speed_inputB = 2.5;
        } else {
            speed_inputB = 3.0;
        }

        if (speed_inputB < 0.0) {
            looperB.SetReverse(true);
        } else {
            looperB.SetReverse(false);
        }
        speed_inputBabs = abs(speed_inputB);

        looperB.SetIncrementSize(speed_inputBabs);
    }





    // EFFECTS //
    if (pswitch2[0] == true) { // Center switch left

        smoothRandA.SetFreq(vmodA*4);
        smoothRandB.SetFreq(vmodB*4);
        

    } else if (pswitch2[1] == true) { // Center switch right
        float vReverbTime = vmodA;
        if (vReverbTime < 0.01) { // if knob < 1%, set reverb to 0
            verb.SetFeedback(0.0);
        } else if (vReverbTime >= 0.01 && vReverbTime <= 0.1) {
            verb.SetFeedback(vReverbTime * 6.4 ); // Reverb time range 0.0 to 0.6 for 1% to 10% knob turn (smooth ramping to useful reverb time values, i.e. 0.6 to 1)
        } else {
            verb.SetFeedback(vReverbTime * 0.4 + 0.6); // Reverb time range 0.6 to 1.0
        }

        float invertedFreq = 1.0 - vmodB; // Invert the damping param so that knob left is less dampening, knob right is more dampening
        invertedFreq = invertedFreq * invertedFreq; // also square it for exponential taper (more control over lower frequencies)
        verb.SetLpFreq(600.0 + invertedFreq * (16000.0 - 600.0));

    } else { // Center switch center

       // TODO Need to do CUBE taper for this filter control
       float vfilterA = vmodA;
        // Set Filter Controls
        if (vfilterA <= 0.5) {
            float filter_valueA = (vfilterA * vfilterA * vfilterA * 39800.0f) + 80.0f;
            toneA.SetFreq(filter_valueA);
        } else {
            vfilterA = vfilterA - 0.5;
            float filter_valueA = (vfilterA * vfilterA) * 500.0f + 40.0f;
            toneHPA.SetFreq(filter_valueA);
        }

        float vfilterB = vmodB;
        // Set Filter Controls
        if (vfilterB <= 0.5) {
            float filter_valueB = (vfilterB * vfilterB * vfilterB *  39800.0f) + 80.0f;
            toneB.SetFreq(filter_valueB);
        } else {
            vfilterB = vfilterB - 0.5;
            float filter_valueB = (vfilterB  * vfilterB) * 500.0f + 40.0f;
            toneHPB.SetFreq(filter_valueB);
        }
    }

    float inL, inR;
    float sendl, sendr, wetl, wetr;

    for(size_t i = 0; i < size; i++)
    {
        ledBrightnessA = led_oscA.Process();
        ledBrightnessB = led_oscB.Process();

        // Handle smooth speed knob transitions
        if (pswitch1[0] == true) { // Switch1 left = smooth
            // Smooth out Looper A transitions
            fonepole(currentSpeedA, speed_inputA, .00006f); 

            if (currentSpeedA < 0.0) {
                looperA.SetReverse(true);
            } else {
                looperA.SetReverse(false);
            }
            speed_inputAabs = abs(currentSpeedA);
            looperA.SetIncrementSize(speed_inputAabs);


            // Smooth out Looper B transitions
            fonepole(currentSpeedB, speed_inputB, .00006f);  

            if (currentSpeedB < 0.0) {
                looperB.SetReverse(true);
            } else {
                looperB.SetReverse(false);
            }
            speed_inputBabs = abs(currentSpeedB);
            looperB.SetIncrementSize(speed_inputBabs);

        } 

        if (pswitch2[0] == true) {    // Stability
            // Set Stablility of Loops

            looperA.SetIncrementSize(speed_inputAabs + smoothRandA.Process() * vmodA * 0.05);
            looperB.SetIncrementSize(speed_inputBabs + smoothRandB.Process() * vmodB * 0.05);
        }


        // Process your signal here
        if (pdip[0]) {            // If Stereo, use Looper A for Left channel and Looper B for right channel
            inL = in[0][i];
            inR = in[1][i]; 
        } else {                  // Else if MISO, just use the left channel for both looper inputs
            inL = in[0][i];
            inR = in[0][i];
        }


        /// Process Looper A // 
        float loop_outA = 0.0;
        if (!pausePlaybackA) {
            loop_outA = looperA.Process(inL) * vlevelA * 2.0;
        }

        /// Process Looper B // 
        float loop_outB = 0.0;
        if (!pausePlaybackB) {
            loop_outB = looperB.Process(inR) * vlevelB * 2.0;
        }

        wetl = wetr = 0.0;

        if (pswitch2[1] == true) {  // Reverb
            sendl = loop_outA;
            sendr = loop_outB;
            verb.Process(sendl, sendr, &wetl, &wetr);

        } else if (pswitch2[0] == false && pswitch2[1] == false) {                 // Filter
            // Process Tone
            float filter_inA =  loop_outA;
            float filter_outA;
            
            if (vmodA <= 0.5) {
                filter_outA = toneA.Process(filter_inA);
                loop_outA = balA.Process(filter_outA, filter_inA);

            } else {
                filter_outA = toneHPA.Process(filter_inA);
                loop_outA = balA.Process(filter_outA, filter_inA);
            }

            // Process Tone
            float filter_inB =  loop_outB;
            float filter_outB;
            
            if (vmodB <= 0.5) {
                filter_outB = toneB.Process(filter_inB);
                loop_outB = balB.Process(filter_outB, filter_inB);

            } else {
                filter_outB = toneHPA.Process(filter_inB);
                loop_outB = balA.Process(filter_outB, filter_inB);
            }

        }
        if (pdip[0]) {            // If Stereo, output loop A on left and loop b on right
                                  // Note, if using stereo and moving the speed settings for loop A and B differently.. things will get really wacky, should I make only 1 speed control for stereo?

            out[0][i] = inL + loop_outA + wetl;
            out[1][i] = inR + loop_outB + wetr;

        } else {                  // Else if MISO, output both loops to both left and right channels

            out[0][i] = inL + loop_outA + loop_outB + wetl;
            out[1][i] = inR + loop_outA + loop_outB + wetr;
        }
     
    }

    // Handle Pulsing LEDs
    if (looperA.Recording()) {
        led1.Set(ledBrightnessA * 0.5 + 0.5);       // Pulse the LED when recording
    } 

    if (pausePlaybackB) {
        led1.Set(ledBrightnessA * 2.0);             // Blink the LED when paused
    }


    if (looperB.Recording()) {
        led2.Set(ledBrightnessB * 0.5 + 0.5);       // Pulse the LED when recording
    } 
    if (pausePlaybackB) {
        led2.Set(ledBrightnessB * 2.0);             // Blink the LED when paused
    }
   

    led1.Update();
    led2.Update();
}
          

int main(void)
{
    float samplerate;

    hw.Init(); 
    
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_96KHZ);  // Test raising the samplerate to have higher fidelity at slower playback speeds
                                                                        // ReverbSC doesn't work running at 96kHz, TODO look into getting this working
    samplerate = hw.AudioSampleRate();  

    hw.SetAudioBlockSize(48);

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
    pdip[0]= false; // MISO or STEREO
    pdip[1]= false;
    pdip[2]= false;
    pdip[3]= false;


    levelA.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    modA.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    levelB.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR); 
    speedA.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::LINEAR);
    modB.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    speedB.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR); 
    expression.Init(hw.expression, 0.0f, 1.0f, Parameter::LINEAR); 


    looperA.Init(bufA, MAX_SIZE);
    looperA.SetMode(varSpeedLooper::Mode::NORMAL);
    led_oscA.Init(samplerate);
    led_oscA.SetFreq(1.0);
    led_oscA.SetWaveform(1); // WAVE_SIN = 0, WAVE_TRI = 1, WAVE_SAW = 2, WAVE_RAMP = 3, WAVE_SQUARE = 4
    ledBrightnessA = 0.0;
    pausePlaybackA = false;
    currentSpeedA = 1.0;

    led_oscA.Init(samplerate);
    led_oscA.SetFreq(1.5);
    led_oscA.SetWaveform(1);



    looperB.Init(bufB, MAX_SIZE);
    looperB.SetMode(varSpeedLooper::Mode::NORMAL);
    led_oscB.Init(samplerate);
    led_oscB.SetFreq(1.0);
    led_oscB.SetWaveform(1); // WAVE_SIN = 0, WAVE_TRI = 1, WAVE_SAW = 2, WAVE_RAMP = 3, WAVE_SQUARE = 4
    ledBrightnessB = 0.0;
    pausePlaybackB = false;
    currentSpeedB = 1.0;

    led_oscB.Init(samplerate);
    led_oscB.SetFreq(1.5);
    led_oscB.SetWaveform(1);

    verb.Init(samplerate); 

    toneA.Init(samplerate);
    toneHPA.Init(samplerate);
    balA.Init(samplerate);

    toneB.Init(samplerate);
    toneHPB.Init(samplerate);
    balB.Init(samplerate);


    smoothRandA.Init(samplerate);
    smoothRandB.Init(samplerate);
    

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