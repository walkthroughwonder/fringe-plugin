# Fringe 1.1

**Photonic synthesizer** — wave optics as a playable instrument.

Port of: [edwinrosero.com/fringe](https://edwinrosero.com/fringe/)

![version](https://img.shields.io/badge/version-1.1.0-c9a84c) ![license](https://img.shields.io/badge/license-GPL--3.0-green)

## Website

**https://edwinrosero.com/fringe-plugin/**  
Local: [`website/index.html`](website/index.html) · mirror: https://walkthroughwonder.github.io/fringe-plugin/

## Download

**[GitHub Releases](https://github.com/walkthroughwonder/fringe-plugin/releases)**

| Asset | Contents |
|-------|----------|
| `Fringe-*-macOS-universal.zip` | VST3 + AU + Standalone + CLAP (CI, arm64+x86_64) |
| `Fringe-*-macOS-x86_64.zip` | Local Intel build (VST3 + Standalone ± CLAP) |
| `Fringe-*-Windows-x64.zip` | VST3 + Standalone + CLAP |
| `Fringe-*-Linux-x86_64.zip` | VST3 + Standalone + CLAP |

See [INSTALL.md](INSTALL.md).

## Features
- Live FDTD wave field · drag SRC/DET · CINE/SCI views  
- Presets: slits, lens, Mach–Zehnder, freehand draw  
- Scale/drone · FDN reverb · 3 LFOs · Space = wavefront  
- MIDI + QWERTY  

## Build

### macOS (recommended flags for 1.1)

```bash
# Universal + CLAP (needs Xcode for AU)
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DFRINGE_BUILD_CLAP=ON

cmake --build build --config Release -j
./scripts/package_release.sh   # FRINGE_UNIVERSAL=1 for universal zip name
```

### Windows / Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFRINGE_BUILD_CLAP=ON
cmake --build build --config Release -j
./scripts/package_release.sh
```

### Notarized macOS (Developer ID)

```bash
export APPLE_DEVELOPER_ID="Developer ID Application: …"
export APPLE_TEAM_ID="…"
# + API key or app-specific password — see docs/SIGNING_AND_NOTARIZATION.md
./scripts/package_release.sh
```

## CI

| Workflow | Platforms |
|----------|-----------|
| `.github/workflows/build.yml` | macOS universal (AU+VST3+CLAP), Windows, Linux |

Tag `v1.1.0` to attach release zips. Add Apple secrets for notarization.

## Docs
- [INSTALL.md](INSTALL.md)  
- [CHANGELOG.md](CHANGELOG.md)  
- [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md)  
- [docs/SIGNING_AND_NOTARIZATION.md](docs/SIGNING_AND_NOTARIZATION.md)  
- [docs/PRODUCT_DECISIONS.md](docs/PRODUCT_DECISIONS.md)  

## License

**GPL-3.0**. JUCE under GPL. Closed-source shipping needs a commercial JUCE license.

**Edwin Rosero** · `EdRo` / `Frng`
