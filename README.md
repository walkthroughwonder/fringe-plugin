# Fringe 1.0

**Photonic synthesizer** — wave optics as a playable instrument.

Port of the browser instrument: [edwinrosero.com/fringe](https://edwinrosero.com/fringe/)

![Fringe](https://img.shields.io/badge/version-1.0.0-c9a84c) ![platform](https://img.shields.io/badge/macOS-Intel%20VST3-blue) ![license](https://img.shields.io/badge/license-GPL--3.0-green)

## Download

**[Releases → Fringe 1.0.0](https://github.com/walkthroughwonder/fringe-plugin/releases)**  
See [INSTALL.md](INSTALL.md) for DAW setup (Renoise, etc.).

## What it is

A **VST3 / Standalone** instrument that runs a real-time **FDTD wave field**, sonifies three detector columns, and treats optics (slits, lenses, draw-your-own walls) as the synthesis engine.

### Highlights
- Live supersampled wave display (CINE / SCI views)
- Drag **source** and **detector** on the field
- Presets: Single/Double Slit, Lens, Diffraction, Mach–Zehnder, Draw, Open Field
- Scale / drone modes, FDN reverb, 3 LFOs
- **Space** = wavefront pulse · QWERTY + MIDI

## Formats (1.0.0)

| Format | Status |
|--------|--------|
| macOS VST3 (Intel) | ✅ Release zip |
| macOS Standalone (Intel) | ✅ Release zip |
| macOS AU | Build with full Xcode |
| Apple Silicon native | Build from source / upcoming CI |
| Windows / Linux / CLAP | Post-1.0 |

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

Package a release zip:

```bash
./scripts/package_release.sh
# → dist/Fringe-1.0.0-macOS-Intel.zip
```

## Docs

- [INSTALL.md](INSTALL.md) — install & first play  
- [CHANGELOG.md](CHANGELOG.md) — 1.0 notes  
- [docs/PRODUCT_DECISIONS.md](docs/PRODUCT_DECISIONS.md) — design locks  
- [docs/RENOISE.md](docs/RENOISE.md) — Renoise tips  
- [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) — limitations  

## License

**GPL-3.0** (open source). Uses the JUCE framework under GPL.  
Commercial closed-source distribution requires a commercial JUCE license.

## Credits

**Edwin Rosero** — concept, web Fringe, plugin  
Manufacturer code `EdRo` · plugin code `Frng`
