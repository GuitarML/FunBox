# Jupiter (Planet Series)

Jupiter is a Stereo Reverb pedal. The reverb used here is from the [CloudSeed plugin](https://github.com/ValdemarOrn/CloudSeed) (MIT License). It is the same reverb from the Neptune pedal, 
but more EQ controls are exposed to the user, specifically the post reverb low and high shelf filters, both the frequency cutoff and gain.


![app](https://github.com/GuitarML/Funbox/blob/main/software/images/jupiter_infographic.jpg)

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Reverb Decay | Increases the decay time of the reverb, about 300ms to 60 seconds, exponential taper (noon is 2 seconds decay) |
| Ctrl 2 | Low Shelf Frequency  | Log Taper 20Hz to 1000Hz, noon is about 200Hz |
| Ctrl 3 | High Shelf Frequency  | Log Taper 400 Hz to 20000Hz, noon is about 4300Hz |
| Ctrl 4 | Mix | Dry to Wet mix |
| Ctrl 5 | Low Shelf Gain | Low shelf cut, from 100% cut to 0.0 gain at max knob  |
| Ctrl 6 | High Shelf Gain | High shelf cut, from 100% cut to 0.0 gain at max knob |
| 3-Way Switch 1 | Lush Level | Amount of "Lushness", low, medium, high |
| 3-Way Switch 2 | Modulation Level  |  Sets the modulation amount and rate, low, medium, high  |
| 3-Way Switch 3 | PreDelay | Sets predelay, 0, 100ms, 200ms | 
| Dip Switch 1 | MISO/Stereo | MISO (Mono in stereo out), or True Stereo |
| Dip Switch 2 |  |  |
| Dip Switch 3 |  |  |
| Dip Switch 4 |  |  |
| FS 1 | Bypass/Engage |  |
| FS 2 | Preset | Hold to save preset, press to engage/disengage preset. Saves between power cycles. |
| LED 1 | Bypass Indicator |  |
| LED 2 | Preset Indicator |  |
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


## Build

Before building Jupiter, you must build Cloudseed with ```make``` in the CloudSeed folder.