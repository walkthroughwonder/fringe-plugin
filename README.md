# Fringe

**Photonic synthesizer** — wave-optics simulation as a virtual instrument (port of the web Fringe).

Live web original: [edwinrosero.com/fringe](https://edwinrosero.com/fringe/)

## What’s inside (v0.2)

- **CPU FDTD wave field** (same physics model as the web `WaveEngine`)
- **Optical presets:** Single Slit, Double Slit, Convex Lens, Diffraction, Open Field
- **3 detector columns** sonified as stereo voices (web ScriptProcessor path)
- **FX:** compressor, resonant peaks, scale/drone modes, FDN reverb, filter
- **Editor:** live wave field + knobs (volume, speed, freq, slit, sens, filter, reverb)
- **MIDI:** note pulses (Parity) or hold (Enhanced); CC1 → freq, CC74 → filter
- **QWERTY** pentatonic when the editor is focused

## Formats

- **VST3** → `~/Library/Audio/Plug-Ins/VST3/Fringe.vst3`
- **Standalone** app under `build/Fringe_artefacts/Release/Standalone/`

## Build (macOS)

```bash
export PATH="$HOME/.local/cmake/bin:$PATH"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

## Play

1. Open Standalone or load **Fringe** in a DAW  
2. Leave **SOURCE** on to hear continuous field  
3. Play MIDI notes or keyboard (Z/A/Q rows)  
4. Switch presets and twist **Slit** / **Freq** / **Reverb**

## License

GPL-3.0 (JUCE under GPL). See `docs/PRODUCT_DECISIONS.md`.
