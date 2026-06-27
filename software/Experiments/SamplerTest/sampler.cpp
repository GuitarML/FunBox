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
Parameter trim1, trim2, attack, decay, sustain, release;


bool force_reset = false;

bool            bypass;
bool            hold;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];



#define MAX_SAMPLE static_cast<int>(48000.0 * 10.0) // 10 second sample
#define MAX_SAMPLE_SIZET static_cast<size_t>(MAX_SAMPLE) 
float DSY_SDRAM_BSS audioSample[MAX_SAMPLE_SIZET];

//float audioSample[MAX_SAMPLE_SIZET];  

bool recording = false;

int current_sample_size = 0;


int recording_sample_index = 0;
bool trigger;
int fade_length;  // fade in/fade out audio sample by this many individual samples


float middleC = 261.6256;

// global midi key values for granular synth
//float note_ = 0.0;
//float velocity_ = 0.0;


bool first_start=true;

Led led1, led2;

// Midi
bool midi_control[6]; // knobs 0-5
float pknobValues[6];
float knobValues[6];



int counter = 0;


class Voice {
  public:
    Voice() {}
    ~Voice() {}
    void Init(float samplerate) {
        active_ = false;

        env_.Init(samplerate);
        env_.SetSustainLevel(0.4f);
        env_.SetTime(ADSR_SEG_ATTACK, 0.005f);
        env_.SetTime(ADSR_SEG_DECAY, 0.08f);
        env_.SetTime(ADSR_SEG_RELEASE, 0.2f);

        playhead = 0.0;
        play_speed = 1.0;

    }

    // Process a single sample
    float getNextSample() {
        if (current_sample_size == 0) return 0.0f;

        // If we get to the end of the sample, reset
        if (playhead >= static_cast<float>(current_sample_size)) {
            playhead = 0.0;
            active_ = false; // for now, end sample playback at end of sample, TODO make a wrap around option
        }

        // Separate integer and fractional parts
        size_t index = static_cast<size_t>(playhead);
        float fraction = playhead - index;

        // Get current and next samples (handle boundary protection)
        float sample1 = audioSample[index];
        float sample2 = (index + 1 < static_cast<float>(current_sample_size)) ? audioSample[index + 1] : 0.0f;

        // Linear Interpolation
        float output = sample1 + static_cast<float>(fraction) * (sample2 - sample1);

        // Advance the playhead
        playhead += play_speed;

        return output;
    }

    float Process() {
        if (active_) {
            float sig, amp;
            amp = env_.Process(env_gate_);
            if (!env_.IsRunning()) {
                active_ = false;
                playhead = 0.0;
            }

            return getNextSample() * (velocity_ / 127.f) * amp;
        }
        return 0.f;
    }

    void OnNoteOn(float note, float velocity) {
        note_ = note;
        velocity_ = velocity;

        active_ = true;
        env_gate_ = true;
        play_speed = mtof(note_) / middleC;

    }

    void OnNoteOff() { env_gate_ = false; }


    inline bool IsActive() const { return active_; }
    inline float GetNote() const { return note_; }

  private:

    Adsr env_;
    float note_, velocity_;
    bool active_;
    bool env_gate_;
    //int sampleIndex;
    float play_speed;
    float playhead;
};

template <size_t max_voices> class VoiceManager {
  public:
    VoiceManager() {}
    ~VoiceManager() {}

    void Init(float samplerate) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].Init(samplerate);
        }
    }

    float Process() {
        float sum;
        sum = 0.f;
        for (size_t i = 0; i < max_voices; i++) {
            sum += voices[i].Process();
        }
        return sum;
    }

    void OnNoteOn(float notenumber, float velocity) {
        Voice *v = FindFreeVoice();
        if (v == NULL)
            return;
        v->OnNoteOn(notenumber, velocity);
    }

    void OnNoteOff(float notenumber, float velocity) {
        for (size_t i = 0; i < max_voices; i++) {
            Voice *v = &voices[i];
            if (v->IsActive() && v->GetNote() == notenumber) {
                v->OnNoteOff();
            }
        }
    }

    void FreeAllVoices() {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].OnNoteOff();
        }
    }



  private:
    Voice voices[max_voices];
    Voice *FindFreeVoice() {
        Voice *v = NULL;
        for (size_t i = 0; i < max_voices; i++) {
            if (!voices[i].IsActive()) {
                v = &voices[i];
                break;
            }
        }
        return v;
    }
};


