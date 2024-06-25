# MidiKeys

Experiment using the DaisySP ModalVoice and StringVoice class. Use a midi keyboard to strike the resonator, and shape the sound with the controls.
Added a Reverb as well.
Monophonic (one note at a time)

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Structure |  |
| Ctrl 2 | Brightness |  |
| Ctrl 3 | Level |  |
| Ctrl 4 | Damping |  |
| Ctrl 5 | Reverb Time |  |
| Ctrl 6 | Reverb Damping |  |
| 3-Way Switch 1 | Voice Mode | left=Modal, center=String spreadFreq, right= |
| 3-Way Switch 2 |  |  |
| 3-Way Switch 3 |  |  |
| Dip Switch 1 |  |  |
| Dip Switch 2 |  |  |
| FS 1 | Bypass |  |
| FS 2 |  |  |
| LED 1 | Bypass Indicator |  |
| LED 2 |  | |
| Audio In 1 | N/A | Ignores Right channel input  |
| Audio Out 1 | Stereo | |

### MIDI Reference

| Control | MIDI CC | Value |
| --- | --- | --- |
| Knob 1 | 14 | 0- 127 |
| Knob 2 | 15 | 0- 127 |
| Knob 3 | 16 | 0- 127 |
| Knob 4 | 17 | 0- 127 |
| Knob 5 | 18 | 0- 127 |
| Knob 6 | 19 | 0- 127 |

Takes Keyboard Midi input to play a sound.

## Build

MidiKeys runs in Flash memory on the Daisy Seed.

