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
Parameter beta_param, thresh_param;//, param3, param4, param5, param6;


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
const size_t order = 12;
const size_t N = (1 << order);
const float sqrtN = sqrt(N);
const size_t laps = 4;
const size_t buffsize = 2 * laps * N; // size of the FFT, 32768

// buffers for STFT processing
// audio --> in --(fft)--> middle --(process)--> out --(ifft)--> in -->
// each of these is a few circular buffers stacked end-to-end.
float in[buffsize]; // buffers for input and output (from / to user audio callback)
float middle[buffsize]; // buffers for unprocessed frequency domain data
float out[buffsize]; // buffers for processed frequency domain data

ShyFFT<float, N, RotationPhasor>* fft; // fft object
Fourier<float, N>* stft; // stft object

float beta, thresh;


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
 

    beta = beta_param.Process();
    thresh = thresh_param.Process(); 
    //float vparam3 = param3.Process();

    //float vparam4 = param4.Process();
    //float vparam5 = param5.Process();
    //float vparam6 = param6.Process();

    // Handle Knob Changes Here
    // flag to throttle control updates
    //if (controls_processed)
    //    controls_processed = false;


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
	    out[0][i] = stft->read(); // read the next sample from the STFT
	    out[1][i] = out[0][i]; // Mono processing for now
        }
    }
}


// Put any controls processing here, updated at 60Hz framerate
//static void ProcessControls()
//{
//
//}


// shy_fft packs arrays as [real, real, real, ..., imag, imag, imag, ...]
inline void denoise(const float* in, float* out)
{
	// convenient constant for grabbing imaginary parts
	static const size_t offset = N / 2;

	float average = 0;
	for (size_t i = 0; i < N; i++)
	{
		out[i] = 0; 
		average += in[i] * in[i];
	}

	average /= N;

	for (size_t i = 0; i < N / 2; i++)
	{
		if ((in[i] * in[i] + in[i + offset] * in[i + offset]) < thresh * thresh * average)
		{
			// rescale the low-amplitude frequency bins by (1 - beta) ...
			out[i] = (1 - beta) * in[i];
			out[i + offset] = (1 - beta) * in[i + offset];
		}
		else
		{
			// ... and the high-amplitude ones by beta
			out[i] = beta * in[i];
			out[i + offset] = beta * in[i + offset];
		}
	}
}
          

int main(void)
{
    float samplerate;

    hw.Init();
    samplerate = hw.AudioSampleRate();

    hw.SetAudioBlockSize(256); // matching original code at 256, TODO test lower latency settings from note:
    // `N = 4096` and `laps = 4` (higher frequency resolution, greater latency), or when `N = 2048` and `laps = 8` (higher time resolution, less latency). 

           
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


    beta_param.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    thresh_param.Init(hw.knob[Funbox::KNOB_2], 0.0f, 15.0f, Parameter::LINEAR);
    //param3.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR); 
    //param4.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::LINEAR);
    //param5.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    //param6.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR); 


    // initial parameters for denoise process
    // if testing without hardware control, try changing these values
    // beta = mix between high- and low-energy frequency bands
    // thresh = cutoff for designation of a bin as high- or low-energy
    beta = 1; 
    thresh = 15;

    // initialize FFT and STFT objects    
    fft = new ShyFFT<float, N, RotationPhasor>();
    fft->Init();
    stft = new Fourier<float, N>(denoise, fft, &hann, laps, in, middle, out);


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
	//if (!controls_processed)
	//{
	//    ProcessControls();
	//    controls_processed = true;
	//}

	//System::DelayUs(16667); // 1/60 second // Matching original code, KAB Note - I like this idea

        System::Delay(10);

    }

    delete stft;
    delete fft;

}