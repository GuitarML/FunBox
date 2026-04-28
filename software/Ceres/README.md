# Ceres

An envelope filter / waveshaping effect for the Funbox platform. Ceres tracks your playing dynamics and uses the input amplitude to drive resonant filters and wavefolding, producing synthy, auto-wah-style sounds.

## Modes

**Waveshape** (Switch 1 Left) — `tanh` soft-clipping saturation feeds into an envelope-following resonant state-variable filter (lowpass). Playing harder opens the filter up; as your signal decays, the cutoff sweeps back down. The result is a synthy, harmonically rich distortion with a vocal, wah-like quality.

**Wavefold** (Switch 1 Center) — Envelope-controlled wavefolding with adjustable decay. Playing dynamics modulate the fold depth for evolving, bell-like harmonic textures that bloom and decay with your input.

*One additional mode is planned for future development (Switch 1 Right).*

## Controls

| Control        | Function                                                                 |
|----------------|--------------------------------------------------------------------------|
| **Knob 1**     | **FREQ** (Waveshape) — Filter cutoff ceiling (100–8000 Hz, logarithmic). Sets the upper limit of the envelope-driven filter sweep. |
|                | **DECAY** (Wavefold) — Envelope decay/release time (20 ms–1000 ms, logarithmic). Controls how quickly the fold effect fades after a note. |
| **Knob 2**     | **RES** (Waveshape) — Filter resonance (0–0.99). Higher values add a sharper, more vocal peak. |
|                | **FOLD** (Wavefold) — Maximum fold gain (1×–12×, exponential). Sets how deeply the envelope drives the wavefolding. |
| **Knob 3**     | **SENSE** — Envelope sensitivity (1×–50×, exponential). How strongly the input level drives the effect. Shared across both modes. |
| **Knob 4**     | *(Reserved)*                                                             |
| **Knob 5**     | *(Reserved)*                                                             |
| **Knob 6**     | **LEVEL** — Output level (0–1).                                          |
| **Switch 1**   | Mode select — Left: Waveshape, Center: Wavefold, Right: *(reserved)*     |
| **Switch 2**   | *(Reserved)*                                                             |
| **Switch 3**   | *(Reserved)*                                                             |
| **DIP 1**      | Mono (off) / MISO (on) output mode                                      |
| **DIP 2–4**    | *(Reserved)*                                                             |
| **Footswitch 1** | Bypass toggle                                                          |
| **Footswitch 2** | *(Reserved)*                                                           |
| **LED 1**      | On when effect is active                                                 |

## Build

```sh
cd software/Ceres
make
make program-dfu   # Flash via USB bootloader
```

> **Note:** Audio block size is set to 2 (instead of the default 48) to eliminate a ~1 kHz digital whine on the Funbox PCB. Most effects run fine at this block size.

