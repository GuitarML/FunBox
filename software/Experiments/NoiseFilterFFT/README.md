# NoiseFilterFFT

A test using a Short Time Fourier Transform to filter out noise using a subractive method. Hold the
right footswitch for the pedal to "learn" the current background noise. Don't play the guitar while 
in learning mode. Let go of the right footswitch to stop learning, and perform the noise reduction on
your guitar signal. The learned noise in each FFT bin is subtracted from your signal. Hold the right
footswitch again to clear the previous learned noise and start learning mode again.

Note: The NoiseFilter won't eliminate noise created by the Funbox pedal itself, as it can only learn the noise from the signal going into it.


This code adapts the code found here to run on Funbox hardware:
https://github.com/amcerbu/DaisySTFT

The noise reduction logic was obtained from the noise-reduction STFT example in this blog post:
https://forum.cockos.com/showthread.php?t=225955


## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Reduction Factor | Amount of noise reduction |
| Ctrl 2 | Level | Output level, 0 to 2x |
| Ctrl 3 |  |  |
| Ctrl 4 |  |  |
| Ctrl 5 |  |  |
| Ctrl 6 |  |  |
| 3-Way Switch 1 |  |  |
| 3-Way Switch 2 |  |  |
| 3-Way Switch 3 |  |  |
| Dip Switch 1 |  |  |
| Dip Switch 2 |  |  |
| FS 1 | Bypass/Engage |  |
| FS 2 | Learning mode | While held, will learn the current noise level. Let go to stop learning.  |
| LED 1 | Bypass indicator |  |
| LED 2 | Learning indicator | Illuminated while in learning mode |
| Audio In 1 | Mono In |   |
| Audio Out 1 | Mono out (left copied to right channel) |  |


## Build

Denoise runs in Flash memory on the Daisy Seed.
