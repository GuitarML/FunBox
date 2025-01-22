#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"
#include "operator.h"

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
Parameter level, modlevel, modratio, attack, sustain, release, expression;

bool            bypass;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];


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

float led2_brightness = 0.0;

class Voice
{
  public:
    Voice() {}
    ~Voice() {}
    void Init(float samplerate)
    {
        active_ = false;
        op1_.Init(samplerate, false); // op1 ratio is set to 1.0 on initialization and doesn't change (design choice for control simplicity)
        op2_.Init(samplerate, true);

        env_.Init(samplerate);
        env_.SetSustainLevel(0.5f);
        env_.SetTime(ADSR_SEG_ATTACK, 0.005f);
        env_.SetTime(ADSR_SEG_DECAY, 0.005f);
        env_.SetTime(ADSR_SEG_RELEASE, 0.2f);

        env2_.Init(samplerate);
        env2_.SetSustainLevel(0.5f);
        env2_.SetTime(ADSR_SEG_ATTACK, 0.005f);
        env2_.SetTime(ADSR_SEG_DECAY, 0.005f);
        env2_.SetTime(ADSR_SEG_RELEASE, 0.2f);

    }

    float Process()
    {
        if(active_)
        {
            float sig, sig2, amp, amp2;
            amp = env_.Process(env_gate_);   // Carrier envelope
            amp2 = env2_.Process(env_gate_); // Modulator envelope
            if(!env_.IsRunning())
                active_ = false;
            sig = op1_.Process();
            op2_.setPhaseInput(sig * amp2);
            sig2 = op2_.Process();

            return sig2 * amp;
        }
        return 0.f;
    }

    void OnNoteOn(float note, float velocity)
    {
        note_     = note;
        velocity_ = velocity;
        op1_.OnNoteOn(note, velocity);
        op2_.OnNoteOn(note, velocity);
        active_   = true;
        env_gate_ = true;
    }

    void OnNoteOff() { env_gate_ = false; }

    void SetModulatorRatio(float val) { op1_.SetRatio(val); }

    void SetCarrierRatio(float val) { op2_.SetRatio(val); }

    void SetModulatorLevel(float val) { op1_.SetLevel(val); }

    void SetCarrierLevel(float val) { op2_.SetLevel(val); }



    void SetCarrierAttack(float val) { env_.SetTime(ADSR_SEG_ATTACK, val); }

    void SetCarrierDecay(float val) { env_.SetTime(ADSR_SEG_DECAY, val); }

    void SetCarrierSustain(float val) { env_.SetSustainLevel(val); }

    void SetCarrierRelease(float val) { env_.SetTime(ADSR_SEG_RELEASE, val); }


    void SetModAttack(float val) { env2_.SetTime(ADSR_SEG_ATTACK, val); }

    void SetModDecay(float val) { env2_.SetTime(ADSR_SEG_DECAY, val); }

    void SetModSustain(float val) { env2_.SetSustainLevel(val); }

    void SetModRelease(float val) { env2_.SetTime(ADSR_SEG_RELEASE, val); }


    inline bool  IsActive() const { return active_; }
    inline float GetNote() const { return note_; }

  private:
    //Oscillator osc_;
    Operator   op1_;
    Operator   op2_;
    Adsr       env_;
    Adsr       env2_;
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

