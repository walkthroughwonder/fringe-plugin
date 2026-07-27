# Fringe

**Photonic synthesizer** — wave-optics as a VST3/Standalone instrument (port of [web Fringe](https://edwinrosero.com/fringe/)).

## Features (v0.3)

- Live **FDTD wave field** with cathedral colour grading + soft bloom
- Presets: **Single/Double Slit, Lens, Diffraction, Mach–Zehnder, Draw, Open Field**
- **Draw mode** — paint walls on the field (Width = brush; ERASE; CLEAR; double-click clear)
- 3-detector stereo sonification + FDN reverb + scale/drone
- **LFO 1–3** rate/depth (targets: freq/speed/slit/sens/filter/reverb via APVTS)
- Detector graph sidebar
- MIDI (pulse/hold) · QWERTY pentatonic · Space = source gate

## Run

```bash
open build/Fringe_artefacts/Release/Standalone/Fringe.app
# VST3 also installed to ~/Library/Audio/Plug-Ins/VST3/Fringe.vst3
```

## Build

```bash
export PATH="$HOME/.local/cmake/bin:$PATH"
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

GPL-3.0 · see `docs/PRODUCT_DECISIONS.md`
