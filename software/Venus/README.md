# Venus

Venus is a Spectral Reverb that uses a STFT (Short time Fourier Transform) to operate on your signal in the frequency
domain. It can create lush soundscapes with string-like overtones, detuning, and spectral freeze.  It features
two LoFi modes that reduce the samplerate and two "drift" modes that modulate knob values slowly over time. 

Notes: The Venus pedal runs at 32kHz samplerate and buffersize of 256 in order to run smoothly with the current STFT implementation.
Improving the efficiency of the STFT to run at higher samplerates and lower buffersize is of interest.

![app](https://github.com/GuitarML/Funbox/blob/main/software/images/venus_infographic.jpg)

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Decay | Decay time of reverb |
| Ctrl 2 | Mix | Dry/Wet mix |
| Ctrl 3 | Damp | Reverb dampening |
| Ctrl 4 | Shimmer Amount | Adds octave shimmer |
| Ctrl 5 | Shimmer Tone | Adds in octave + 5ths to shimmer |
| Ctrl 6 | Detune | Left of noon detunes down, right of noon detunes up |
| 3-Way Switch 1 | Shimmer Mode | left=octave down, center= octave up, right=octave up and octave down |
| 3-Way Switch 2 | LoFi Mode (lofi applied to wet mix only, not dry mix) | left= samplerate of 9600 with lowpass filter, center=None, right= samplerate of 6400, no lowpass filter |
| 3-Way Switch 3 | Drift Mode | left=same as right except at a rate of ~50 seconds, center=none, right=more, oscillates the Damp, Shimmer, ShimmerTone, and Detune controls from the current setting to 0 at a rate of 1 cycle per ~25 seconds |
| Dip Switch 1 | N/A |  |
| Dip Switch 2 | N/A |  |
| Dip Switch 3 | N/A |  |
| Dip Switch 4 | N/A |  |
| FS 1 | Bypass/Engage |  |
| FS 2 | Freeze |  |
| LED 1 | Bypass indicator |  |
| LED 2 | Freeze indicator | |
| Audio In 1 | Mono in |   |
| Audio Out 1 | Mono out (left copied to right) |  |

### Expression
1. Plug in passive expression pedal into the 1/8" jack on the top left side of pedal. (will need a 1/4" female to 1/8" male TRS adapter)<br>
2. Hold left footswitche until they both LEDS blink (more than 0.6 seconds, but less than 2 seconds), you are now in Set Expression mode.<br>
3. Move the expression pedal into the heel position (up) and move any number of knobs to where you want to heel limit to be (for example you could turn Volume down). The right LED should be brighter up to indicate the heel position is ready to set.*<br>
3. Move the expression pedal into the toe position (down) and move any number of knobs to where you want to toe limit to be (for example you could turn Volume up). The Left LED should be brighter to indicate the toe position is ready to set.*<br>
4. Hold left footswitch to exit Set Expression mode. This will activate expression for the moved knobs. The moved knobs will be inactive until Expression Mode is deactivated.<br>
5. Repeat step 2 to reset Expression knobs.<br>
6. Hold left footswitch for 2 seconds or more to clear any Expression action and give control back to the Funbox knobs.<br>
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

Venus runs in Flash memory on the Daisy Seed.

##Acknowledgements:
This code adapts the code found here to run a STFT on the Daisy Seed:
https://github.com/amcerbu/DaisySTFT

Which uses the "shyfft" authored by Émilie Gillet of Mutable Instruments:
https://github.com/pichenettes/eurorack

The math used in this reverb is heavily inspired by the Atlantis Reverb by Geraint Luff:
https://geraintluff.github.io/jsfx/#Atlantis%20Reverb

And directly from the examples in this forum post by Geraint Luff:
https://forum.cockos.com/showthread.php?t=225955

The Venus pedal would not be possible without the generosity of smart people sharing their work in the open source community. Thank you!

