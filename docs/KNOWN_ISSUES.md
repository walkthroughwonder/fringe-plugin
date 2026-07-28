# Known Issues — Fringe 1.0.0

## Platform
- **Intel x86_64 only** in the official zip. Apple Silicon: install Rosetta or build from source on an M-series Mac.
- **Not notarized.** First launch may need *Open Anyway* or `xattr -cr`.
- **AU** is not in the CLT-only release package (requires full Xcode to compile).

## Hosts
- Prefer loading as an **instrument**, not a track insert effect.
- Some hosts cache old parameter state; remove/re-add Fringe after updating.
- Editor needs **keyboard focus** for Space / QWERTY.

## Sound / CPU
- FDTD is heavier than a simple synth; raise buffer size if you hear glitches.
- Continuous SOURCE + high sensitivity can still get bright; factory defaults are mid/bass-biased.
- Offline bounce quality matches the live session (no auto HQ grid bump).

## Graphics
- Wave field is supersampled from a modest sim grid (performance tradeoff).
- Draw mode walls are 2D speed-map strokes, not full 3D optics.

## Roadmap (post-1.0)
- Universal binary + Developer ID notarization  
- AU in default macOS package  
- Windows VST3  
- CLAP  
- Scene A/B/C/D pads  
- Higher quality tiers  
