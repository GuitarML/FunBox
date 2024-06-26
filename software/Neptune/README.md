# Neptune (Planet Series)

Neptune is a Stereo Reverb/Delay pedal. The reverb used here is from the [CloudSeed plugin](https://github.com/ValdemarOrn/CloudSeed) (MIT License), and the Delay includes normal, octave, and reverse modes.
Neptune is very similar to the GuitarML Atlas pedal. The reverse delay is a modified version of the reverse delay from [Veno-Echo](https://github.com/AdamFulford/Veno-Echo) (MIT License)

![app](https://github.com/GuitarML/Funbox/blob/main/software/images/neptune_infographic.jpg)

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Reverb Decay | Increases the decay time of the reverb |
| Ctrl 2 | Mix (rev/delay mix)  | Mix control between the Dry input and effect, alternate is Reverb/Delay volume ratio |
| Ctrl 3 | Delay Time | Delay time from 0 to 4 seconds |
| Ctrl 4 | Mod (Mod Rate) | Amount of modulation of the reverb, Alternate mode sets is mod rate |
| Ctrl 5 | Reverb LowPass | Delay time 0 to 2 seconds.  |
| Ctrl 6 | Delay Feedback | Delay Feedback (how long delay takes to fade out) |
| 3-Way Switch 1 | Reverb Mode |  left: Factory Chorus, center: Medium Space, right: RubiKai (These mostly match the CloudSeed presets) |
| 3-Way Switch 2 | Routing |  left: Some delay fed to reverb, center: Parallel reverb/delay right: delay into reverb in series |
| 3-Way Switch 3 | Delay Mode | left: normal, center: octave, right: reverse |
| Dip Switch 1 | MISO/Stereo | MISO (Mono in stereo out), or True Stereo |
| Dip Switch 2 |  |  |
| FS 1 | Bypass, hold to put into Alternate mode | Alternate mode indicated by dimmed LED, switches back when let go |
| FS 2 | Preset | Hold to save preset, press to engage/disengage preset. Saves between power cycles. |
| LED 1 | Bypass/Alternate Indicator |  |
| LED 2 | Preset Indicator |  ||
| Audio In 1 | Stereo In | Right channel ignored if MISO Mode turned on |
| Audio Out 1 | Stereo Out  |  |
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

Neptune runs in SRAM memory on the Daisy Seed. You must use the Bootloader to load the executable.
Note: MIDI will not work if you use the Bootloader from the Daisy Web Programmer (it's an old version).

Before building Neptune, you must build Cloudseed with ```make```