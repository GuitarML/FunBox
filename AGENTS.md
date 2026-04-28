# AGENTS.md

## Project Overview

Funbox is a digital stereo guitar effects platform targeting the **Daisy Seed** (STM32-based) microcontroller. Each effect pedal ("planet") is a standalone C++ firmware in `software/<Planet>/`. The hardware has 6 knobs, 3 three-way switches, 4 DIP switches, 2 footswitches, 2 LEDs, and stereo audio I/O.

## Architecture

- **`software/<Planet>/`** — Each pedal is a self-contained firmware with its own `Makefile` and single main `.cpp` file (e.g., `mars.cpp`, `venus.cpp`). Some pedals include local DSP helpers (e.g., `CloudSeed/`, `ImpulseResponse/`, custom FFT headers).
- **`software/Template/template.cpp`** — **Start here** when creating a new effect. Shows the canonical structure: global state → `AudioCallback` → `main()` init sequence.
- **`include/funbox.h`** — Hardware abstraction mapping knobs/switches/LEDs to Daisy Seed pins. Always use `Funbox::KNOB_1`, `Funbox::SWITCH_1_LEFT`, etc. — never raw pin numbers.
- **`include/expressionHandler.h`** — Expression pedal support for v3 boards. Header-only class.
- **`mod/`** — Modified `daisy_petal.h/.cpp` that **must replace** `libDaisy/src/daisy_petal.*` before building libDaisy. This remaps the Electro-Smith Petal board definition to match the Funbox PCB.
- **`libDaisy/`**, **`DaisySP/`** — Electro-Smith libraries (git submodules). Built once with `make -C libDaisy` and `make -C DaisySP`.
- **`RTNeural/`**, **`eigen/`**, **`gcem/`**, **`q/`** — Third-party libs for neural network inference (Mars/Mercury), math, and DSP. Included as submodules.

## Build System

Each pedal uses a Makefile that includes `libDaisy/core/Makefile`. Key variables:
```makefile
TARGET = planet_name
CPP_SOURCES = planet.cpp            # Add local .cpp files as needed
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP
C_INCLUDES += -I../../include       # Always include funbox.h
# For neural net pedals (Mars, Mercury):
C_INCLUDES += -I../../RTNeural -I../../RTNeural/modules/Eigen
APP_TYPE = BOOT_SRAM               # Only for large programs (Mars, Neptune); omit for Flash
```

**First-time build sequence** (requires [Daisy Toolchain](https://github.com/electro-smith/DaisyWiki)):
```sh
git submodule update --init --recursive
cp mod/daisy_petal.cpp libDaisy/src/ && cp mod/daisy_petal.h libDaisy/src/
make -C libDaisy
make -C DaisySP
cd software/<Planet> && make
```

**Flash upload:** `make program-dfu` (USB bootloader) or `make program` (JTAG/SWD). SRAM programs require the Daisy bootloader first.

## Code Patterns

- **Audio callback structure:** All pedals follow the same pattern — `AudioCallback` calls `hw.ProcessAnalogControls()` + `hw.ProcessDigitalControls()`, reads knob params via `param.Process()`, processes audio sample-by-sample, and writes to `out[0][i]`/`out[1][i]`.
- **Bypass pattern:** Footswitch 1 toggles `bypass` bool on `FallingEdge()`. When bypassed, pass input directly to output.
- **Switch reading:** 3-way switches are read as two bools (left/right); both false = center position. See `UpdateSwitches()` in template.
- **DIP switch 1 convention:** Typically toggles Mono vs. MISO (mono-in/stereo-out) mode. Check `pdip[0]` in the audio loop.
- **Preset system:** Footswitch 2 often implements hold-to-save, press-to-toggle preset, persisted across power cycles.
- **Namespaces:** Always `using namespace funbox;` alongside `daisy` and `daisysp`.

## Adding a New Effect

1. Copy `software/Template/` to `software/NewName/`
2. Rename `template.cpp` → `newname.cpp`, update `TARGET` and `CPP_SOURCES` in `Makefile`
3. Fill in `updateSwitch*()` functions, audio processing in the `AudioCallback` loop, and parameter ranges in `main()`
4. Update `README.md` with the controls table (follow the template format)
5. If using extra libs (RTNeural, CloudSeed), add to `C_INCLUDES` and `CPP_SOURCES`

## Key Constraints

- **ARM Cortex-M7 target** — no STL containers with heap allocation in audio path; use fixed-size arrays
- **Flash (128KB) vs SRAM (480KB):** Large programs (neural nets, dual loopers) must use `APP_TYPE = BOOT_SRAM`
- **48kHz sample rate**, block size typically 48 samples (`hw.SetAudioBlockSize(48)`), but prefer block size 2 (`hw.SetAudioBlockSize(2)`) to eliminate a ~1 kHz digital whine on the Funbox PCB. Most effects run fine at block size 1–2.
- **Board versions:** v1 has 2 DIP switches; v2/v3 have 4. v3 adds expression pedal input. Code should handle via `funbox.h` enums

