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
#define SR 48000
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
Parameter reduction_factor, level; //, thresh_param;//, param3, param4, param5, param6;


bool            bypass;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];


Led led1, led2;



// convenient lookup tables
Wave<float> hann([] (float phase) -> float { return 0.5 * (1 - cos(2 * PI * phase)); });
Wave<float> halfhann([] (float phase) -> float { return sin(PI * phase); });

//const size_t bsize = 256;

//bool controls_processed = false;


// 4 overlapping windows of size 2^12 = 4096
// `N = 4096` and `laps = 4` (higher frequency resolution, greater latency), or when `N = 2048` and `laps = 8` (higher time resolution, less latency). 
//const size_t order = 12;
const size_t order = 11;
const size_t N = (1 << order);
const float sqrtN = sqrt(N);
//const size_t laps = 4;
const size_t laps = 8;
const size_t buffsize = 2 * laps * N; // size of the FFT, 32768

// buffers for STFT processing
// audio --> in --(fft)--> middle --(process)--> out --(ifft)--> in -->
// each of these is a few circular buffers stacked end-to-end.
float in[buffsize]; // buffers for input and output (from / to user audio callback)
float middle[buffsize]; // buffers for unprocessed frequency domain data
float out[buffsize]; // buffers for processed frequency domain data

ShyFFT<float, N, RotationPhasor>* fft; // fft object
Fourier<float, N>* stft; // stft object

float vreduction_factor;
int mode = 0;  // 0=clear, 1=reduction, 2=learning
bool learning = false;

float noise[N/2];

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

    // While footswitch 2 is held, pedal is learning the current noise. After let go, the learned noise reduction is applied.
    if(hw.switches[Funbox::FOOTSWITCH_2].RisingEdge() && !learning) {
        learning = true;
        mode = 0; // set mode to clear, which will be updated to learning mode in the noisefilter() function after noise buffer cleared
    } 

    if(hw.switches[Funbox::FOOTSWITCH_2].FallingEdge()) {
        learning = false;
        mode = 2; // If footswitch not pressed, set to reduction mode
    }

    led2.Set(learning ? 1.0f : 0.0f);

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
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    UpdateButtons();
    UpdateSwitches();
 

    vreduction_factor = reduction_factor.Process() * 2.0; // range of 0 to 2
    float vlevel = level.Process(); 
    //float vparam3 = param3.Process();

    //float vparam4 = param4.Process();
    //float vparam5 = param5.Process();
    //float vparam6 = param6.Process();



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
	    out[0][i] = stft->read() * vlevel; // read the next sample from the STFT, multiply by level knob
	    out[1][i] = out[0][i]; // Mono processing for now
        }
    }
}




// shy_fft packs arrays as [real, real, real, ..., imag, imag, imag, ...]
inline void noisefilter(const float* in, float* out)
{


	// convenient constant for grabbing imaginary parts
	static const size_t offset = N / 2;

	for (size_t i = 0; i < N / 2; i++)
	{

            //float fft_bin = i + 1;

            float real = in[i];
            float imag = in[i + offset];

	    float energy = real * real + imag * imag;

            if (mode == 0) {  // clear mode
                // clear the noise energy
                noise[i] = 0.0;

            } else if (mode == 1) {
                // learning mode, keep track of max level in current frequency bin
                noise[i] = std::max(noise[i], energy);
            }
           
            // guess energy without the noise based on max noise in learning mode and reduction_factor
            float energy_subtracted = std::max(0.0f, energy - vreduction_factor * noise[i]);


            real *= sqrt(energy_subtracted/energy);
            imag *= sqrt(energy_subtracted/energy);

            out[i] = real;
            out[i + offset] = imag;

	}
 
        if (mode == 0) { // Once the noise buffer has been cleared, start learning mode
            mode = 1;
        }

}
          

int main(void)
{
    float samplerate;

    hw.Init();
    samplerate = hw.AudioSampleRate();

    hw.SetAudioBlockSize(128); // matching original code at 256, TODO test lower latency settings from note:
    // `N = 4096` and `laps = 4` (higher frequency resolution, greater latency), or when `N = 2048` and `laps = 8` (higher time resolution, less latency). 


    for (size_t i = 0; i < N / 2; i++)  // loop i from 0 to 2047
    {
        noise[i] = 0.0;
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


    reduction_factor.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    level.Init(hw.knob[Funbox::KNOB_2], 0.0f, 2.0f, Parameter::LINEAR);
    //param3.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR); 
    //param4.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::LINEAR);
    //param5.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    //param6.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR); 



    // initialize FFT and STFT objects    
    fft = new ShyFFT<float, N, RotationPhasor>();
    fft->Init();
    stft = new Fourier<float, N>(noisefilter, fft, &hann, laps, in, middle, out);


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

    delete stft;
    delete fft;

}