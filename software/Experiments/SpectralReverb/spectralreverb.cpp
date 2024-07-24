#include <string.h>
#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"


#include <cmath>
#include <complex>
#include "shy_fft.h"

#include "fourier.h"
#include "wave.h"

#define PI 3.1415926535897932384626433832795
//#define SR 48000
//typedef float S; // sample type

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
using namespace soundmath;

// Declare a local daisy_petal for hardware access
DaisyPetal hw;
Parameter decay, mix, damp, shimmer, shimmer_tone;//, param6;

float samplerate = 32000; // making global

bool            bypass;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];


Led led1, led2;



// convenient lookup tables
Wave<float> hann([] (float phase) -> float { return 0.5 * (1 - cos(2 * PI * phase)); });
Wave<float> halfhann([] (float phase) -> float { return sin(PI * phase); });

//const size_t bsize = 256;

bool controls_processed = false;


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

float vdecay, vmix, vdamp,vshimmer,vshimmer_tone; // making these global
float octave_up_rate_persecond, octave_up_rate_perinterval, shimmer_double, shimmer_triple, shimmer_remainder;

float window_samples = 32768; // This is the buffsize
float interval_samples = ceil(window_samples/laps);

bool freeze = false;

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

    // (De-)Activate bypass and toggle LED when left footswitch is let go, or enable/disable amp if held for greater than 1 second //
    // Can only disable/enable amp when not in bypass mode
    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {
        bypass = !bypass;
        led1.Set(bypass ? 0.0f : 1.0f);
    }

    if (hw.switches[Funbox::FOOTSWITCH_2].Pressed()) {
        freeze = true;
    } else {
        freeze = false;
    }
    led2.Set(freeze ? 1.0f : 0.0f);
        

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


    //float vparam6 = param6.Process();

    // Handle Knob Changes Here
    // flag to throttle control updates
    if (controls_processed)
        controls_processed = false;



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

 	    stft->write(in[0][i]); // put a new sample in the STFT
	    out[0][i] = stft->read() * vmix + in[0][i] * (1.0 - vmix); // read the next sample from the STFT
	    out[1][i] = out[0][i]; // Mono processing for now
        }
    }
}


// Put any controls processing here, updated at 60Hz framerate
static void ProcessControls()
{
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    UpdateButtons();
    UpdateSwitches();
 

    vdecay = decay.Process() * 99 + 1; // 1 to 100 blocks
    vmix = mix.Process();  // 0 to 1
    vdamp = damp.Process(); // 0 to 1

    vshimmer = shimmer.Process();
    vshimmer_tone = shimmer_tone.Process();
   
    octave_up_rate_persecond = std::pow(8, vshimmer) - 1;
    octave_up_rate_perinterval = std::min(0.75f, octave_up_rate_persecond/samplerate*interval_samples);

    shimmer_double = octave_up_rate_perinterval*(1 - vshimmer_tone/1.58);
    shimmer_triple = (octave_up_rate_perinterval/1.58) * vshimmer_tone;
    shimmer_remainder = (1 - shimmer_double - shimmer_triple);

}


// shy_fft packs arrays as [real, real, real, ..., imag, imag, imag, ...]
inline void reverb(const float* in, float* out)
{
        // Reverb logic based on code by geraintluff in this forum:
        // https://forum.cockos.com/showthread.php?t=225955
        // Here I only use mono though

	// convenient constant for grabbing imaginary parts
	static const size_t offset = N / 2; // equals 2048

	//for (size_t i = 0; i < N; i++)  // loop 0 to 4096
	//{
	//	out[i] = 0; // Is this necessary? It was in the original example code

	//}
        float reverb_amp = 0.0;
	for (size_t i = 0; i < N / 2; i++)  // loop i from 0 to 2047
	{

            // float fft_size = N / 2; // moved to global to be accessed by ProcessControls()
            float fft_bin = i + 1;

            float real = in[i];
            float imag = in[i + offset];

            float energy = real * real + imag * imag;

            // Amplitude from energy
            //reverb_amp = vmix * sqrt(reverb_energy[i]);
            reverb_amp = sqrt(reverb_energy[i]);
            if (fft_bin / fft_size > vdamp) {

                // Reduce amplitude by 1/f
                reverb_amp *= vdamp * fft_size/fft_bin;
            }


            // Add random phase reverb energy
            float random_phase = rand()*2*PI;
            //real += reverb_amp * cos(random_phase);
            //imag += reverb_amp * sin(random_phase);
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
                    
                    // Morph reverb up by octaves 
                    reverb_energy[2*i - 1] += 0.25*shimmer_double*current;
                    reverb_energy[2*i] += 0.5*shimmer_double*current;
                    reverb_energy[2*i + 1] += 0.25*shimmer_double*current;

                    // Morph reverb up by octave+5th
                    if (3*i + 1 < half_fft_size) {

                        reverb_energy[3*i - 2] += 0.11*shimmer_triple*current;
                        reverb_energy[3*i - 1] += 0.22*shimmer_triple*current;
                        reverb_energy[3*i] += 0.34*shimmer_triple*current;
                        reverb_energy[3*i + 1] += 0.22*shimmer_triple*current;
                        reverb_energy[3*i + 2] += 0.21*shimmer_triple*current;

                    }
                }
                reverb_energy[i] = shimmer_remainder*current;
            }


            // TODO Is this right for Mono? The code from forum does some left/right mixing for stereo
            out[i] = real;
            out[i + offset] = imag;

	}
}
          

int main(void)
{

    hw.Init();
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_32KHZ); 
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


    decay.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    mix.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    damp.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::EXPONENTIAL);

    shimmer.Init(hw.knob[Funbox::KNOB_4], 0.0f, 0.1f, Parameter::LINEAR);
    shimmer_tone.Init(hw.knob[Funbox::KNOB_5], 0.0f, 0.3f, Parameter::LINEAR);
    //param6.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR); 

    vdecay = 10;
    vmix = 0.5;
    vdamp = 0.1;
    vshimmer = 0.0;
    vshimmer_tone = 0.0;

    // initialize FFT and STFT objects    
    fft = new ShyFFT<float, N, RotationPhasor>();
    fft->Init();
    stft = new Fourier<float, N>(reverb, fft, &hann, laps, in, middle, out);


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
	if (!controls_processed)
	{
	    ProcessControls();
	    controls_processed = true;
	}

	System::DelayUs(16667); // 1/60 second // Matching original code, KAB Note - I like this idea

    }

    delete stft;
    delete fft;

}