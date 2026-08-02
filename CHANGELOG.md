# Changelog

## Unreleased

### Packaging
- CI imports Developer ID **.p12** via `APPLE_CERTIFICATE_BASE64` + password (required for real codesign on runners)
- `resources/Fringe.entitlements` for hardened runtime notarization
- `NOTARIZED.txt` marker inside signed packages
- Host smoke checklist: `docs/HOST_SMOKE.md`
- Product site: “Try in browser first”, clearer Gatekeeper / instrument install notes

## 1.1.0 — multi-platform

### Packaging & platforms
- **macOS universal** (`arm64` + `x86_64`) via CI / `FRINGE_UNIVERSAL=1`
- **AU** included when built with full Xcode (`FRINGE_FORCE_AU` on CI)
- **CLAP** via [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions)
- **Windows** VST3 + Standalone + CLAP (GitHub Actions)
- **Linux** VST3 + Standalone + CLAP (GitHub Actions)
- Developer ID **sign + notarize** scripts (secrets-driven; optional)

See `docs/SIGNING_AND_NOTARIZATION.md` and `scripts/package_release.sh`.

---

## 1.0.0 — 2026-07-28

First public release of **Fringe** as a native audio plugin.

### Instrument
- CPU FDTD wave-optics synthesizer (port of the web Fringe)
- Optical presets: Single Slit, Double Slit, Convex Lens, Diffraction, Mach–Zehnder, Draw, Open Field
- 3-detector stereo sonification + compressor, resonant peaks, scale/drone modes, FDN reverb, filter, sub
- Mid/bass musical factory defaults (warm filter, pentatonic scale on)
- MIDI (pulse / hold modes), CC1 → freq, CC74 → filter
- QWERTY pentatonic when editor focused
- **Spacebar** fires a wavefront pulse
- 3 routable LFOs (rate/depth; targets via host automation)

### Editor
- Cinematic **20:9** UI
- Live supersampled wave field (CINE / SCI views)
- Drag **SRC** and **DET** probes on the field
- Draw mode (paint/erase walls)
- Detector scope + energy meter
- LFO activity glow on knobs

### Formats (1.0.0 zip)
- macOS VST3 + Standalone (x86_64 Intel)
- Ad-hoc signature

### License
- GPL-3.0 (open source; JUCE under GPL)
