# Ceres

An envelope filter / waveshape / wavefold / phaser effect for the Funbox platform. Ceres tracks your playing dynamics and uses the input amplitude to drive resonant filters, wavefolding, and phaser sweeps, producing synthy, auto-wah-style sounds. An expression pedal or MIDI CC19 can override the envelope follower for direct control.

## Modes

**Waveshape** — Triple-cascaded `tanh` soft-clipping saturation feeds into an envelope-following resonant state-variable filter (lowpass). Playing harder opens the filter up; as your signal decays, the cutoff sweeps back down. The result is a synthy, harmonically rich distortion with a vocal, wah-like quality.

**Wavefold** — Envelope-controlled two-pass wavefolding with cube-root envelope shaping for sustained fold during note sustain. The folded signal passes through an envelope-driven lowpass filter for tonal shaping. Playing dynamics modulate the fold depth for evolving, bell-like harmonic textures.

**Phaser** — Envelope-controlled 8-pole phaser (4 biquad allpass stages with staggered frequencies). Playing dynamics sweep the allpass frequencies across the guitar range, producing deep, animated notches. Transparent compression and adjustable feedback add character.

## Controls

| Control        | Function                                                                 |
|----------------|--------------------------------------------------------------------------|
| **Knob 1**     | **LEVEL** — Output level (0–1).                                          |
| **Knob 2**     | **FREQ** — Frequency ceiling. Waveshape/Wavefold: 100–8000 Hz. Phaser: 40–5000 Hz. |
| **Knob 3**     | **RES / FOLD / FDBK** — Waveshape: Resonance (0–0.99). Wavefold: Max fold depth (1×–12×, exponential). Phaser: Feedback (0–0.95). |
| **Knob 4**     | **SENSE** — Envelope sensitivity (1×–20×, logarithmic). Set to minimum to enable expression/MIDI envelope control. |
| **Switch 1**   | Mode select — Left: Waveshape, Center: Wavefold, Right: Phaser           |
| **Footswitch 1** | Short press: Bypass toggle. Hold 3 seconds: Enter/exit program mode (see MIDI Presets below). |
| **LED 1**      | On when effect is active                                                 |
| **LED 2**      | Envelope indicator — brightness tracks playing dynamics (or expression/MIDI value). Blinks at 500 ms in program mode; blinks 3× fast (100 ms) after a preset save. |
| **Expression** | Directly controls envelope (overrides input envelope follower, v3 board). Only active when SENSE knob is at minimum. |
| **MIDI CC19**  | Directly controls envelope (overrides input envelope follower). Only active when SENSE knob is at minimum. |
| **MIDI PC**    | Program Change 0–127 on Channel 1 — recalls or saves presets (see below) |

## MIDI Presets

Ceres supports 128 presets. Each preset captures the current mode, knob positions, and bypass state.

### Saving a Preset

1. **Enter program mode** — Hold Footswitch 1. LED 2 begins blinking.
2. **Dial in your settings** — Adjust knobs, mode switch, and bypass as desired.
3. **Send a MIDI Program Change** on Channel 1. The current settings are saved to that program slot, program mode exits, and LED 2 blinks quickly to confirm.

### Cancelling Without Saving

- Hold Footswitch 1 again while in program mode, **or** power off the device.

### Recalling a Preset

- Send a MIDI Program Change on Channel 1 while **not** in program mode. If that slot has been saved, the preset is loaded immediately — mode, knobs, and bypass state are all restored. Moving any physical knob exits the preset override and returns to live hardware control. Moving the mode switch exits the preset's mode override.

- Uninitialized preset slots (never saved) are ignored.

## Build

```sh
cd software/Ceres
make
make program-dfu   # Flash via USB bootloader
```

> **Note:** Audio block size is set to 2 (instead of the default 48) to eliminate a ~1 kHz digital whine on the Funbox PCB. Most effects run fine at this block size.
