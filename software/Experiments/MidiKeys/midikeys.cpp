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
Parameter structure, brightness, level, damping, verbtime, verbdamp;

bool            bypass;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];

StringVoice   stringvoice;
ModalVoice   modalvoice;
ReverbSc     verb;
int mode = 0; // 0=modalvoice 1=stringvoice

bool first_start=true;


Led led1, led2;

// Midi
bool midi_control[6]; // knobs 0-5
float pknobValues[6];
float knobValues[6];

bool knobMoved(float old_value, float new_value)
{
    float tolerance = 0.005;
    if (new_value > (old_value + tolerance) || new_value < (old_value - tolerance)) {
        return true;
    } else {
        return false;
    }
}


void updateSwitch1() // left=, center=, right=
{
    if (pswitch1[0] == true) {  // left
        mode = 0;
    } else if (pswitch1[1] == true) {  // right
        mode = 1;

    } else {   // center
        mode = 1;
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

    first_start=false;

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

    float vlevel;

    // Knob 1
    if (!midi_control[0])   // If not under midi control, use knob ADC
        pknobValues[0] = knobValues[0] = structure.Process();
    else if (knobMoved(pknobValues[0], structure.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[0] = false;

    // Knob 2
    if (!midi_control[1])   // If not under midi control, use knob ADC
        pknobValues[1] = knobValues[1] = brightness.Process();
    else if (knobMoved(pknobValues[1], brightness.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[1] = false;

    // Knob 3
    if (!midi_control[2])   // If not under midi control, use knob ADC
        pknobValues[2] = knobValues[2] = level.Process();
    else if (knobMoved(pknobValues[2], level.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[3] = false;

    // Knob 4
    if (!midi_control[3])   // If not under midi control, use knob ADC
        pknobValues[3] = knobValues[3] = damping.Process();
    else if (knobMoved(pknobValues[3], damping.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[3] = false;

    // Knob 5
    if (!midi_control[4])   // If not under midi control, use knob ADC
        pknobValues[4] = knobValues[4] = verbtime.Process();
    else if (knobMoved(pknobValues[4], verbtime.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[4] = false;

    // Knob 6
    if (!midi_control[5])   // If not under midi control, use knob ADC
        pknobValues[5] = knobValues[5] = verbdamp.Process();
    else if (knobMoved(pknobValues[5], verbdamp.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[5] = false;

    // Handle Knob Changes Here
    vlevel = knobValues[2];

    modalvoice.SetStructure(knobValues[0]);
    modalvoice.SetBrightness(knobValues[1]);
    modalvoice.SetDamping(knobValues[3]);

    stringvoice.SetStructure(knobValues[0]);
    stringvoice.SetBrightness(knobValues[1]);
    stringvoice.SetDamping(knobValues[3]);

    verb.SetFeedback(.4 + (1.0 - .4) * knobValues[4]);
    verb.SetLpFreq(300 + (18000 - 300) * (1.0 - knobValues[5] * knobValues[5]));


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


            float voice_out = 0.0;
            if (mode == 0)
                voice_out = modalvoice.Process();
            else {
                voice_out = stringvoice.Process();
            }

            float wetl, wetr;
            verb.Process(voice_out, voice_out, &wetl, &wetr);


            // Final mix
            out[0][i] = (voice_out + wetl) * vlevel * 0.1;
            out[1][i] = (voice_out + wetr) * vlevel * 0.1;

        }
    }
}

void OnNoteOn(float notenumber, float velocity)
{
    // Note Off can come in as Note On w/ 0 Velocity
    if(velocity == 0.f)
    {
        
    }
    else
    {
        led2.Set(1.0);
        // Using velocity for accent setting (striking the resonator harder)
        modalvoice.SetAccent(velocity/128.0);
        modalvoice.SetFreq(mtof(notenumber));
        modalvoice.Trig();

        stringvoice.SetAccent(velocity/128.0);
        stringvoice.SetFreq(mtof(notenumber));
        stringvoice.Trig();
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


    structure.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    brightness.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    level.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR); 
    damping.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::LINEAR);
    verbtime.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    verbdamp.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR); 

    modalvoice.Init(samplerate);
    stringvoice.Init(samplerate);
    verb.Init(samplerate);

    // Init the LEDs and set activate bypass
    led1.Init(hw.seed.GetPin(Funbox::LED_1),false);
    led1.Update();
    bypass = true;

    led2.Init(hw.seed.GetPin(Funbox::LED_2),false);
    led2.Update();

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