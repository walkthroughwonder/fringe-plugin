# Install Fringe 1.0 (macOS)

## Requirements
- macOS 11+ recommended  
- **Intel Mac**, or Apple Silicon with **Rosetta** (this release is `x86_64`)  
- A host that loads **VST3 instruments** (Renoise 3.4+, Ableton, Reaper, Bitwig, etc.)

## Quick install (VST3)

1. Download `Fringe-1.0.0-macOS-Intel.zip` from [Releases](https://github.com/walkthroughwonder/fringe-plugin/releases).
2. Unzip.
3. Copy **`Fringe.vst3`** to:
   ```
   ~/Library/Audio/Plug-Ins/VST3/
   ```
   (Create the `VST3` folder if needed.)
4. Rescan plugins in your DAW.
5. Load **Edwin Rosero: Fringe** as an **instrument** (not an effect).

## Standalone

Open **`Fringe.app`** from the zip.  
If macOS blocks it: **System Settings → Privacy & Security → Open Anyway**.

## Renoise

1. **Preferences → Plug-ins** → enable VST3 → Rescan  
2. Instrument slot → Plugin → **Edwin Rosero: Fringe**  
3. Click the editor so it has keyboard focus  

See also `docs/RENOISE.md`.

## First sounds

| Control | Action |
|---------|--------|
| **SOURCE** | Continuous field |
| **Space** | Fire a wavefront |
| **Z–P** | Pentatonic keys |
| **Double Slit** preset | Classic interference |
| Drag **SRC** / **DET** | Move source & detector |

## Gatekeeper / “damaged” app

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Fringe.vst3
xattr -cr /path/to/Fringe.app
```

## Build from source

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
# VST3 installs to ~/Library/Audio/Plug-Ins/VST3/ when COPY_PLUGIN_AFTER_BUILD is on
```

## License

GPL-3.0 — see `LICENSE`. Source: https://github.com/walkthroughwonder/fringe-plugin  
Web original: https://edwinrosero.com/fringe/
