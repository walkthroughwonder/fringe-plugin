# Changelog

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

### Formats (this release)
- **macOS VST3** (x86_64 Intel)
- **macOS Standalone** (x86_64 Intel)
- AU when built with full Xcode (not in CLT-only CI images by default)

### License
- GPL-3.0 (open source; JUCE under GPL)

### Known limitations
- Intel Mac binary in the zip (Apple Silicon: use Rosetta, or build from source / CI universal later)
- Ad-hoc code signature (Gatekeeper may require Open Anyway)
- Not notarized with Apple Developer ID
- No Windows/Linux builds in this release
- CLAP deferred

---

## 0.x development

Internal milestones: scaffold → DSP port → warm defaults → 20:9 UI → interactive field → wavefront spacebar.
