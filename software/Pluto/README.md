# Pluto

Pluto is a wacky dual/stereo looper for endless experimentation. Each looper features a variable speed control, smooth or stepped,
and several effects and loop modes. Pluto runs at 96kHz instead of 48kHz, in order to have the best quality when slowing down loops,
but don't worry, lots of aliasing/artifacts are present when strethching audio!

Loops are recorded at the current speed setting. For example, if the speed of Loop A is at 2x and you record a loop, it will sound normal, but turning the speed 
knob down to 1x will play your loop at half speed (and half the samplerate). 

Pluto is in active development, more features coming 


![app](https://github.com/GuitarML/Funbox/blob/main/software/images/pluto_infographic.jpg)

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Looper A Volume |  |
| Ctrl 2 | Looper A Modify | Modifies the current effect applied to loop A (or for Reverb, controls decay time) |
| Ctrl 3 | Looper B Volume |  |
| Ctrl 4 | Looper A Speed/Direction | Speed and direction of Loop A, -2x (reverse) to +2x speed, noon is 1x speed |
| Ctrl 5 | Looper A Modify | Modifies the current effect applied to loop A (or for Reverb, controls decay time) |
| Ctrl 6 | Looper B Speed/Direction | Speed and direction of Loop B, -2x (reverse) to +2x speed, noon is 1x speed |  |
| 3-Way Switch 1 | Looper A/B Speed mode | Left = smooth, Center=Stepped, Right= TBD |
| 3-Way Switch 2 | Live effect select (controls what modify knobs do) | Left=Stability(random variations in playback speed), Center=Filter (LP to the left, HP to the right), Right=Reverb (stereo, ModA is decay, ModB is damping) |
| 3-Way Switch 3 | Loop Record Mode | Left = normal, Center = One Time loop record, Right= Frippertronics (when recording, acts like a delay, applying a decay time to the loop) |
| Dip Switch 1 | MISO/Stereo | Miso only takes the left channel input, and treats each looper as independant. Stereo takes L/R input and treats loopers as connected when recording (Looper B footswitch is inactive in Stereo Mode) |
| Dip Switch 2 | TBD | Want to add a Sync mode here, where the loop time for both A/B are equal |
| FS 1 | Loop A control | Tap once to start recording loop, tap again to stop record and start playback. Double tap to pause loop. Hold to clear loop. Tap again to overdub. Currently no undo function. |
| FS 2 | Loop B control | Same as Loop A |
| LED 1 | Loop A Indicator | Pulses while recording, Solid while playback, blinking while paused. |
| LED 2 | Loop B indicator | Same as Loop A|
| Audio In 1 | Stereo (or MISO) | MISO will use 2 speparate loopers, and copy left channel output to right, Stereo will use 1 looper, and loop A is left channel, loop B is right channel |
| Audio Out 1 | Stereo |  |

<br>
### Expression
Expression Operation: NOTE THAT THIS IS DIFFERENT FROM OTHER PLANET PEDALS**<br>
1. Plug in passive expression pedal into the 1/8" jack on the top left side of pedal. (will need a 1/4" female to 1/8" male TRS adapter)<br>
2. Turn on Dipswitch 3 to enter Set Expression mode, both LEDs should light up, you are now in Set Expression mode.<br>
3. Move the expression pedal into the heel position (up) and move any number of knobs to where you want to heel limit to be (for example you could turn Volume down). The right LED should be brighter up to indicate the heel position is ready to set.*<br>
3. Move the expression pedal into the toe position (down) and move any number of knobs to where you want to toe limit to be (for example you could turn Volume up). The Left LED should be brighter to indicate the toe position is ready to set.*<br>
4. Turn off Dipswitch 3 to exit Set Expression mode and enter Expression Active Mode. <br>
5. Flip Dipswitch 3 on and move expression pedal into both up and down positions without turning any knobs. Flip Dipswitch 3 off to clear expression and give control back to knobs.

<br>
* Currently, the expression input requires the full range of the expression pedal, in order to detect Up/Down positions. You can sometimes trim the expression pedal to not use the full range, so adjust the trim as necessary.<br>
  Also, some expression pedals have a "Standard" or "Alternate/Other" mode. Funbox should work on the "Standard" mode.<br>
<br>
** Since Pluto uses the footswitches for looper funtions, the third dip switch is used to enter/exit Set Expression mode. Flip dipswitch 3 without adjusting any knobs to clear expression settings.

### MIDI Reference

| Control | MIDI CC | Value |
| --- | --- | --- |
| Knob 1 | 14 | 0- 127 |
| Knob 2 | 15 | 0- 127 |
| Knob 3 | 16 | 0- 127 |
| Knob 4 | 17 | 0- 127 |
| Knob 5 | 18 | 0- 127 |
| Knob 6 | 19 | 0- 127 |
| Toggle Left | 21 | 0,1:left ; 2, center; 3 or > right |
| Toggle Center | 22 | 0,1:left ; 2, center; 3 or > right |
| Toggle Right| 23 | 0,1:left ; 2, center; 3 or > right |

## Build
