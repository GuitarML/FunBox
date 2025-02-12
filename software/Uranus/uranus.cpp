#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"
#include "granularplayermod.h"
#include "fm_synth_2op.h"
#include <q/fx/envelope.hpp>

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
Parameter grainsize, speed, pitch, mix, feedback, width, expression;
float vpitch1, vpitch2;

float dryMix;
float wetMix;

bool alternateMode;
// Initialize "previous" p values
float pmix, ppitch_raw, pspeed_raw;
float vspeed, vpitch; // These need to be global for alternate funtionality

bool force_reset = false;

bool            bypass;
bool            hold;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];

float      current_grainsize; //for smoothing the size param

int mode = 0; // 0=modalvoice 1=stringvoice 2=synth

#define MAX_SAMPLE static_cast<int>(48000.0 / 2) // 0.5 second sample (delay), continually updating with live audio
#define MAX_SAMPLE_SIZET static_cast<size_t>(MAX_SAMPLE) 
//float DSY_SDRAM_BSS audioSample[MAX_SAMPLE_SIZET];  // Need to initialize this to all 0's
float audioSample[MAX_SAMPLE_SIZET];  // Need to initialize this to all 0's


GranularPlayerMod swarm[2];


int sample_index;
bool trigger;
int fade_length;  // fade in/fade out audio sample by this many individual samples
int current_grain_buffer_size;

// synth
Adsr       env_;
bool env_gate_ = false;
// global midi key values for granular synth
float note_ = 0.0;
float velocity_ = 0.0;

// LFO for pitch modulation
Oscillator LFO;
float LFO_depth;

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
int synth_mode = 0;

float led2_brightness = 0.0;

int counter = 0;


// FM Synth
static VoiceManager<12> voice_handler;


bool knobMoved(float old_value, float new_value)
{
    float tolerance = 0.005;
    if (new_value > (old_value + tolerance) || new_value < (old_value - tolerance)) {
        return true;
    } else {
        return false;
    }
}

// Switches1 live mode, sample mode, dry sample loop mode
void updateSwitch1() 
{
    if (pswitch1[0] == true) { 
        effect_mode = 0;
        vpitch1 = 0;
        vpitch2 = 1200;

    } else if (pswitch1[1] == true) {  
        effect_mode = 2;
        vpitch1 = -1200;
        vpitch2 = 1200;

    } else {  
        effect_mode = 1;
        vpitch1 = 0;
        vpitch2 = -1200;
    }   
}

void updateSwitch2() // envelope modes
{
    if (pswitch2[0] == true) {         // left  cosine grain envelope
        mode = 0;
    } else if (pswitch2[1] == true) {  // right   fast attack-decay grain envelope
        mode = 2;
    } else {                           // center  attack- fast decay grain envelope
        mode = 1;
    }

    for (int i = 0; i < 2; i++) {
        swarm[i].setEnvelopeMode(mode);
    }     
}


void updateSwitch3() // left=, center=, right=
{
    if (pswitch3[0] == true) {  // left
        synth_mode = 0;
        

    } else if (pswitch3[1] == true) {  // right
        synth_mode = 2;
        

    } else {   // center
        synth_mode = 1;
    } 

    force_reset = true;   
}


