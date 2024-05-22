# Mars (Planet Series)

Mars is an amp sim / delay pedal. It includes 3 neural models, 3 Impulse Responses (Cab Sim), and a delay effect with dotted 8th and triplett modes.

Neural Network inference for the amp/pedal models uses [RTNeural](https://github.com/jatinchowdhury18/RTNeural)
The Impluse Response code is a modified version of the IR processing in the [NAM plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) (MIT License) 

Note: Due to the processing power required for the neural models and IRs, Mars is a mono effect, which on the Funbox pedal is copied left to right channel for output. 
If a stereo signal is going into the input, Mars will only take the left channel for processing.

![app](https://github.com/GuitarML/Funbox/blob/main/software/images/mars_infographic.jpg)

## Controls

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Gain | Increases the input gain to the neural model |
| Ctrl 2 | Mix | Mix control between the amp and delay |
| Ctrl 3 | Level | Overall volume output |
| Ctrl 4 | Filter | Lowpass filter left of center, highpass right of center |
| Ctrl 5 | Delay Time | Delay time 0 to 2 seconds.  |
| Ctrl 6 | Delay Feedback | Delay Feedback (how long delay takes to fade out) |
| 3-Way Switch 1 | Amp/Model Select | 3 available models, default: left: Matchless SC30, center: Klon pedal, right: Mesa iib |
| 3-Way Switch 2 | IR Select |  3 available IRs, default: left: Proteus (from GuitarML Proteus plugin), center: American, right: Rectifier |
| 3-Way Switch 3 | Delay Mode | left: normal, center: dotted 8th, right: triplett |
| Dip Switch 1 | Amp On/Off | Engage/Disengage Neural model |
| Dip Switch 2 | IR On/Off| Engage/Disengage IR |
| FS 1 | Bypass |  |
| FS 2 |  |  |
| LED 1 | Bypass Indicator |  |
| LED 2 |  | |
| Audio In 1 | Mono In (Left Channel) | Right channel ignored if using TRS  |
| Audio Out 1 | Mono Out  | Left channel copied to Right Channel if using TRS |


## Build

Mars runs in SRAM memory on the Daisy Seed. You must use the Bootloader to load the executable.