# Fringe

**Photonic synthesizer** — wave-optics simulation as a virtual instrument.

Browser original: [edwinrosero.com/fringe](https://edwinrosero.com/fringe/)  
Design plan: see `docs/` and `~/Documents/fringe-plugin-ultraplan.md`

## Formats (target)

- **VST3** / **AU** / **Standalone** (v1)
- **CLAP** (fast-follow)

## License

GPL-3.0 (open source). Uses JUCE under GPL.

## Build (macOS)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

Outputs land under `build/Fringe_artefacts/`.

## Status

**PR-01:** scaffold + sine stub instrument (loads in hosts).  
DSP / FDTD / editor follow the ultraplan PR ladder.