    void SetModulatorLevel(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetModulatorLevel(all_val);
        }
    }

    void SetModulatorRatio(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetModulatorRatio(all_val);
        }
    }

    void SetCarrierLevel(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetCarrierLevel(all_val);
        }
    }

    void SetCarrierRatio(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetCarrierRatio(all_val);
        }
    }


    void SetCarrierAttack(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetCarrierAttack(all_val);
        }
    }

    void SetCarrierDecay(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetCarrierDecay(all_val);
        }
    }

    void SetCarrierSustain(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetCarrierSustain(all_val);
        }
    }

    void SetCarrierRelease(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetCarrierRelease(all_val);
        }
    }



    void SetModAttack(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetModAttack(all_val);
        }
    }

    void SetModDecay(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetModDecay(all_val);
        }
    }

    void SetModSustain(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetModSustain(all_val);
        }
    }

    void SetModRelease(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetModRelease(all_val);
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



static VoiceManager<18> voice_handler;  //  moved param changing to outside function, can run 18 (125B pedal SRAM, can Flash handle more?)




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

    // (De-)Activate bypass and toggle LED when left footswitch is let go, or enable/disable amp if held for greater than 1 second //
    // Can only disable/enable amp when not in bypass mode
    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {
        bypass = !bypass;
        led1.Set(bypass ? 0.0f : 1.0f);
    }


    led1.Update();


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
        pknobValues[0] = knobValues[0] = level.Process();
    } else if (knobMoved(pknobValues[0], level.Process())) {  // If midi controlled, watch for knob movement to end Midi control
        midi_control[0] = false;
    }

    // Knob 2
    if (!midi_control[1]) { // If not under midi control, use knob ADC
        pknobValues[1] = knobValues[1] = modlevel.Process();
    } else if (knobMoved(pknobValues[1], modlevel.Process())) { // If midi controlled, watch for knob movement to end Midi control
        midi_control[1] = false;
        expression_control = false;
    }

    // Knob 3
    if (!midi_control[2])   // If not under midi control, use knob ADC
        pknobValues[2] = knobValues[2] = modratio.Process();
    else if (knobMoved(pknobValues[2], modratio.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[3] = false;

    // Knob 4
    if (!midi_control[3])   // If not under midi control, use knob ADC
        pknobValues[3] = knobValues[3] = attack.Process();
    else if (knobMoved(pknobValues[3], attack.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[3] = false;

    // Knob 5
    if (!midi_control[4])   // If not under midi control, use knob ADC
        pknobValues[4] = knobValues[4] = sustain.Process();
    else if (knobMoved(pknobValues[4], sustain.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[4] = false;

    // Knob 6
    if (!midi_control[5])   // If not under midi control, use knob ADC
        pknobValues[5] = knobValues[5] = release.Process();
    else if (knobMoved(pknobValues[5], release.Process()))  // If midi controlled, watch for knob movement to end Midi control
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

    // Knob 1 is Carrier level (overall volume output)
    voice_handler.SetCarrierLevel(knobValues[0]);

    // Knob 2 is Modulator level
    voice_handler.SetModulatorLevel(knobValues[1]);

    // Knob 3 is Modulator Ratio, 0.5 to 16 by increments of 0.5
    float temp = knobValues[2] * 2; // scale 0 to 16... 0 to 32
    int temp2 = static_cast<int>(temp); // round by converting to int
    float temp3 = static_cast<float>(temp2);
    float float_val = temp3 / 2.0; // 0 to 16 increments of 0.5
    voice_handler.SetModulatorRatio(float_val);  

    // Knob 4 is labeled "Attack", but here controls both attack and release time of both modulator and carrier.
    //  Design choice for control simplicity. Could split out these controls to have a larger range of sounds.
    voice_handler.SetCarrierAttack(knobValues[3]);
    voice_handler.SetModAttack(knobValues[3]);
    voice_handler.SetCarrierDecay(knobValues[3]);
    voice_handler.SetModDecay(knobValues[3]);

    // Knob 5 is sustain level for both the carrier and modulator
    voice_handler.SetCarrierSustain(knobValues[4]);
    voice_handler.SetModSustain(knobValues[4]);

    // Knob 6 is release time for both carrier and modulator
    voice_handler.SetCarrierRelease(knobValues[5]);
    voice_handler.SetModRelease(knobValues[5]);  

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
            // Something to do with the right LED, sets brightness to 1.0 when key pressed,
            //   fades back to 0.0 over time.
            fonepole(led2_brightness, 0.0, .00002f * knobValues[3]); // TODO I think this will decay the brightness about the same rate as the note
            led2.Set(led2_brightness);
            led2.Update();

            float sum        = 0.f;
            sum        = voice_handler.Process() * 0.6f; // 0.6 for volume reduction
            out[0][i] = out[1][i]   = sum; 

        }
    }
}

void OnNoteOn(float notenumber, float velocity)
{
    // Note Off can come in as Note On w/ 0 Velocity
    if(velocity == 0.f)
    {
        voice_handler.OnNoteOff(notenumber, velocity);
    }
    else
    {
        voice_handler.OnNoteOn(notenumber, velocity);
    }
}

void OnNoteOff(float notenumber, float velocity)
{

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
            led2_brightness = 1.0; // A fun "fading led" every time a note is pressed
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


    level.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    modlevel.Init(hw.knob[Funbox::KNOB_2], 0.0f, 0.6f, Parameter::EXPONENTIAL);
    modratio.Init(hw.knob[Funbox::KNOB_3], 0.5f, 16.0f, Parameter::LINEAR); 
    attack.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::EXPONENTIAL);
    sustain.Init(hw.knob[Funbox::KNOB_5], 0.01f, 1.0f, Parameter::LINEAR);
    release.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR); 
    expression.Init(hw.expression, 0.0f, 1.0f, Parameter::LINEAR); 

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