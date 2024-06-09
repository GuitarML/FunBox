# Jupiter (Planet Series)

Jupiter is a Stereo Reverb pedal. The reverb used here is from the [CloudSeed plugin](https://github.com/ValdemarOrn/CloudSeed) (MIT License). It is the same reverb from the Neptune pedal, 
but more EQ controls are exposed to the user, specifically the post reverb low and high shelf filters, both the frequency cutoff and gain.

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
| FS 2 | Freeze | Holds the current Reverb feedback level, and doesn't allow new input to reverb while held, drops the reverb feedback momentarily when let go |
| LED 1 | Bypass Indicator |  |
| LED 2 | Freeze Indicator | |
| Audio In 1 | Stereo In | Right channel ignored if MISO Mode turned on |
| Audio Out 1 | Stereo Out  |  |


## Build

Jupiter runs in SRAM memory on the Daisy Seed. You must use the Bootloader to load the executable.

Before building Jupiter, you must build Cloudseed with ```make```