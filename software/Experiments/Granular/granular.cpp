#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"

//
// This is a template for creating a pedal on the GuitarML Funbox_v3/Daisy Seed platform.
// You can start from here to fill out your effects processing and controls.
// Allows for Stereo In/Out, 6 knobs, 3 3-way switches, 4 dipswitches, 2 SPST Footswitches, 2 LEDs.
//
// Keith Bloemer 6/12/2024
//

using namespace daisy;
using namespace daisysp;
using namespace funbox;  // This is important for mapping the correct controls to the Daisy Seed on Funbox PCB

// Declare a local daisy_petal for hardware access
DaisyPetal hw;
Parameter grainsize, speed, pitch, mix, level, samplesize, expression;

bool            bypass;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];


int mode = 0; // 0=modalvoice 1=stringvoice 2=synth

#define MAX_SAMPLE static_cast<int>(48000.0 * 4) // 4 second sample, continually updating with live audio
#define MAX_SAMPLE_SIZET static_cast<size_t>(MAX_SAMPLE) // 2 second sample, continually updating with live audio
float DSY_SDRAM_BSS audioSample[MAX_SAMPLE_SIZET];  // Need to initialize this to all 0's
//float audioSample[MAX_SAMPLE_SIZET];  // Not sure if this needs to be manually initialized to 0's if not in SDRAM, but doing it in main() just in case

GranularPlayer granular;
int sample_index;
bool trigger;
int fade_length;  // fade in/fade out audio sample by this many individual samples

bool first_start=true;

Led led1, led2;

// Midi
bool midi_control[6]; // knobs 0-5
float pknobValues[6];
float knobValues[6];

// Expression 
bool expression_control = false;
float pexpression = 0.0;


int effect_mode = 0;

float led2_brightness = 0.0;

int counter = 0;


bool knobMoved(float old_value, float new_value)
{
    float tolerance = 0.005;
    if (new_value > (old_value + tolerance) || new_value < (old_value - tolerance)) {
        return true;
    } else {
        return false;
    }
}

// Switches currently unused
void updateSwitch1() 
{
    if (pswitch1[0] == true) {  // left
        mode = 0;
    } else if (pswitch1[1] == true) {  // right
        mode = 2;

    } else {   // center
        mode = 1;
    }      
}

