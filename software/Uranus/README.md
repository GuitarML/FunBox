# Uranus

Uranus is a Granular delay and synth. It splits your guitar signal (or anything else) into 
small grains of audio that can be manipulated to create atmospheric soundscapes. It features a monophonic
granular synth mode and a polyphonic FM synthesizer with controllable parameters over Midi.

Uranus uses a modified version of the GranularPlayer class in DaisySP.

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Size | Grain size 1ms to 300ms |
| Ctrl 2 | Mix (Alt Stereo Spread) | Dry/Wet Mix, or alternate mode Stereo spread of each individual grain |
| Ctrl 3 | Pitch (Alt pitch LFO Depth) | Pitch (transposition) of the grains, -1 to +1 octave in semitones |
| Ctrl 4 | Feedback | Feedback of the delayline |
| Ctrl 5 | Width | The width of time that grains can be randomly sampled from (0 to 50ms). Turn down for more a concise sound, turn up to sample a wider range of the audio  |
| Ctrl 6 | Speed (Alt pitch LFO Rate) | Speed of grain playback -2x to +2x |
| 3-Way Switch 1 | Grain Mode | left=Oct up mid=oct down right=oct up and oct down |
| 3-Way Switch 2 | Grain Envelope | left= smooth cosine envelope for each grain  mid= slow attack linear envelope  right=fast attack linear envelope |
| 3-Way Switch 3 | Synth Mode | left=none (only taking audio input)  mid=granular synth mode (puts an ADSR envelope over the current granular effect, triggered by a midi keyboard and transposed from middle C) right=FM Synth (blocks audio in, use midi for polyphonic, 2 operator FM synthesizer) |
| Dip Switch 1 | N/A |  |
| Dip Switch 2 | N/A |  |
| Dip Switch 3 | N/A |  |
| Dip Switch 4 | N/A |  |
| FS 1 | Bypass | Effect bypass or hold for Alternate mode |
| FS 2 | Hold Sample | Holds the current delay buffer (latching), blocks overwriting |
| LED 1 | Bypass/Alt Indicator | Dims LED while footswitch held in alt mode |
| LED 2 | Hold Indicator | On when in Hold mode|
| Audio In 1 | Mono In | Ignores Right channel input  |
| Audio Out 1 | Stereo | Stereo Out |

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


FM Synth Parameters over MIDI

| Control | MIDI CC | Value |
| --- | --- | --- |
| Carrier Level (overall volume) | 20 | 0- 127 |
| Carrier Ratio (0.5 to 16.5 in 0.5 increments) | 21 | 0- 127 |
| Modulator Level | 22 | 0- 127 |
| Modulator Ratio (0.5 to 16.5 in 0.5 increments) | 23 | 0- 127 |
| Modulator Envelope Attack | 24 | 0- 127 |
| Modulator Envelope Decay  | 25 | 0- 127 |
| Modulator Envelope Sustain| 26 | 0- 127 |
| Modulator Envelope Release| 27 | 0- 127 |
| Carrier Envelope Attack  | 28 | 0- 127 |
| Carrier Envelope Decay   | 29 | 0- 127 |
| Carrier Envelope Sustain | 30 | 0- 127 |
| Carrier Envelope Release | 31 | 0- 127 |


Granular Synth Parameters over MIDI (uses the same MIDI CC as FM Carrier Envelope, based on Synth Mode Toggle)

| Control | MIDI CC | Value |
| --- | --- | --- |
| Envelope Attack  | 28 | 0- 127 |
| Envelope Decay   | 29 | 0- 127 |
| Envelope Sustain | 30 | 0- 127 |
| Envelope Release | 31 | 0- 127 |

## Build

Uranus runs in Flash memory on the Daisy Seed.

