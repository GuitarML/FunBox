# Saturn (Planet Series)

Saturn is a Spectral Delay. It works by using a FFT (Fast Fourier Transform) to split up your signal in the frequency domain and apply different delays to each frequency group. 
Saturn diffracts your signal into "particles", creating a kaleidoscope of sound from your guitar.

This code adapts the FFT code here to run on Funbox hardware:
https://github.com/amcerbu/DaisySTFT

![app](https://github.com/GuitarML/Funbox/blob/main/software/images/saturn_infographic.jpg)

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | PreDelay | Adds up to 1 second of delay prior to the spectral delay starting |
| Ctrl 2 | Mix  | Dry/Wet mix |
| Ctrl 3 | Filter  | Lowpass to the left, Highpass to the right |
| Ctrl 4 | Time | 0 to 4 seconds of delay spread in the frequency bins, exact action determined by time toggle |
| Ctrl 5 | Feedback | Amount of feedback, depending on the FDBK toggle  |
| Ctrl 6 | DriftMod | Oscillation rate for drift, 0 to 1Hz |
| 3-Way Switch 1 | Time Mode | Left= Linear increase of delay time as frequency increases (TimeMod controls slope), Center= Sine wave of delay times across frequencies (TimeMod controls frequency) , Right= Random delay time for each line (TimeMod controls spread) |
| 3-Way Switch 2 | Feedback Mode  | Left=Same FDBK across delaylines , Center=linear decrease with frequency increase , Right= random amount of fdbk for each delayline |
| 3-Way Switch 3 | Drift | Left= Predelay not included, modulates delaytime and delayfeedback; Center= No Drift ; Right= Predelay included in drift, causing pitch warping sounds from modulating the predelay line | 
| Dip Switch 1 | Mono (off) / Stereo(on) | Mono mode uses the right channel's delay lines to double the frequency bins used in the spectral delay, (MONO overrides MISO/Stereo switch |
| Dip Switch 2 | MISO (off) /Stereo (on) | MISO (Mono in stereo out), or True Stereo |
| Dip Switch 3 | Stereo Spread Off/On | Places each frequency bin at a random point in the stereo field |
| Dip Switch 4 | On adds feedback of 0.75 to the Predelay |  |
| FS 1 | Bypass/Engage |  |
| FS 2 | Preset | Hold to save preset, press to engage/disengage preset. Saves between power cycles. |
| LED 1 | Bypass Indicator |  |
| LED 2 | Preset Indicator |  |
| Audio In 1 | Stereo In | Right channel ignored if in Mono or MISO mode |
| Audio Out 1 | Stereo Out  | Left channel copied to right if in Mono mode |
<br>
### Expression
1. Plug in passive expression pedal into the 1/8" jack on the top left side of pedal. (will need a 1/4" female to 1/8" male TRS adapter)<br>
2. Hold both footswitches until they both light up (more than 0.5 seconds, but less than 2 seconds), you are now in Set Expression mode.<br>
3. Move the expression pedal into the heel position (up) and move any number of knobs to where you want to heel limit to be (for example you could turn Volume down). The right LED should be brighter up to indicate the heel position is ready to set.*<br>
3. Move the expression pedal into the toe position (down) and move any number of knobs to where you want to toe limit to be (for example you could turn Volume up). The Left LED should be brighter to indicate the toe position is ready to set.*<br>
4. Hold both footswitches to exit Set Expression mode. This will activate expression for the moved knobs. The moved knobs will be inactive until Expression Mode is deactivated.<br>
5. Repeat step 2 to reset Expression knobs.<br>
6. Hold both footswitches for 2 seconds or more to clear any Expression action and give control back to the Funbox knobs.<br>
<br>
* Currently, the expression input requires the full range of the expression pedal, in order to detect Up/Down positions. You can sometimes trim the expression pedal to not use the full range, so adjust the trim as necessary.<br>
  Also, some expression pedals have a "Standard" or "Alternate/Other" mode. Funbox should work on the "Standard" mode.<br>

### MIDI Reference

| Control | MIDI CC | Value |
| --- | --- | --- |
| Knob 1 | 14 | 0- 127 |
| Knob 2 | 15 | 0- 127 |
| Knob 3 | 16 | 0- 127 |
| Knob 4 | 17 | 0- 127 |
| Knob 5 | 18 | 0- 127 |
| Knob 6 | 19 | 0- 127 |

## Build

Saturn compiles for FLASH memory on the Daisy Seed using the standard upload procedure.

## Technical Info:
Saturn uses a FFT with N=1024 and laps=4 (4 overlapping hann windows). This splits the frequencies into N/2=512 frequency bins. 

At 48000kHz samplerate, the bins are split into (samplerate / 2 / #bins) = 46.875 Hz per bin. 

Each bin output is applied to a separate delayline with different time/feedback to create the Spectral Delay. 
I use two DaisySP delaylines for each bin, one for the real component and one for the imaginary component. 

In order to run on the Daisy Seed in Stereo (using two FFTs), the maximum number of bins with delaylines I was able to use was 175. Using
more than 175 caused audio dropouts when running at 48kHz samplerate. You can utilize more bins by running at a lower samplerate, such as 32kHz.

The frequency range of the spectral delay is (#bins * 46.875) = 8203Hz. This is nowhere near the full audible spectrum, but it still sounds pretty good.

When using Mono mode (dipswitch 1 in the off position), I'm able to use 350 frequency bins for a range of 16406Hz, much closer to the full range of the audible frequencies.
See if you can hear the difference!

The custom FFT function "spectraldelay()" and "spectraldelay2()" (for the right channel) is called every N/laps, so 1024/4=256 samples. 
At 48kHz, this means that the spectraldelay function is called 48000/256 = 187.5 times per second. This means that even though we use 
a relatively large number of delaylines, each one only needs to have a size of 187.5 * 4 = 750 for a max of 4 seconds of delay. (compared to 48000 samples per second in the time domain).
