# Pluto

Pluto is a wacky dual/stereo looper for endless experimentation. Each looper features a variable speed control, smooth or stepped,
and several effects and loop modes. Pluto runs at 96kHz instead of 48kHz, in order to have the best quality when slowing down loops,
but don't worry, lots of aliasing/artifacts are present when strethching audio!

Loops are recorded at the current speed setting. For example, if the speed of Loop A is at 2x and you record a loop, it will sound normal, but turning the speed 
knob down to 1x will play your loop at half speed (and half the samplerate). 

Pluto is in active development, more features coming 

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


## Build
