# FmSynth

Experiment creating a FM Synth for Midi keyboard using Funbox. This synth uses just two operators, a modulator and a carrier. The modulator oscillator
uses phase modulation to change the timbre of the carrier oscillator. An ADSR envelope is applied to both the modulator and carrier. 
The controls have been simplified to fit within the 6 knobs on Funbox.

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Level | The carrier level, or overall output of the synth |
| Ctrl 2 | Mod level | The modulator level, which changes the timbre of the sound |
| Ctrl 3 | Mod Ratio | Frequency multiplier of the modulator, from 0.5 to 16 by increments of 0.5, multiplied by the base frequncy of the note |
| Ctrl 4 | Attack | Controls the Attack/Decay time of both the modulator and carrier |
| Ctrl 5 | Sustain | Controls the sustain level (volume while note is held) |
| Ctrl 6 | Release | Controls the release time of the note |
| 3-Way Switch 1 |  | |
| 3-Way Switch 2 |  |  |
| 3-Way Switch 3 |  |  |
| Dip Switch 1 |  |  |
| Dip Switch 2 |  |  |
| FS 1 | Bypass |  |
| FS 2 |  |  |
| LED 1 | Bypass Indicator |  |
| LED 2 | Lights up for MIDI Notes Received | |
| Audio In 1 | N/A | Ignores Right channel input  |
| Audio Out 1 | Stereo | |

Expression controls Knob 2 only, Mod Level. Plug in expression pedal to use, move knob 2 to deactivate expression control.

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

