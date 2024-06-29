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
Parameter structure, brightness, level, damping, verbtime, verbdamp, expression;

bool            bypass;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];

StringVoice   stringvoice;
ModalVoice   modalvoice;
ReverbSc     verb;
int mode = 0; // 0=modalvoice 1=stringvoice 2=synth

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

// Delay
#define MAX_DELAY static_cast<size_t>(48000 * 3.f + 1000) // 3 second max delay
DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayLine;

struct delay
{
    DelayLine<float, MAX_DELAY> *del;
    float                        currentDelay;
    float                        delayTarget;
    float                        feedback;
    float                        active = false;
    
    float Process(float in)
    {
        //set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        del->SetDelay(currentDelay);

        float read = del->Read();
        if (active) {
            del->Write((feedback * read) + in);
        } else {
            del->Write((feedback * read)); // if not active, don't write any new sound to buffer
        }

        return read;
    }
};

delay             delay1;


class Voice
{
  public:
    Voice() {}
    ~Voice() {}
    void Init(float samplerate)
    {
        active_ = false;
        osc_.Init(samplerate);
        osc_.SetAmp(0.75f);
        osc_.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
        env_.Init(samplerate);
        env_.SetSustainLevel(0.5f);
        env_.SetTime(ADSR_SEG_ATTACK, 0.005f);
        env_.SetTime(ADSR_SEG_DECAY, 0.005f);
        env_.SetTime(ADSR_SEG_RELEASE, 0.2f);
        filt_.Init(samplerate);
        filt_.SetFreq(6000.f);
        filt_.SetRes(0.6f);
        filt_.SetDrive(0.8f);
    }

    float Process()
    {
        if(active_)
        {
            float sig, amp;
            amp = env_.Process(env_gate_);
            if(!env_.IsRunning())
                active_ = false;
            sig = osc_.Process();
            filt_.Process(sig);
            return filt_.Low() * (velocity_ / 127.f) * amp;
        }
        return 0.f;
    }

    void OnNoteOn(float note, float velocity)
    {
        note_     = note;
        velocity_ = velocity;
        osc_.SetFreq(mtof(note_));
        active_   = true;
        env_gate_ = true;
    }

    void OnNoteOff() { env_gate_ = false; }

    void SetCutoff(float val) { filt_.SetFreq(val); }
    //void SetSustain(float val) { env_.SetSustainLevel(val); }

    inline bool  IsActive() const { return active_; }
    inline float GetNote() const { return note_; }

  private:
    Oscillator osc_;
    Svf        filt_;
    Adsr       env_;
    float      note_, velocity_;
    bool       active_;
    bool       env_gate_;
};

template <size_t max_voices>
class VoiceManager
{
  public:
    VoiceManager() {}
    ~VoiceManager() {}

    void Init(float samplerate)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].Init(samplerate);
        }
    }

    float Process()
    {
        float sum;
        sum = 0.f;
        for(size_t i = 0; i < max_voices; i++)
        {
            sum += voices[i].Process();
        }
        return sum;
    }

    void OnNoteOn(float notenumber, float velocity)
    {
        Voice *v = FindFreeVoice();
        if(v == NULL)
            return;
        v->OnNoteOn(notenumber, velocity);
    }

    void OnNoteOff(float notenumber, float velocity)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            Voice *v = &voices[i];
            if(v->IsActive() && v->GetNote() == notenumber)
            {
                v->OnNoteOff();
            }
        }
    }

    void FreeAllVoices()
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].OnNoteOff();
        }
    }

    void SetCutoff(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetCutoff(all_val);
        }
    }


  private:
    Voice  voices[max_voices];
    Voice *FindFreeVoice()
    {
        Voice *v = NULL;
        for(size_t i = 0; i < max_voices; i++)
        {
            if(!voices[i].IsActive())
            {
                v = &voices[i];
                break;
            }
        }
        return v;
    }
};


