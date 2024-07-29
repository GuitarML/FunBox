#include <string.h>
#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"
#include "expressionHandler.h"

#include <cmath>
#include <complex>
#include "shy_fft.h"

#include "fourier.h"
#include "wave.h"

#define PI 3.1415926535897932384626433832795

using namespace daisy;
using namespace daisysp;
using namespace funbox;  // This is important for mapping the correct controls to the Daisy Seed on Funbox PCB
using namespace soundmath;

// Declare a local daisy_petal for hardware access
DaisyPetal hw;
Parameter decay, mix, damp, shimmer, shimmer_tone, detune, expression;

float samplerate = 32000; // making global

bool            bypass;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];


Led led1, led2;

// Expression
ExpressionHandler expHandler;
bool expression_pressed;
// Control Values
float knobValues[6];

// Midi
bool midi_control[6]; //  just knobs for now
float pknobValues[6]; // Used for Midi control logic

SampleRateReducer samplerateReducer;
Tone lowpass;       // Low Pass for lofi mode
int reverb_mode = 0;

// convenient lookup tables
Wave<float> hann([] (float phase) -> float { return 0.5 * (1 - cos(2 * PI * phase)); });
Wave<float> halfhann([] (float phase) -> float { return sin(PI * phase); });


// 4 overlapping windows of size 2^12 = 4096
// `N = 4096` and `laps = 4` (higher frequency resolution, greater latency), or when `N = 2048` and `laps = 8` (higher time resolution, less latency). 
const size_t order = 12;
//const size_t order = 11;
const size_t N = (1 << order);
const float sqrtN = sqrt(N);
const size_t laps = 4;
//const size_t laps = 8;
const size_t buffsize = 2 * laps * N; 

// buffers for STFT processing
// audio --> in --(fft)--> middle --(process)--> out --(ifft)--> in -->
// each of these is a few circular buffers stacked end-to-end.
float in[buffsize]; // buffers for input and output (from / to user audio callback)
float middle[buffsize]; // buffers for unprocessed frequency domain data
float out[buffsize]; // buffers for processed frequency domain data // TODO Try doubling the outbuffer and creating a MISO (mono in stereo out) reverb

float reverb_energy[N/2];

ShyFFT<float, N, RotationPhasor>* fft; // fft object
Fourier<float, N>* stft; // stft object

float fft_size = N / 2; // moved to global

float vdecay, vmix, vdamp,vshimmer,vshimmer_tone, vdetune; // making these global
float octave_up_rate_persecond, octave_up_rate_perinterval, shimmer_double, shimmer_triple, shimmer_remainder;
float detune_rate_persecond, detune_rate_perinterval, detune_double, detune_remainder;

float window_samples = 32768; // This is the buffsize
float interval_samples = ceil(window_samples/laps);

bool freeze = false;
int shimmer_mode = 0;

int detune_mode = 1;
int detune_multiplier = 1;

bool first_start = true;


int drift_mode = 1;
Oscillator drift_osc;
Oscillator drift_osc2;
Oscillator drift_osc3;
Oscillator drift_osc4;
float drift_multiplier = 1.0;
float drift_multiplier2 = 1.0;
float drift_multiplier3 = 1.0;
float drift_multiplier4 = 1.0;

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
    if (pswitch1[0] == true) {  // left  octave down
        shimmer_mode = 0;  
    } else if (pswitch1[1] == true) {  // right  octave up and octave down
        shimmer_mode = 2;

    } else {   // center octave up
        shimmer_mode = 1;  
    }      
}

void updateSwitch2() // left=, center=, right=
{
    if (pswitch2[0] == true) {  // left less lofi 
        reverb_mode = 0;
        samplerateReducer.SetFreq(0.3);     
        lowpass.SetFreq(8000.0);

    } else if (pswitch2[1] == true) {  // right more lofi
        reverb_mode = 2;
        samplerateReducer.SetFreq(0.2);  

    } else {   // center  normal
        reverb_mode = 1;

    }    
}

void updateSwitch3() // left=detune down, center= no detune, right=detune up
{
    if (pswitch3[0] == true) {  // left  slower drift 
        drift_mode = 0; 

       // damp, shimmer=osc2, shimmertone=osc3, detune=4
       // each oscillator frequency is slightly offset from eachother to create evolving soundscapes that don't repeat 

        drift_osc.SetFreq(0.009); 
        drift_osc.SetWaveform(0); // WAVE_SIN = 0, WAVE_TRI = 1, WAVE_SAW = 2, WAVE_RAMP = 3, WAVE_SQUARE = 4

        drift_osc2.SetFreq(0.01);
        drift_osc2.SetWaveform(0);

        drift_osc3.SetFreq(0.011); 
        drift_osc3.SetWaveform(0);

        drift_osc4.SetFreq(0.012); 
        drift_osc4.SetWaveform(0);

    } else if (pswitch3[1] == true) {  // right  faster drift
        drift_mode = 2;
        drift_osc.SetFreq(0.020); 
        drift_osc.SetWaveform(1);

        drift_osc2.SetFreq(0.025); 
        drift_osc2.SetWaveform(1);

        drift_osc3.SetFreq(0.03); 
        drift_osc3.SetWaveform(1);

        drift_osc4.SetFreq(0.035);
        drift_osc4.SetWaveform(1);

    } else {   // center  no drift
        drift_mode = 1;
    }    
}


