# Neptune (Planet Series)

Neptune is a Stereo Reverb/Delay pedal. The reverb used here is from the [CloudSeed plugin](https://github.com/ValdemarOrn/CloudSeed), and the Delay includes normal, octave, and reverse modes.
Neptune is very similar to the GuitarML Atlas pedal. 

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Reverb Size | Increases the input gain to the neural model |
| Ctrl 2 | Mix | Mix control between the amp and delay |
| Ctrl 3 | Delay Time | Overall volume output |
| Ctrl 4 | Mod | Lowpass filter left of center, highpass right of center |
| Ctrl 5 | Reverb LowPass | Delay time 0 to 2 seconds.  |
| Ctrl 6 | Delay Feedback | Delay Feedback (how long delay takes to fade out) |
| 3-Way Switch 1 | Reverb Mode |  left: Factory Chorus, center: Medium Space, right: RubiKai (These mostly match the CloudSeed presets) |
| 3-Way Switch 2 | Routing |  left: Some delay fed to reverb, center: Parallel reverb/delay right: delay into reverb in series |
| 3-Way Switch 3 | Delay Mode | left: normal, center: octave, right: reverse |
| Dip Switch 1 | MISO/Stereo | MISO (Mono in stereo out), or True Stereo |
| Dip Switch 2 |  |  |
| FS 1 | Bypass |  |
| FS 2 | Freeze | Holds the current Reverb/Delay feedback level, and doesn't allow new input to reverb/delay while held, drops the reverb feedback momentarily when let go |
| LED 1 | Bypass Indicator |  |
| LED 2 | Freeze Indicator | |
| Audio In 1 | Stereo In | Right channel ignored if MISO Mode turned on |
| Audio Out 1 | Stereo Out  |  |


## Build

Neptune runs in SRAM memory on the Daisy Seed. You must use the Bootloader to load the executable.