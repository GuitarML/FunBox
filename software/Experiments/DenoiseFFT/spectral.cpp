#include <string.h>
#include "daisy_seed.h"
#include "daisysp.h"

#include <cmath>
#include <complex>
#include "shy_fft.h"

#include "fourier.h"
#include "wave.h"

#define PI 3.1415926535897932384626433832795
#define SR 48000
typedef float S; // sample type

using namespace daisy;
using namespace daisysp;
using namespace soundmath;

// convenient lookup tables
Wave<S> hann([] (S phase) -> S { return 0.5 * (1 - cos(2 * PI * phase)); });
Wave<S> halfhann([] (S phase) -> S { return sin(PI * phase); });

const size_t bsize = 256;

bool controls_processed = false;
DaisySeed hw;

// 4 overlapping windows of size 2^12 = 4096
const size_t order = 12;
const size_t N = (1 << order);
const S sqrtN = sqrt(N);
const size_t laps = 4;
const size_t buffsize = 2 * laps * N;

// buffers for STFT processing
// audio --> in --(fft)--> middle --(process)--> out --(ifft)--> in -->
// each of these is a few circular buffers stacked end-to-end.
S in[buffsize]; // buffers for input and output (from / to user audio callback)
S middle[buffsize]; // buffers for unprocessed frequency domain data
S out[buffsize]; // buffers for processed frequency domain data

ShyFFT<S, N, RotationPhasor>* fft; // fft object
Fourier<S, N>* stft; // stft object

static void Callback(AudioHandle::InterleavingInputBuffer in,
					 AudioHandle::InterleavingOutputBuffer out,
					 size_t size)
{
	// flag to throttle control updates
	if (controls_processed)
		controls_processed = false;

	for (size_t i = 0; i < size; i += 2)
	{
		stft->write(in[i]); // put a new sample in the STFT
		out[i] = stft->read(); // read the next sample from the STFT
		out[i + 1] = out[i];
	}
}

// initial parameters for denoise process
// if testing without hardware control, try changing these values
// beta = mix between high- and low-energy frequency bands
// thresh = cutoff for designation of a bin as high- or low-energy
S beta = 1, thresh = 15;

// deal with analog & digital controls -- maybe update beta, thresh, ...
static void ProcessControls()
{

}

// shy_fft packs arrays as [real, real, real, ..., imag, imag, imag, ...]
inline void denoise(const S* in, S* out)
{
	// convenient constant for grabbing imaginary parts
	static const size_t offset = N / 2;

	S average = 0;
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
	hw.Init();

	// initialize FFT and STFT objects
	fft = new ShyFFT<S, N, RotationPhasor>();
	fft->Init();
	stft = new Fourier<S, N>(denoise, fft, &hann, laps, in, middle, out);

/*
	// Add control initialiation here, for example:
	AdcChannelConfig configs[num_controls]; // one config per control
	for (size_t i = 0; i < num_controls; i++)
		configs[i].InitSingle(seed.GetPin(control_pins[i]));
	hw.adc.Init(configs, num_controls);
*/

	hw.SetAudioBlockSize(bsize);
	hw.StartAudio(Callback);

	// throttle control updates
	while (true)
	{
		if (!controls_processed)
		{
			ProcessControls();
			controls_processed = true;
		}

		System::DelayUs(16667); // 1/60 second
	}

	delete stft;
	delete fft;
}