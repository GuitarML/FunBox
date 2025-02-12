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
| Ctrl 5 | Width | The width of time that grains can be randomly sampled from (0 to 500ms). Turn down for more a concise sound, turn up to sample a wider range of the audio  |
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
| LED 2 | Recording/Hold Indicator | On when in Hold mode|
| Audio In 1 | Mono In | Ignores Right channel input  |
| Audio Out 1 | Stereo | Stereo Out |

Expression 

MIDI



## Build

Uranus runs in Flash memory on the Daisy Seed.

