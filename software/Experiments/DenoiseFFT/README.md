# Denoise

A test using a Short Time Fourier Transform to denoise a signal based on low/high bins and a threshold.
This code adapts the code found here to run on Funbox hardware:
https://github.com/amcerbu/DaisySTFT

Hoping to open up the door to lots of interesting things you can do with FFTs, on the Daisy Seed.

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Beta | mix between high and low energy frequency bands |
| Ctrl 2 | Threshold | cutoff for assigning a bin as high or low energy |
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
| FS 2 |  |  |
| LED 1 | Bypass indicator |  |
| LED 2 |  | |
| Audio In 1 | Mono out (left copied to right) |   |
| Audio Out 1 |  |  |


## Build

Denoise runs in Flash memory on the Daisy Seed.

# NOTES FROM ORIGINAL CODE:

# DaisySTFT

## A minimial short-time Fourier transform example for the Daisy Seed

As configured, the Seed listens for audio, feeds it to a `Fourier<T, N>` instance (which handles the buffering, windowing, forward FFT, frequency-domain processing, inverse FFT), and then outputs the processed audio. The example frequency-domain process is defined in the `denoise` method. Try modifying it!

I've found the program works well when `N = 4096` and `laps = 4` (higher frequency resolution, greater latency), or when `N = 2048` and `laps = 8` (higher time resolution, less latency). 

To compile, make sure you modify the Makefile so that the definitions of `DAISYSP_DIR` and `LIBDAISY_DIR` point to your installations of DaisySP and libDaisy. Then, as usual,

    make clean && make && make program-dfu