// Sampler
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

// Switches1 live mode, sample mode, dry sample loop mode
void updateSwitch1() 
{
    if (pswitch1[0] == true) { 



    } else if (pswitch1[1] == true) {  



    } else {  


    }   
}

void updateSwitch2() // envelope modes
{
    if (pswitch2[0] == true) {         // left  cosine grain envelope

    } else if (pswitch2[1] == true) {  // right   fast attack-decay grain envelope

    } else {                           // center  attack- fast decay grain envelope

    }


}


void updateSwitch3() // left=, center=, right=
{
    if (pswitch3[0] == true) {  // left

        

    } else if (pswitch3[1] == true) {  // right

        

    } else {   // center

    } 

    force_reset = true;   
}


void UpdateButtons()
{

    if(hw.switches[Funbox::FOOTSWITCH_1].RisingEdge())
    {

        recording = true;
        led1.Set(1.0f);

    } 

    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {
        recording = false;
        recording_sample_index = 0;
        led1.Set(0.0f);
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
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    UpdateButtons();
    UpdateSwitches();

//Parameter trim1, trim2, attack, decay, sustain, release;

    float vtrim1 = trim1.Process();
    float vtrim2 = trim2.Process();
    float vattack = attack.Process();
    float vdecay = decay.Process();
    float vsustain = sustain.Process();
    float vrelease = release.Process();



    // Handle Knob Changes Here

   

    first_start=false;
    force_reset = false;

    // Process the Audio Buffer //
    for(size_t i = 0; i < size; i++)
    {
        // Process your signal here
        if(bypass)
        {
            
            out[0][i] = out[1][i] = in[0][i];  // MISO when in bypass

        } else {   
            
            float input = in[0][i];
            
            // Record input audio while footswitch is held, or until max buffer is reached
            if (recording) {
                audioSample[recording_sample_index] = input;
                current_sample_size = recording_sample_index;
                recording_sample_index++;
                
                if (recording_sample_index >= MAX_SAMPLE) {
                    recording = false;
                    recording_sample_index = 0;
                    current_sample_size = MAX_SAMPLE;
                    led1.Set(0.0f);
                }

            }


            float sum        = 0.f;
            sum        = voice_handler.Process();

            
            out[0][i] = sum + input;
            out[1][i] = sum + input; 

        }
    }
}

void OnNoteOff(float notenumber, float velocity)
{

    voice_handler.OnNoteOff(notenumber, velocity);

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
        voice_handler.OnNoteOn(notenumber, velocity);

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



    counter = 0;

    // I dont think this is needed after moving buffer from SDRAM to SRAM TODO Verify that
    for(int i = 0; i < MAX_SAMPLE; i++) { // hard coding sample length for now
        audioSample[i] = 0.;
    }




    voice_handler.Init(samplerate);

    trim1.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    trim2.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    attack.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR); 
    decay.Init(hw.knob[Funbox::KNOB_4], 0.01f, 1.0f, Parameter::LINEAR);
    sustain.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR); 
    release.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR);


    // Init the LEDs and set activate bypass
    led1.Init(hw.seed.GetPin(Funbox::LED_1),false);
    led1.Update();
    bypass = false;

    led2.Init(hw.seed.GetPin(Funbox::LED_2),false);
    led2.Update();




    // Midi
    for( int i = 0; i < 6; ++i ) 
        midi_control[i] = false;  

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