void UpdateButtons()
{

    // (De-)Activate bypass and toggle LED when left footswitch is let go
    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {
        if (alternateMode) {
            alternateMode = false;
            force_reset = true; // force parameter reset to enforce current knob settings when leaving alternate mode
            led1.Set(1.0f);
        } else {
            bypass = !bypass;
            led1.Set(bypass ? 0.0f : 1.0f);
        }
    }

    // Toggle Alternate mode by holding down the left footswitch, if not already in alternate mode and not in bypass
    if(hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 400 && !alternateMode && !bypass) {
        alternateMode = true;
        led1.Set(0.5f);  // Dim LED in alternate mode
    }

    led1.Update();

    // Hold current sample when activated (half led brightness when held)
    if(hw.switches[Funbox::FOOTSWITCH_2].RisingEdge())
    {
        hold = !hold;
        led2.Set(hold ? 1.0 : 0.0);


    }

    led2.Update();

    // TODO Add standard set expression action (neptune as template with alt mode)

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
        pknobValues[1] = knobValues[1] = mix.Process();
    } else if (knobMoved(pknobValues[1], mix.Process())) {  // If midi controlled, watch for knob movement to end Midi control
        midi_control[1] = false;
        expression_control = false;
    }

    // Knob 3
    if (!midi_control[2])   // If not under midi control, use knob ADC
        pknobValues[2] = knobValues[2] = pitch.Process();
    else if (knobMoved(pknobValues[2], pitch.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[2] = false;

    // Knob 4
    if (!midi_control[3])   // If not under midi control, use knob ADC
        pknobValues[3] = knobValues[3] = feedback.Process();
    else if (knobMoved(pknobValues[3], feedback.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[3] = false;

    // Knob 5
    if (!midi_control[4])   // If not under midi control, use knob ADC
        pknobValues[4] = knobValues[4] = width.Process();
    else if (knobMoved(pknobValues[4], width.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[4] = false;


    // Knob 6
    if (!midi_control[5])   // If not under midi control, use knob ADC
        pknobValues[5] = knobValues[5] = speed.Process();
    else if (knobMoved(pknobValues[5], speed.Process()))  // If midi controlled, watch for knob movement to end Midi control
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
    // TODO Update with full expression controls
    if (expression_control) {
        knobValues[1] = vexpression;
    } 

    // Handle Knob Changes Here

    float vgrain_size = knobValues[0]*300 + 1; // 1 to 300ms grain size


    // Mix and Alternate Stereo Spread /////////////
    float vmix = knobValues[1];
    if (knobMoved(pmix, vmix) || force_reset == true) {
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

        } else {
            float vspread = vmix;
            for (int j = 0; j < 2; j++) {
                swarm[j].setStereoSpread(vspread);
            }

        }
        pmix = vmix;
    }


    // Set pitch in semitones, -12 to + 12 (+/- 1 octave) //////////////
    //  or Alternate Mode LFO Depth

    float vpitch_raw = knobValues[2];

    if (knobMoved(ppitch_raw, vpitch_raw) || force_reset == true) {

        if (alternateMode == false) {
            float vpitch_temp = vpitch_raw * 26 - 13; // -1200 to +1200 pitch in cents (100 cents= 1 semitone), +/- 1 octave range // TODO Might be only hitting semitone below octave, check
            int rounded_pitch = static_cast<int>(vpitch_temp);
            vpitch = rounded_pitch * 100; 

        } else {
            LFO_depth = vpitch_raw * 100; // range of 0 to 100, one semitone
        }
        ppitch_raw = vpitch_raw;
    }

    float vfeedback = knobValues[3];

    float vwidth = knobValues[4] * 50.0; // Width of 50ms

    // Set Grain Speed or Alternate Mode LFO Frequency ////////////////
    float vspeed_raw = knobValues[5];

    if (knobMoved(pspeed_raw, vspeed_raw) || force_reset == true) {

        if (alternateMode == false) {
            vspeed = vspeed_raw * 4.0 - 2.0;  // -2 to +2 speed

        } else {
            LFO.SetFreq(vspeed_raw * vspeed_raw * 6.0); // exponential 0 to 6
        }
        pspeed_raw = vspeed_raw;
    }


    first_start=false;
    force_reset = false;

    for(size_t i = 0; i < size; i++)
    {
        // Process your signal here
        if(bypass)
        {
            
            out[0][i] = out[1][i] = in[0][i];  // MISO when in bypass

        } else {   
            
            float input = in[0][i];

            // If in FM Synth mode, ignore audio input and use the FM synth
            if (synth_mode == 2) {
                float sum        = 0.f;
                sum        = voice_handler.Process() * 0.05f; // 0.05 for volume reduction
                input   = sum;  
            } 

            // If not in Hold mode, process the next sample into delayline and loop back when it reaches the end
            if (!hold) {
                float next_sample_feedback = audioSample[sample_index] * vfeedback; //const feedback for now
                audioSample[sample_index] = next_sample_feedback + input;
                sample_index += 1;
                if (sample_index > (MAX_SAMPLE-1)) {
                    sample_index = 0;

                }
            }

            /** Processes the granular player.
                \param speed playback speed. 1 is normal speed, 2 is double speed, 0.5 is half speed, etc. Negative values play the sample backwards.
                \param transposition transposition in cents. 100 cents is one semitone. Negative values transpose down, positive values transpose up.
                \param grain_size grain size in milliseconds. 1 is 1 millisecond, 1000 is 1 second. Does not accept negative values. Minimum value is 1.
                \param width width range in milliseconds. Will start each grain envelope at a random location within width range. 0 for no randomized location.
            */
            float gran_out_right = 0.0;
            float gran_out_left = 0.0;

            // If in granular monophonic synth mode, use the midi key to calculate pitch, with middle c (midi key 60) being neutral
            if (synth_mode == 1) {
                // note_ is the midi key, 0 to 127
                vpitch = (note_ - 60.0) * 100.0 ; // override pitch setting, note 0 will be 0, note 61=+100, note 59=-100, etc.
            }

            fonepole(current_grainsize, vgrain_size, .0001f); // decrease decimal to slow down transfer

            // Process pitch LFO
            float LFO_out = LFO.Process() - 1.0; // range -1 to 1
            float pitch_mod = LFO_out * LFO_depth; // range -100 to + 100 based on depth

            // Each GranularPlayerMod in the swarm starts at a different phase so the
            //  grains are offest from eachother to create a smoother sound. They also have
            //  different pitch settings. Play around with pitch variations to create 
            //  new textures.
            swarm[0].Process(vspeed, vpitch + vpitch1 + pitch_mod, current_grainsize, vwidth);
            swarm[1].Process(vspeed, vpitch + vpitch2 + pitch_mod, current_grainsize, vwidth); // oct up
            //swarm[2].Process(vspeed, vpitch-1200.0 + pitch_mod, current_grainsize, vwidth); // oct down
            //swarm[3].Process(vspeed, vpitch+700.0 + pitch_mod, current_grainsize, vwidth); // adds a 5th


            gran_out_left += swarm[0].getLeftOut();
            gran_out_right += swarm[0].getRightOut();
        
            gran_out_left += swarm[1].getLeftOut();
            gran_out_right += swarm[1].getRightOut();

        
            // If in monophonic granular synth mode (synth mode 1) process the note envelope and key velocity on the granular output
            if (synth_mode == 1) {
                float amp = env_.Process(env_gate_);
                gran_out_left = gran_out_left * (velocity_ / 127.f) * amp * 3.0; // 3.0 is volume increase
                gran_out_right = gran_out_right * (velocity_ / 127.f) * amp * 3.0;
            }
            
            out[0][i] = gran_out_left * wetMix + input * dryMix;
            out[1][i] = gran_out_right * wetMix + input* dryMix; 

        }
    }
}

void OnNoteOff(float notenumber, float velocity)
{
    if (synth_mode==1) {
        env_gate_ = false;

    } else if (synth_mode == 2) {
        voice_handler.OnNoteOff(notenumber, velocity);
    }
}

void OnNoteOn(float notenumber, float velocity)
{
    // Note Off can come in as Note On w/ 0 Velocity
    if(velocity == 0.f)
    {
        OnNoteOff(notenumber, velocity);
    }
    else
    {
        if (synth_mode == 0) {
            // Do nothing

        } else if (synth_mode == 1) {
            note_     = notenumber;  // Treating middle c (midi 60) as neutral, pitch shift up or down from there
            velocity_ = velocity;
            env_gate_ = true;

        } else {
            voice_handler.OnNoteOn(notenumber, velocity);
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
            OnNoteOn(p.note, p.velocity);
        }
        break;

        case NoteOff:
        {
            NoteOffEvent p = m.AsNoteOff();
            OnNoteOff(p.note, p.velocity);
        }
        break;

        case ControlChange:
        {

            ControlChangeEvent p = m.AsControlChange();
            float val = (float)p.value / 127.0f;
            switch(p.control_number)
            {   
                
                // Knob Controls //////////////////////////////
                case 14: {
                    midi_control[0] = true;
                    expression_control = false;
                    knobValues[0] = ((float)p.value / 127.0f);
                    break;
                }
                case 15: {
                    midi_control[1] = true;
                    knobValues[1] = ((float)p.value / 127.0f);
                    break;
                }
                case 16: {
                    midi_control[2] = true;
                    knobValues[2] = ((float)p.value / 127.0f);
                    break;
                }
                case 17: {
                    midi_control[3] = true;
                    knobValues[3] = ((float)p.value / 127.0f);
                    break;
                }
                case 18: {
                    midi_control[4] = true;
                    knobValues[4] = ((float)p.value / 127.0f);
                    break;
                }
                case 19: {
                    midi_control[5] = true;
                    knobValues[5] = ((float)p.value / 127.0f);
                    break;
                }


                // FM Synth Controls ///////////////////////////

                // Operater Levels and Ratios
                case 20: {
                    voice_handler.SetCarrierLevel(val / 0.5);  // 0.5 for volume reduction
                    break; 


                }
                case 21: {

                    float temp = val * 32; // scale 0 to 32
                    int temp2 = static_cast<int>(temp); // round by converting to int
                    float temp3 = static_cast<float>(temp2);
                    float float_val = temp3 / 2.0 + 0.5; // 0 to 16 increments of 0.5

                    voice_handler.SetCarrierRatio(float_val);
                    break;
                }
                case 22: {
                    voice_handler.SetModulatorLevel((val / 2.0) * (val / 2.0)); // exponential and reduced max level
                    break;
                }
                case 23: {
                    float temp = val * 32; // scale 0 to 32
                    int temp2 = static_cast<int>(temp); // round by converting to int
                    float temp3 = static_cast<float>(temp2);
                    float float_val = temp3 / 2.0 + 0.5; // 0 to 16 increments of 0.5, 
                    voice_handler.SetModulatorRatio(float_val);
                    break;
                }
                // Modulator ADSR
                case 24: {
                    voice_handler.SetModAttack(val * val + 0.0001); // exponential
                    break;
                }
                case 25: {
                    voice_handler.SetModDecay(val * val + 0.0001); // exponential
                    break;
                }
                case 26: {
                    voice_handler.SetModSustain(val + 0.001);
                    break;
                }
                case 27: {
                    voice_handler.SetModRelease(val + 0.001);
                    break;
                }

                // Carrier ADSR or Granular Synth ADSR, based on the synth mode setting (3rd toggle)
                case 28: {
                    if (synth_mode == 2) {
                        voice_handler.SetCarrierAttack(val * val + 0.0001);
                    } else if (synth_mode == 1) {
                        env_.SetTime(ADSR_SEG_ATTACK, val * val + 0.0001);
                    }
                    break;
                }
                case 29: {
                    if (synth_mode == 2) {
                        voice_handler.SetCarrierDecay(val * val + 0.0001);
                    } else if (synth_mode == 1) {
                        env_.SetTime(ADSR_SEG_DECAY, val * val + 0.0001);
                    }
                    break;
                }
                case 30: {
                    if (synth_mode == 2) {
                        voice_handler.SetCarrierSustain(val + 0.001);
                    } else if (synth_mode == 1) {
                        env_.SetSustainLevel(val + 0.01);
                    }
                    break;
                }
                case 31: {
                    if (synth_mode == 2) {
                        voice_handler.SetCarrierRelease(val + 0.001);
                    } else if (synth_mode == 1) {
                        env_.SetTime(ADSR_SEG_RELEASE, val + 0.001);
                    }
                    break;
                }

                default: { break; }
            }
            break;
        }
        default: { break; }
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
    counter = 0;


    for(int i = 0; i < MAX_SAMPLE; i++) { // hard coding sample length for now
        audioSample[i] = 0.;
    }

    /** Initializes the GranularPlayer module.
        \param sample pointer to the sample to be played
        \param size number of elements in the sample array
        \param sample_rate audio engine sample rate
    */
    // Using a 2 GranularPlayers, or 4 individual grains.
    // 10 (20 total grains) seems to be hitting a processing limit
    // Can handle more that 2 granular players, but going for a 
    // more sparse sound with this effect.
    swarm[0].Init(audioSample, MAX_SAMPLE, samplerate, 0.0, 0.5);
    swarm[1].Init(audioSample, MAX_SAMPLE, samplerate, 0.25, 0.75);
    //swarm[2].Init(audioSample, MAX_SAMPLE, samplerate, 0.125, 0.625);
    //swarm[3].Init(audioSample, MAX_SAMPLE, samplerate, 0.375, 0.875);


    // monophonic granular synth envelope
    env_.Init(samplerate);
    env_.SetSustainLevel(0.5f);
    env_.SetTime(ADSR_SEG_ATTACK, 0.005f);
    env_.SetTime(ADSR_SEG_DECAY, 0.005f);
    env_.SetTime(ADSR_SEG_RELEASE, 0.2f);

    voice_handler.Init(samplerate);

    voice_handler.SetModulatorRatio(16.0);
    voice_handler.SetCarrierRatio(1.0);
    voice_handler.SetModulatorLevel(0.10);
    voice_handler.SetCarrierLevel(0.5);

    voice_handler.SetCarrierAttack(0.005);
    voice_handler.SetCarrierDecay(0.005);
    voice_handler.SetCarrierSustain(0.5);
    voice_handler.SetCarrierRelease(0.2);

    voice_handler.SetModAttack(0.005);
    voice_handler.SetModDecay(0.005);
    voice_handler.SetModSustain(0.5);
    voice_handler.SetModRelease(0.2);


    alternateMode = false;

    // Start with some stereo spread by default (TODO Maybe change this later)
    for (int j = 0; j < 2; j++) {
        swarm[j].setStereoSpread(0.4);
    }


    pmix = 0.5;            
    wetMix = 0.7;
    dryMix = 0.7; 

    // LFO For Pitch Modulation
    LFO.Init(samplerate);
    LFO.SetFreq(0.25); // try range 0 to 5 ( TODO Try exponential here? for more lower freq range)
    LFO.SetWaveform(0); // Sine wave
    LFO.SetAmp(2.0); // 
    LFO_depth = 0.0;   // 0 to 200 (2 semitones)

    ppitch_raw = 0.5; // the knob value for pitch param
    vpitch = vpitch1 = vpitch2 = 0.0;  // the actual pitch in cents used to modify granular pitch (-1200 to 1200)

    vspeed = 1.0;  // the grain speed used by the granularplayer
    pspeed_raw = 0.5;

    grainsize.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    mix.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    pitch.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR); 
    feedback.Init(hw.knob[Funbox::KNOB_4], 0.01f, 1.0f, Parameter::LINEAR);
    width.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR); 
    speed.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR);
    expression.Init(hw.expression, 0.0f, 1.0f, Parameter::LINEAR); 


    // Init the LEDs and set activate bypass
    led1.Init(hw.seed.GetPin(Funbox::LED_1),false);
    led1.Update();
    bypass = true;

    led2.Init(hw.seed.GetPin(Funbox::LED_2),false);
    led2.Update();
    hold = false;

    hw.InitMidi();
    hw.midi.StartReceive();

    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    while(1)
    {
        hw.midi.Listen();
        // Handle MIDI Events
        while(hw.midi.HasEvents())
        {
            HandleMidiMessage(hw.midi.PopEvent());
        }

    }
}