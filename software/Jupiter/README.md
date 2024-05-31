# Jupiter (Planet Series)

Jupiter is a Stereo Reverb pedal. The reverb used here is from the [CloudSeed plugin](https://github.com/ValdemarOrn/CloudSeed) (MIT License). It is the same reverb from the Neptune pedal, 
but more controls are exposed to the user, including access to all 9 of CloudSeed's presets. The maximum line count is limited to 2 per channel for processing on the Daisy Seed.

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Reverb Decay | Increases the decay time of the reverb |
| Ctrl 2 | Mod  | Amount of modulation of the reverb |
| Ctrl 3 | Mix  | Mix control between the Dry input and reverb |
| Ctrl 4 | PreDelay | Predelay, 0 to 1 second |
| Ctrl 5 | Mod Rate | Rate of modulation |
| Ctrl 6 | Filter | Post  EQ of the reverb, type is determined by Switch 3 |
| 3-Way Switch 1 | Reverb Group |  Chooses the set of 3 reverbs available form Switch 2 (Left=Room/Hall-ish Middle=Special  Right=Cloud/Lush |
| 3-Way Switch 2 | Reverb Preset  |  Sets the specific reverb preset from the set chosen by switch 1 (9 possible combinations of these two switches, one for each Cloudseed preset) |
| 3-Way Switch 3 | Filter mode | left: Low Pass, center: Low Shelf cut, right: High Shelf cut | (Frequecy of shelf cut is pre determined by the internal presets, adjust in code as needed)
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

Presets: (Switch1/Switch2 position)<br>
Left/Left  Small room<br>
Left/Mid   Med space<br>
Left/Right Dull Echoes<br><br>

Mid/Left  Factory Chorus<br>
Mid/Mid   Noise in Hallway<br>
Mid/Right 90s<br><br>

Right/Left  RubiKai<br>
Right/Mid   LookingGlass<br>
Right/Right Hyperplane<br>


## Build

Neptune runs in SRAM memory on the Daisy Seed. You must use the Bootloader to load the executable.