void updateSwitch2() // left=reverb, center=delay, right=
{
    if (pswitch2[0] == true) {  // left
        effect_mode = 0;

    } else if (pswitch2[1] == true) {  // right
        effect_mode = 1;


    } else {   // center
        effect_mode = 1;


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

    // (De-)Activate bypass and toggle LED when left footswitch is let go
    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {
        bypass = !bypass;
        led1.Set(bypass ? 0.0f : 1.0f);
    }

    led1.Update();


    // Freeze function
    if(hw.switches[Funbox::FOOTSWITCH_2].RisingEdge())
    {
        trigger = true;
        led2.Set(1.0);
    }

    led2.Update();

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
    if (changed1 || first_start) 
        updateSwitch1();
    

    // 3-way Switch 2
    bool changed2 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch2[i]].Pressed() != pswitch2[i]) {
            pswitch2[i] = hw.switches[switch2[i]].Pressed();
            changed2 = true;
        }
    }
    if (changed2 || first_start) 
        updateSwitch2();

    // 3-way Switch 3
    bool changed3 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch3[i]].Pressed() != pswitch3[i]) {
            pswitch3[i] = hw.switches[switch3[i]].Pressed();
            changed3 = true;
        }
    }
    if (changed3 || first_start) 
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


    // Knob 1
    if (!midi_control[0]) {  // If not under midi control or expression control, use knob ADC
        pknobValues[0] = knobValues[0] = grainsize.Process();
    } else if (knobMoved(pknobValues[0], grainsize.Process())) {  // If midi controlled, watch for knob movement to end Midi control
        midi_control[0] = false;
    }

    // Knob 2
    if (!midi_control[1]) {  // If not under midi control, use knob ADC
        pknobValues[1] = knobValues[1] = speed.Process();
    } else if (knobMoved(pknobValues[1], speed.Process())) {  // If midi controlled, watch for knob movement to end Midi control
        midi_control[1] = false;
        expression_control = false;
    }

    // Knob 3
    if (!midi_control[2])   // If not under midi control, use knob ADC
        pknobValues[2] = knobValues[2] = pitch.Process();
    else if (knobMoved(pknobValues[2], pitch.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[3] = false;

    // Knob 4
    if (!midi_control[3])   // If not under midi control, use knob ADC
        pknobValues[3] = knobValues[3] = mix.Process();
    else if (knobMoved(pknobValues[3], mix.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[3] = false;

    // Knob 5
    if (!midi_control[4])   // If not under midi control, use knob ADC
        pknobValues[4] = knobValues[4] = level.Process();
    else if (knobMoved(pknobValues[4], level.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[4] = false;


    // Knob 6
    if (!midi_control[5])   // If not under midi control, use knob ADC
        pknobValues[5] = knobValues[5] = samplesize.Process();
    else if (knobMoved(pknobValues[5], samplesize.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[5] = false;

    float vexpression = expression.Process();
    // Expression just for knob 1
    if (knobMoved(pexpression, vexpression)) {
        pexpression = vexpression;
        if (!first_start) {
            expression_control = true;
        }
    }

    // Overwrite knob 2 with expression value (only using one parameter for expression here: mod level)
    // No "Set Expression Mode", activates when expression plugged in, deactivates when knob 2 moved
    if (expression_control) {
        knobValues[1] = vexpression;
    } 

    // Handle Knob Changes Here

 
    float vgrain_size = knobValues[0]*599 + 1; // 1 to 600ms grain size
    float vspeed = knobValues[1] * 4.0 - 2.0;  // -2 to +2 speed

    // Set pitch in semitones, -12 to + 12 (+/- 1 octave)
    float vpitch_temp = knobValues[2] * 24 - 12; // -1200 to +1200 pitch in cents (100 cents= 1 semitone), +/- 1 octave range
    int rounded_pitch = static_cast<int>(vpitch_temp);
    float vpitch = rounded_pitch * 100;    
    float vmix = knobValues[3];

    float vlevel = knobValues[4];
    int vsamplesize = static_cast<int>(knobValues[5] * MAX_SAMPLE);

    //granular.SetSize(vsamplesize);  // Comment out if granularplayer not updated with setsize

    first_start=false;


    for(size_t i = 0; i < size; i++)
    {

        // Process your signal here
        if(bypass)
        {
            
            out[0][i] = in[0][i]; 
            out[1][i] = in[1][i];

        }
        else
        {   
            
            float input = in[0][i];

            // Update the sample buffer if not in freeze mode
            if (trigger) {
                float multiplier = 1.0;
                float fade_len_float = static_cast<float>(fade_length);
                if (sample_index < fade_length) { // fade in
                    multiplier = sample_index / fade_len_float; // 0 to 1

                } else if (sample_index > (MAX_SAMPLE - fade_length)) { // fade out
                    multiplier = (MAX_SAMPLE - sample_index) / fade_len_float;  //  1 to 0  // Might be missing last index, (maxsample-1)

                }

                audioSample[sample_index] = input * multiplier;

                sample_index += 1;
                if (sample_index > (MAX_SAMPLE - 1)) {   /// TODO Right now records for full max_sample size, See if I can record for just the samplesize knob setting
                    sample_index = 0;
                    trigger = false;
                    led2.Set(0.0);
                    led2.Update();
                    counter = 0;
                }
            }


            /** Processes the granular player.
                \param speed playback speed. 1 is normal speed, 2 is double speed, 0.5 is half speed, etc. Negative values play the sample backwards.
                \param transposition transposition in cents. 100 cents is one semitone. Negative values transpose down, positive values transpose up.
                \param grain_size grain size in milliseconds. 1 is 1 millisecond, 1000 is 1 second. Does not accept negative values. Minimum value is 1.
            */
            float gran_out = 0.0;
            if (!trigger)
                gran_out = granular.Process(vspeed, vpitch, vgrain_size);  

            //FOR TROUBLESHOOTING TO CHECK NORMAL SAMPLE PLAYBACK
            //float gran_out = audioSample[counter];    
            //counter+=1;    
            //if (counter == MAX_SAMPLE) {
            //    counter = 0;
            //}

            out[0][i] = out[1][i] = (gran_out * vmix + input * (1.0 - vmix)) * vlevel * 4.0; 

        }
    }
}


          

int main(void)
{
    float samplerate;

    hw.Init(true);
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
    pdip[0]= false;
    pdip[1]= false;
    pdip[2]= false;
    pdip[3]= false;


    sample_index = 0;
    trigger = false;
    fade_length = 8000; 

    for(int i = 0; i < MAX_SAMPLE; i++) { // hard coding sample length for now
        audioSample[i] = 0.;
    }

    /** Initializes the GranularPlayer module.
        \param sample pointer to the sample to be played
        \param size number of elements in the sample array
        \param sample_rate audio engine sample rate
    */
    granular.Init(audioSample, MAX_SAMPLE, samplerate);

    grainsize.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    speed.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    pitch.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR); 
    mix.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::LINEAR);
    level.Init(hw.knob[Funbox::KNOB_5], 0.01f, 1.0f, Parameter::LINEAR);
    samplesize.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR); 
    expression.Init(hw.expression, 0.0f, 1.0f, Parameter::LINEAR); 


    // Init the LEDs and set activate bypass
    led1.Init(hw.seed.GetPin(Funbox::LED_1),false);
    led1.Update();
    bypass = true;

    led2.Init(hw.seed.GetPin(Funbox::LED_2),false);
    led2.Update();

    //hw.InitMidi();
    //hw.midi.StartReceive();

    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    while(1)
    {
        //hw.midi.Listen();
        // Handle MIDI Events
        //while(hw.midi.HasEvents())
        //{
        //    HandleMidiMessage(hw.midi.PopEvent());
        //}

    }
}