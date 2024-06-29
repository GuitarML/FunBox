# MidiKeys

Experiment using the DaisySP ModalVoice, StringVoice, and a synth voice. Use a midi keyboard to strike the resonator, and shape the sound with the controls.
Added a Reverb as well.
Monophonic (one note at a time) for modal and string, and 8 polyphony for synth.
An expression pedal can be used to control knob 1. The expression takes control when the pedal is moved.

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Structure or Synth Filter | Can be Expression controlled |
| Ctrl 2 | Brightness |  |
| Ctrl 3 | Level |  |
| Ctrl 4 | Damping |  |
| Ctrl 5 | Reverb Time or Delay Time |  |
| Ctrl 6 | Reverb Damping or Delay Feedback|  |
| 3-Way Switch 1 | Voice Mode | left=Modal, center=String spreadFreq, right=Synth |
| 3-Way Switch 2 | Effect Mode | left=Reverb, center=Delay |
| 3-Way Switch 3 |  |  |
| Dip Switch 1 |  |  |
| Dip Switch 2 |  |  |
| FS 1 | Bypass |  |
| FS 2 |  |  |
| LED 1 | Bypass Indicator |  |
| LED 2 | Lights up for MIDI Notes Received | |
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