static VoiceManager<8> voice_handler;




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
    if (!midi_control[0]) {  // If not under midi control or expression control, use knob ADC
        pknobValues[0] = knobValues[0] = structure.Process();
    } else if (knobMoved(pknobValues[0], structure.Process())) {  // If midi controlled, watch for knob movement to end Midi control
        midi_control[0] = false;
        expression_control = false;
    }

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


    float vexpression = expression.Process();
    // Expression just for knob 1
    if (knobMoved(pexpression, vexpression)) {
        pexpression = vexpression;
        if (!first_start) {
            expression_control = true;
        }
    }

    // Overwrite knob 1 with expression value
    if (expression_control) {
        knobValues[0] = vexpression;
    } 

    // Handle Knob Changes Here
    vlevel = knobValues[2];

    if (mode == 0) {
        modalvoice.SetStructure(knobValues[0]);
        modalvoice.SetBrightness(knobValues[1]);
        modalvoice.SetDamping(knobValues[3]);
    } else if (mode == 1) {
        stringvoice.SetStructure(knobValues[0]);
        stringvoice.SetBrightness(knobValues[1]);
        stringvoice.SetDamping(knobValues[3]);
    } else {
        voice_handler.SetCutoff(250 + knobValues[0] * (8500 -  250));
        //voice_handler.SetSustain(knobValues[1]);
    }
    verb.SetFeedback(.4 + (1.0 - .4) * knobValues[4]);
    verb.SetLpFreq(300 + (18000 - 300) * (1.0 - knobValues[5] * knobValues[5]));

    if (knobValues[5] < 0.01) {   // if knob < 1%, set delay to inactive
        delay1.active = false;
    } else {
        delay1.active = true;
    }

    delay1.delayTarget = knobValues[5] * 144000; // in samples 0 to 3 second range  
    delay1.feedback = knobValues[4];

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


            float voice_out = 0.0;
            if (mode == 0)
                voice_out = modalvoice.Process();
            else if (mode == 1) 
                voice_out = stringvoice.Process();
            else
                voice_out = voice_handler.Process() * 0.5f; 

            if (effect_mode == 0) {
                float wetl, wetr;
                verb.Process(voice_out, voice_out, &wetl, &wetr);
                out[0][i] = (voice_out + wetl) * vlevel * 0.2;
                out[1][i] = (voice_out + wetr) * vlevel * 0.2;

            } else {
                float delay_out = delay1.Process(voice_out);  
                out[0][i] = (voice_out + delay_out) * vlevel * 0.2;
                out[1][i] = (voice_out + delay_out) * vlevel * 0.2;
            }

        }
    }
}

void OnNoteOn(float notenumber, float velocity)
{
    // Note Off can come in as Note On w/ 0 Velocity
    if(velocity == 0.f)
    {
        led2.Set(0.0);
        if (mode==2)
            voice_handler.OnNoteOff(notenumber, velocity);
    }
    else
    {
        led2.Set(0.5); // Something for the right LED to do
        if (mode==0) {
            // Using velocity for accent setting (striking the resonator harder)
            modalvoice.SetAccent(velocity/128.0);
            modalvoice.SetFreq(mtof(notenumber));
            modalvoice.Trig();
        } else if (mode == 1) {
            stringvoice.SetAccent(velocity/128.0);
            stringvoice.SetFreq(mtof(notenumber));
            stringvoice.Trig();
        } else {
            voice_handler.OnNoteOn(notenumber, velocity);
        }
    }
}

void OnNoteOff(float notenumber, float velocity)
{

    led2.Set(0.0);
    if (mode==2)
        voice_handler.OnNoteOff(notenumber, velocity);

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
            switch(p.control_number)
            {   


                case 14:
                    midi_control[0] = true;
                    expression_control = false;
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
    damping.Init(hw.knob[Funbox::KNOB_4], 0.0f, 0.45f, Parameter::LINEAR); // limiting amount to 0.45 for ear safety, can hit resonance above that
    verbtime.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    verbdamp.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR); 
    expression.Init(hw.expression, 0.0f, 1.0f, Parameter::LINEAR); 

    modalvoice.Init(samplerate);
    stringvoice.Init(samplerate);
    verb.Init(samplerate);

    delayLine.Init();
    delay1.del = &delayLine;
    delay1.delayTarget = 2400; // in samples
    delay1.feedback = 0.0;
    delay1.active = true;   

    voice_handler.Init(samplerate);


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