void UpdateButtons()
{

    // (De-)Activate bypass and toggle LED when left footswitch is let go, or enable/disable amp if held for greater than 1 second //
    // Can only disable/enable amp when not in bypass mode
    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {
        if (!expression_pressed) {
            bypass = !bypass;
            led1.Set(bypass ? 0.0f : 1.0f);
        }
        expression_pressed = false;
    }


    if (hw.switches[Funbox::FOOTSWITCH_2].Pressed()) {
        freeze = true;
    } else {
        freeze = false;
    }
    if (!expression_pressed && !expHandler.isExpressionSetMode())
        led2.Set(freeze ? 1.0f : 0.0f);


    // Toggle Expression mode by holding down left footswitch for half a second
    if(hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 600  && !expression_pressed ) {
        expHandler.ToggleExpressionSetMode();

        if (expHandler.isExpressionSetMode()) {
            led1.Set(expHandler.returnLed1Brightness()+0.3);  // Dim LEDs in expression set mode
            led2.Set(expHandler.returnLed2Brightness()+0.3);  // Dim LEDs in expression set mode

        } else {
            led1.Set(bypass ? 0.0f : 1.0f); 
            led2.Set(freeze ? 1.0f : 0.0f);
        }
        expression_pressed = true; // Keeps it from switching over and over while held
    }
        
    // Clear Expression settings by holding down left footswitch for 2 seconds
    if(hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 2000) {
        expHandler.Reset();
        led1.Set(bypass ? 0.0f : 1.0f); 
        led2.Set(freeze ? 1.0f : 0.0f);
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
    first_start = false;
}


static void ProcessControls()
{
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    UpdateButtons();
    UpdateSwitches();
 

    // Knob and Expression Processing ////////////////////

    float newExpressionValues[6];

    // Knob 1
    if (!midi_control[0])   // If not under midi control, use knob ADC
        pknobValues[0] = knobValues[0] = decay.Process();
    else if (knobMoved(pknobValues[0], decay.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[0] = false;

    // Knob 2
    if (!midi_control[1])   // If not under midi control, use knob ADC
        pknobValues[1] = knobValues[1] = mix.Process();
    else if (knobMoved(pknobValues[1], mix.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[1] = false;

    // Knob 3
    if (!midi_control[2])   // If not under midi control, use knob ADC
        pknobValues[2] = knobValues[2] = damp.Process();
    else if (knobMoved(pknobValues[2], damp.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[2] = false;

    // Knob 4
    if (!midi_control[3])   // If not under midi control, use knob ADC
        pknobValues[3] = knobValues[3] = shimmer.Process();
    else if (knobMoved(pknobValues[3], shimmer.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[3] = false;
    

    // Knob 5
    if (!midi_control[4])   // If not under midi control, use knob ADC
        pknobValues[4] = knobValues[4] = shimmer_tone.Process();
    else if (knobMoved(pknobValues[4], shimmer_tone.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[4] = false;
    

    // Knob 6
    if (!midi_control[5])  // If not under midi control, use knob ADC
        pknobValues[5] = knobValues[5] = detune.Process();
    else if (knobMoved(pknobValues[5], detune.Process()))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[5] = false;
    

    float vexpression = expression.Process(); // 0 is heel (up), 1 is toe (down)
    expHandler.Process(vexpression, knobValues, newExpressionValues);

    // If in expression set mode, set LEDS accordingly
    if (expHandler.isExpressionSetMode()) {
        led1.Set(expHandler.returnLed1Brightness()+0.3);
        led2.Set(expHandler.returnLed2Brightness()+0.3);
    }
  

    vdecay = newExpressionValues[0] * 99 + 1;;
    vmix = newExpressionValues[1];
    vdamp = newExpressionValues[2];
    vshimmer = newExpressionValues[3];
    vshimmer_tone = newExpressionValues[4]; // limiting to 0.3 based on knob range
    float vdetune_temp = newExpressionValues[5];
    ///////////////////////////////////////////////////////

    vdetune = abs(vdetune_temp);

    // Drift automation
    if (drift_mode == 0 || drift_mode == 2) {
        vdamp = vdamp * abs(drift_multiplier) * 0.7 + 0.3;  
        vshimmer *= abs(drift_multiplier2);
        vshimmer_tone *= abs(drift_multiplier3);
        vdetune *= abs(drift_multiplier4);  // If detune set to noon, this *should* have no effect
    } 

    if (vdetune > .03) { // gives a 10% knob range at noon for no detuning
        vdetune = vdetune - 0.029; // account for reduced range
        if (vdetune_temp >= 0) {
            detune_mode = 2; //detune up
            detune_multiplier = 1;
        } else {
            detune_mode = 0; //detune down
            detune_multiplier = -1;
        }
    } else {
        detune_mode = 1; // no detuning
    }
    
   
    octave_up_rate_persecond = std::pow(8, vshimmer) - 1;
    octave_up_rate_perinterval = std::min(0.75f, octave_up_rate_persecond/samplerate*interval_samples);

    // Make 5ths independent of shimmer control
    float octave_up_rate_persecond2 = std::pow(8, vshimmer_tone) - 1;  
    float octave_up_rate_perinterval2 = std::min(0.75f, octave_up_rate_persecond2/samplerate*interval_samples);

    shimmer_double = octave_up_rate_perinterval*(1 - vshimmer_tone/1.58);
    shimmer_triple = (octave_up_rate_perinterval2/1.58) * vshimmer_tone;
    shimmer_remainder = (1 - shimmer_double - shimmer_triple);

    detune_rate_persecond = std::pow(8, vdetune) - 1;
    detune_rate_perinterval = std::min(0.75f, detune_rate_persecond/samplerate*interval_samples);
    detune_double = detune_rate_perinterval;
    detune_remainder = 1 - detune_double;
}


// This runs at a fixed rate, to prepare audio samples
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{

    led1.Update();
    led2.Update();

    ProcessControls();

    for(size_t i = 0; i < size; i++)
    {
        drift_multiplier = drift_osc.Process(); // process the oscillators for drift control
        drift_multiplier2 = drift_osc2.Process();
        drift_multiplier3 = drift_osc3.Process(); 
        drift_multiplier4 = drift_osc4.Process(); 

        // Process your signal here
        if(bypass)
        {
            
            out[0][i] = in[0][i]; 
            out[1][i] = in[1][i];

        }
        else
        {   

 	    stft->write(in[0][i]); // put a new sample in the STFT

        float wet = 0.0;
        if (reverb_mode == 0) { // less lofi
            wet = lowpass.Process(samplerateReducer.Process(stft->read()));
        } else if (reverb_mode == 1) {  // normal
            wet = stft->read();     
        } else if (reverb_mode == 2) {  // more lofi
            wet = samplerateReducer.Process(stft->read());
        }

	    out[0][i] = wet * vmix + in[0][i] * (1.0 - vmix); // read the next sample from the STFT
	    out[1][i] = out[0][i]; // Mono processing for now
        }
    }
}




// shy_fft packs arrays as [real, real, real, ..., imag, imag, imag, ...]
inline void reverb(const float* in, float* out)
{
        // Reverb logic based on code by geraintluff in this forum:
        // https://forum.cockos.com/showthread.php?t=225955
        // Here I only use mono though

	// convenient constant for grabbing imaginary parts
	static const size_t offset = N / 2; // equals 2048

    float reverb_amp = 0.0;
	for (size_t i = 0; i < N / 2; i++)  // loop i from 0 to 2047
	{

            // float fft_size = N / 2; // moved to global to be accessed by ProcessControls()
            float fft_bin = i + 1;

            float real = in[i];
            float imag = in[i + offset];

            float energy = real * real + imag * imag;

            // Amplitude from energy
            reverb_amp = sqrt(reverb_energy[i]);
            if (fft_bin / fft_size > vdamp) {

                // Reduce amplitude by 1/f
                reverb_amp *= vdamp * fft_size/fft_bin;
            }


            // Add random phase reverb energy
            float random_phase = rand()*2*PI;
            real = reverb_amp * cos(random_phase);
            imag = reverb_amp * sin(random_phase);

            // If frozen, don't add new energy or decay the reverb
            if (!freeze) {
                // Add current energy to reverb
                reverb_energy[i] += energy / laps; // laps=4 "overlap factor"

                // Decay reverb
                float reverb_decay_factor = 1/vdecay;
                reverb_energy[i] *= 1.0 - reverb_decay_factor;

                float half_fft_size = fft_size/2; // this equals 1024
                float current = reverb_energy[i];   // NOTE reverb_energy is size 2048
                if (i > 0 && i < half_fft_size - 2) {  // Prevents accessing outside of array index, from i= 2 to 1022
                    
                    // Morph reverb up by octaves up or down
                    if (shimmer_mode == 1 || shimmer_mode == 2) { // up octave
                        reverb_energy[2*i - 1] += 0.123*shimmer_double*current;
                        reverb_energy[2*i] += 0.25*shimmer_double*current;
                        reverb_energy[2*i + 1] += 0.123*shimmer_double*current;
                    } else if ((shimmer_mode == 0 || shimmer_mode == 2) && i > 1 && !(i % 2)) { // down octave, !(i % 2) this checks if i is even for indexing
                        reverb_energy[i/2 - 1] += 0.75*shimmer_double*current;
                        reverb_energy[i/2] += 1.5* shimmer_double*current;
                        reverb_energy[i/2 + 1] += 0.75*shimmer_double*current;
                    }

                    // Morph reverb up by octave+5th
                    if (3*i + 1 < half_fft_size) {

                        reverb_energy[3*i - 2] += 0.055*shimmer_triple*current;
                        reverb_energy[3*i - 1] += 0.11*shimmer_triple*current;
                        reverb_energy[3*i] += 0.17*shimmer_triple*current;
                        reverb_energy[3*i + 1] += 0.11*shimmer_triple*current;
                        reverb_energy[3*i + 2] += 0.105*shimmer_triple*current;

                    }
                    // Detune up or down based on detune knob
                    if (i > 2 && i < half_fft_size - 2 && detune_mode != 1) {
                        reverb_energy[i + (3*detune_multiplier)] += 0.123*detune_double*current;
                        reverb_energy[i + (2*detune_multiplier)] += .25* detune_double*current;
                        reverb_energy[i + (1*detune_multiplier)] += 0.123*detune_double*current;
                    }
                } 
                //reverb_energy[i] = shimmer_remainder * current;
                if (detune_mode ==1)
                    detune_remainder = 1;
                reverb_energy[i] = detune_remainder * shimmer_remainder * current;
            }

            out[i] = real;
            out[i + offset] = imag;

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
            // This is to avoid Max/MSP Note outs for now..
            if(m.data[1] != 0)
            {
                p = m.AsNoteOn();
                // Do stuff with the midi Note/Velocity info here
                //osc.SetFreq(mtof(p.note));
                //osc.SetAmp((p.velocity / 127.0f));
            }
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
                    knobValues[3] = (((float)p.value / 127.0f) * 0.1);
                    break;
                case 18:
                    midi_control[4] = true;
                    knobValues[4] = (((float)p.value / 127.0f) * 0.3);
                    break;
                case 19:
                    midi_control[5] = true;
                    knobValues[5] = ((float)p.value / 127.0f) * 0.3 - 0.15; //-.15f, 0.15f
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

    hw.Init();
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_32KHZ); // Currently needs to run at 32kHz and block size 256 to keep up with processing
    samplerate = hw.AudioSampleRate();

    hw.SetAudioBlockSize(256); // matching original code at 256, TODO test lower latency settings from note:
    // `N = 4096` and `laps = 4` (higher frequency resolution, greater latency), or when `N = 2048` and `laps = 8` (higher time resolution, less latency). 
           
    for (size_t i = 0; i < N / 2; i++)  // loop i from 0 to 2047
    {
        reverb_energy[i] = 0.0;
    }

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

    // Midi
    for( int i = 0; i < 6; ++i ) 
        midi_control[i] = false;  // Is this needed? or does it default to false

    decay.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    mix.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    damp.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::EXPONENTIAL);

    shimmer.Init(hw.knob[Funbox::KNOB_4], 0.0f, 0.1f, Parameter::LINEAR);
    shimmer_tone.Init(hw.knob[Funbox::KNOB_5], 0.0f, 0.3f, Parameter::LINEAR);
    detune.Init(hw.knob[Funbox::KNOB_6], -.15f, 0.15f, Parameter::LINEAR); 
    expression.Init(hw.expression, 0.0f, 1.0f, Parameter::LINEAR); 

    vdecay = 10;
    vmix = 0.5;
    vdamp = 0.1;
    vshimmer = 0.0;
    vshimmer_tone = 0.0;
    vdetune = 0.0;

    // initialize FFT and STFT objects    
    fft = new ShyFFT<float, N, RotationPhasor>();
    fft->Init();
    stft = new Fourier<float, N>(reverb, fft, &hann, laps, in, middle, out);

    samplerateReducer.Init();
    samplerateReducer.SetFreq(0.3);  
    lowpass.Init(samplerate);
    lowpass.SetFreq(8000.0);

    // Drift
    drift_osc.Init(samplerate);
    drift_osc.SetAmp(1.0);

    drift_osc2.Init(samplerate);
    drift_osc2.SetAmp(1.0);

    drift_osc3.Init(samplerate);
    drift_osc3.SetAmp(1.0);

    drift_osc4.Init(samplerate);
    drift_osc4.SetAmp(1.0);

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

        System::DelayUs(100);  //  KAB Note - 1/60 second is 16667

    }

    delete stft;
    delete fft;

}