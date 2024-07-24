# Spectral Reverb

Spectral Reverb using an stft
This code adapts the code found here to run on Funbox hardware:
https://github.com/amcerbu/DaisySTFT

This reverb is based on the example reverb found on this forum:
https://forum.cockos.com/showthread.php?t=225955


## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Decay | Decay time of reverb |
| Ctrl 2 | Mix | Dry/Wet mix |
| Ctrl 3 | Damp | Reverb dampening |
| Ctrl 4 | Shimmer Amount | Adds octave shimmer |
| Ctrl 5 | Shimmer Tone | Adds in octave + 5ths to shimmer |
| Ctrl 6 |  |  |
| 3-Way Switch 1 | Shimmer Mode | left=octave up, center= octave down, right=octave up and octave down |
| 3-Way Switch 2 |  |  |
| 3-Way Switch 3 |  |  |
| Dip Switch 1 |  |  |
| Dip Switch 2 |  |  |
| FS 1 | Bypass/Engage |  |
| FS 2 | Freeze |  |
| LED 1 | Bypass indicator |  |
| LED 2 | Freeze indicator | |
| Audio In 1 | Mono in |   |
| Audio Out 1 | Mono out (left copied to right) |  |


## Build

Spectral Reverb runs in Flash memory on the Daisy Seed.

