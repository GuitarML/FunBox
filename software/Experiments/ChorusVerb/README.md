# ChorusVerb

Experiment using the DaisySP Chorus and Reverb effects. Features several routing modes, and uses a pitchshifter for different reverb modes.
Getting some interesting sounds, could be successful with some tweaking. The pitchshifted reverb didn't turn out how I hoped.

Note: I had to modify the pitchshifter class in DaisySP to use a smaller delayline, otherwise the SRAM capacity was exceeded from the Reverb and Pitchshifters. 

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Chorus Depth |  |
| Ctrl 2 | Mix |  |
| Ctrl 3 | ReverbTime |  |
| Ctrl 4 | Chorus Freq |  |
| Ctrl 5 | Level |  |
| Ctrl 6 | Reverb Damping |  |
| 3-Way Switch 1 | Chorus Mode | left=norm, center=stereo spreadFreq, right=stereo spreadDelay |
| 3-Way Switch 2 | Routing Mode | left=Chorus->Reverb, center=parallel, right=Reverb->Chorus |
| 3-Way Switch 3 | Reverb Mode | Left: Normal, Center: Pitchshift up octave, Right: Pitchshift down octave |
| Dip Switch 1 | Mono/Miso |  |
| Dip Switch 2 |  |  |
| FS 1 | Bypass |  |
| FS 2 |  |  |
| LED 1 | Bypass Indicator |  |
| LED 2 |  | |
| Audio In 1 | Mono In | Ignores Right channel input  |
| Audio Out 1 | Stereo Out | Mono/MISO |


## Build

ChorusVerb runs in Flash memory on the Daisy Seed.