#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"


using namespace daisy;
using namespace daisysp;
using namespace funbox;  // This is important for mapping the correct controls to the Daisy Seed on Funbox PCB

// Declare a local daisy_petal for hardware access
DaisyPetal hw;
Parameter Cdepth, Mix, ReverbTime, Cfreq, Level, Rdamp;


bool            bypass;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[2];
int             switch1[2], switch2[2], switch3[2], dip[2];


float dryMix, wetMix;

Led led1, led2;

// Pitchshifter
PitchShifter  pitch;
PitchShifter  pitchRight;

// Chorus
Chorus chorus;

// Reverb
ReverbSc        verb;


void updateSwitch1() // Chorus settings, left=norm, center=spreadFreq, right=spreadDelay
{

    
}

void updateSwitch2()  // Routing, left=Chorus->Reverb, center=parallel, right=Reverb->Chorus
{

}


void updateSwitch3()  // Reverb, left=Normal, center=Shimmer(octave up), right=deep(octave down)
{
    if (pswitch3[0] == true) {  // left
        pitch.SetTransposition(0.0);
        pitchRight.SetTransposition(0.0);

    } else if (pswitch3[1] == true) {  // right
        pitch.SetTransposition(-12.0);
        pitchRight.SetTransposition(-12.0);

    } else {   // center
        pitch.SetTransposition(12.0);
        pitchRight.SetTransposition(12.0);
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
    //led1.Set(pdip[0] ? 0.0f : 1.0f); // Seems opposite, "On" position turns led off here
    //led2.Set(pdip[1] ? 0.0f : 1.0f);
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
 

    float vCdepth = Cdepth.Process();
    float vCfreq = Cfreq.Process(); 
    float vmix = Mix.Process();

    float vReverbTime = ReverbTime.Process();
    float vRdamp = Rdamp.Process();
    float vLevel = Level.Process();

    // MIX //

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


    // REVERB //
    //  Reverb time and damping

    float invertedFreq = 1.0 - vRdamp; // Invert the damping param so that knob left is less dampening, knob right is more dampening
    invertedFreq = invertedFreq * invertedFreq; // also square it for exponential taper (more control over lower frequencies)
    verb.SetLpFreq(600.0 + invertedFreq * (16000.0 - 600.0));


    if (vReverbTime < 0.01) { // if knob < 1%, set reverb to 0
        verb.SetFeedback(0.0);
    } else if (vReverbTime >= 0.01 && vReverbTime <= 0.1) {
        verb.SetFeedback(vReverbTime * 6.4 ); // Reverb time range 0.0 to 0.6 for 1% to 10% knob turn (smooth ramping to useful reverb time values, i.e. 0.6 to 1)
    } else {
        verb.SetFeedback(vReverbTime * 0.4 + 0.6); // Reverb time range 0.6 to 1.0
    } 

  
    // CHORUS //

    float lfoLeft = vCfreq;
    float lfoRight = vCfreq;
    float delayLeft = 0.75;
    float delayRight = 0.75;

    if (pswitch1[1] == true) { // Right
        delayLeft = 0.2;
        delayRight = 0.95;
    } else if (pswitch1[0] == false && pswitch1[1] == false) { // center
        lfoRight = 1.0 - vCfreq; // inverted right lfo frequency
    }
    chorus.SetLfoFreq(lfoLeft, lfoRight);
    chorus.SetLfoDepth(vCdepth);
    chorus.SetDelay(delayLeft, delayRight);


    float sendl, sendr, wetl, wetr;
    float chorusL, chorusR;    
    float pitchL, pitchR;    
    float effectsL, effectsR;

    for(size_t i = 0; i < size; i++)
    {

        // Process your signal here
        if(bypass)
        {
            // Could do a trails mode here
            
            out[0][i] = in[0][i]; 
            out[1][i] = in[1][i];

        }
        else
        {   

            float inL = in[0][i];
            float inR = in[1][i];

            // Routing

            if (pswitch2[0] == true) { //left Chorus into Reverb

                chorusL = chorus.Process(inL);
                chorusR = chorus.GetRight();

                sendl = chorusL;
                sendr = chorusR;
                verb.Process(sendl, sendr, &wetl, &wetr);

                pitchL = pitch.Process(wetl);
                pitchR = pitchRight.Process(wetr);

                effectsL = pitchL;
                effectsR = pitchR;

            } else if (pswitch2[1] == true) { // right Reverb into Chorus

                sendl = inL;
                sendr = inL;
                verb.Process(sendl, sendr, &wetl, &wetr);

                pitchL = pitch.Process(wetl);
                pitchR = pitchRight.Process(wetr);

                chorusL = chorus.Process((pitchL + pitchR) / 2.0);
                chorusR = chorus.GetRight();

                effectsL = chorusL;
                effectsR = chorusR;

            } else {        // center parallel
                chorusL = chorus.Process(inL);
                chorusR = chorus.GetRight();

                sendl = inL;
                sendr = inL;
                verb.Process(sendl, sendr, &wetl, &wetr);

                pitchL = pitch.Process(wetl);
                pitchR = pitchRight.Process(wetr);

                effectsL = chorusL + pitchL;
                effectsR = chorusR + pitchR;

            }

            // Final mix
            if (pdip[0] == false) {// Mono
                out[0][i] = (inL * dryMix + effectsL * wetMix) + (inL * dryMix + effectsR * wetMix) * vLevel / 2.0;
                out[1][i] = out[0][i];

            } else { // MISO
                out[0][i] = (inL * dryMix + effectsL * wetMix) * vLevel;
                out[1][i] = (inL * dryMix + effectsR * wetMix) * vLevel;
            }

        }
    }
}
          

int main(void)
{
    float samplerate;

    hw.Init();
    samplerate = hw.AudioSampleRate();

    verb.Init(samplerate);

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


    Cdepth.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    Mix.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    ReverbTime.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR); 
    Cfreq.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::LINEAR);
    Level.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    Rdamp.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR); 

    verb.SetFeedback(0.0);
    verb.SetLpFreq(9000.0);

    pitch.Init(samplerate);
    pitch.SetTransposition(12.0);
    pitch.SetFun(0.0);
    pitch.SetDelSize(300);  // Can't be 0 or doesnt work

    pitchRight.Init(samplerate);
    pitchRight.SetTransposition(12.0);
    pitchRight.SetFun(0.0);
    pitchRight.SetDelSize(300);  // Can't be 0 or doesnt work

    chorus.Init(samplerate);
   
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