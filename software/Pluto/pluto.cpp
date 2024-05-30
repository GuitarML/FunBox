#include "daisy_petal.h"
#include "daisysp.h"
#include "varSpeedLooper.h"
#include "funbox.h"

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
Parameter levelA, modA, levelB, speedA, modB, speedB;

// Looper Parameters
#define MAX_SIZE (96000 * 60) // 1 minute of floats at 96 khz
float DSY_SDRAM_BSS bufA[MAX_SIZE];
varSpeedLooper looperA;
Oscillator      led_oscA; // For pulsing the led when recording / paused playback
float           ledBrightnessA;
int             doubleTapCounterA;
bool            checkDoubleTapA;
bool            pausePlaybackA;


float DSY_SDRAM_BSS bufB[MAX_SIZE];
varSpeedLooper looperB;
Oscillator      led_oscB; // For pulsing the led when recording / paused playback
float           ledBrightnessB;
int             doubleTapCounterB;
bool            checkDoubleTapB;
bool            pausePlaybackB;



bool            bypass;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[2];
int             switch1[2], switch2[2], switch3[2], dip[2];


Led led1, led2;


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

    } else if (pswitch3[1] == true) {  // right


    } else {   // center

    }    
}


void UpdateButtons()
{


    // LOOPER A //
    // Looper footswitch pressed (start/stop recording, doubletap to pause/unpause playback)
    if(hw.switches[Funbox::FOOTSWITCH_1].RisingEdge())
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
    if(hw.switches[Funbox::FOOTSWITCH_2].RisingEdge())
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
    if(hw.switches[Funbox::FOOTSWITCH_2].TimeHeldMs() >= 1000)
    {
        pausePlaybackB = false;
        led_oscB.SetWaveform(1); 
        looperB.Clear();
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


    float vlevelA = levelA.Process();
    float vmodA = modA.Process(); 
    float vlevelB = levelB.Process();

    float vspeedA = speedA.Process(); 
    float vmodB = modB.Process();
    float vspeedB = speedB.Process();

    // Handle Knob Changes Here
 
    // LOOPER A
    float speed_inputA = 0.0;
    float speed_inputAabs = 0.0;

    if (pswitch1[0] == true) { // Switch1 left = smooth
        speed_inputA = vspeedA * 4.0 - 2.0;
        speed_inputAabs = abs(speed_inputA);

        if (speed_inputA < 0.0) {
            looperA.SetReverse(true);
        } else {
            looperA.SetReverse(false);
        }
    } else if (pswitch1[1] == true) { // Switch1 right = random TODO

    } else {                        // Switch1 center = stepped
        if (vspeedA < 0.1) {
            speed_inputA = -2.0;
        } else if (vspeedA >= 0.1 && vspeedA <= 0.2) {
            speed_inputA = -1.5;
        } else if (vspeedA >= 0.2 && vspeedA <= 0.3) {
            speed_inputA = -1.0;
        } else if (vspeedA >= 0.3 && vspeedA <= 0.4) {
            speed_inputA = -0.5;
        } else if (vspeedA >= 0.4 && vspeedA <= 0.5) {
            speed_inputA = -0.25;
        } else if (vspeedA >= 0.5 && vspeedA <= 0.6) {
            speed_inputA = 0.25;
        } else if (vspeedA >= 0.6 && vspeedA <= 0.7) {
            speed_inputA = 0.5;
        } else if (vspeedA >= 0.7 && vspeedA <= 0.8) {
            speed_inputA = 1.0;
        } else if (vspeedA >= 0.8 && vspeedA <= 0.9) {
            speed_inputA = 1.5;
        } else {
            speed_inputA = 2.0;
        }

        if (speed_inputA < 0.0) {
            looperA.SetReverse(true);
        } else {
            looperA.SetReverse(false);
        }
        speed_inputAabs = abs(speed_inputA);
    }

    looperA.SetIncrementSize(speed_inputAabs);



    // LOOPER B
    float speed_inputB = 0.0;
    float speed_inputBabs = 0.0;

    if (pswitch3[0] == true) { // Switch3 left = smooth
        speed_inputB = vspeedB * 4.0 - 2.0;
        speed_inputBabs = abs(speed_inputB);

        if (speed_inputB < 0.0) {
            looperB.SetReverse(true);
        } else {
            looperB.SetReverse(false);
        }
    } else if (pswitch3[1] == true) { // Switch3 right = random TODO

    } else {                        // Switch3 center = stepped
        if (vspeedB < 0.1) {
            speed_inputB = -2.0;
        } else if (vspeedB >= 0.1 && vspeedB <= 0.2) {
            speed_inputB = -1.5;
        } else if (vspeedB >= 0.2 && vspeedB <= 0.3) {
            speed_inputB = -1.0;
        } else if (vspeedB >= 0.3 && vspeedB <= 0.4) {
            speed_inputB = -0.5;
        } else if (vspeedB >= 0.4 && vspeedB <= 0.5) {
            speed_inputB = -0.25;
        } else if (vspeedB >= 0.5 && vspeedB <= 0.6) {
            speed_inputB = 0.25;
        } else if (vspeedB >= 0.6 && vspeedB <= 0.7) {
            speed_inputB = 0.5;
        } else if (vspeedB >= 0.7 && vspeedB <= 0.8) {
            speed_inputB = 1.0;
        } else if (vspeedB >= 0.8 && vspeedB <= 0.9) {
            speed_inputB = 1.5;
        } else {
            speed_inputB = 2.0;
        }

        if (speed_inputB < 0.0) {
            looperB.SetReverse(true);
        } else {
            looperB.SetReverse(false);
        }
        speed_inputBabs = abs(speed_inputB);
    }

    looperB.SetIncrementSize(speed_inputBabs);

    // TODO Add effects like filter and reverb
    for(size_t i = 0; i < size; i++)
    {
        ledBrightnessA = led_oscA.Process();
        ledBrightnessB = led_oscB.Process();

        // Process your signal here


        float inL = in[0][i];
        float inR = in[1][i];


        /// Process Looper A // 
        float loop_outA = 0.0;
        if (!pausePlaybackA) {
            loop_outA = looperA.Process(inL) * vlevelA * 2.0;
        }

        /// Process Looper B // 
        float loop_outB = 0.0;
        if (!pausePlaybackB) {
            loop_outB = looperB.Process(inL) * vlevelB * 2.0;
        }


        out[0][i] = inL + loop_outA + loop_outB;
        out[1][i] = out[0][i];


        // Final mix
        //if (pdip[0] == false) {// MISO
            //out[0][i] = (inL * dryMix + effectsL * wetMix) + (inL * dryMix + effectsR * wetMix) * vLevel / 2.0;
            //out[1][i] = out[0][i];

        //} else { // Stereo
            //out[0][i] = (inL * dryMix + effectsL * wetMix) * vLevel;
             //out[1][i] = (inL * dryMix + effectsR * wetMix) * vLevel;
        //}
     
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

    pswitch1[0]= false;
    pswitch1[1]= false;
    pswitch2[0]= false;
    pswitch2[1]= false;
    pswitch3[0]= false;
    pswitch3[1]= false;
    pdip[0]= false;
    pdip[1]= false;


    levelA.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    modA.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    levelB.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR); 
    speedA.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::LINEAR);
    modB.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    speedB.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR); 


    looperA.Init(bufA, MAX_SIZE);
    looperA.SetMode(varSpeedLooper::Mode::NORMAL);
    led_oscA.Init(samplerate);
    led_oscA.SetFreq(1.0);
    led_oscA.SetWaveform(1); // WAVE_SIN = 0, WAVE_TRI = 1, WAVE_SAW = 2, WAVE_RAMP = 3, WAVE_SQUARE = 4
    ledBrightnessA = 0.0;
    pausePlaybackA = false;

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

    led_oscB.Init(samplerate);
    led_oscB.SetFreq(1.5);
    led_oscB.SetWaveform(1);


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