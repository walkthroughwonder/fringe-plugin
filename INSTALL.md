# Install Fringe 1.1

## macOS

### From release zip

1. Download the **macOS-universal** zip (preferred) or **macOS-x86_64**.
2. Unzip.
3. Run **`Install-Plugins.command`** (installs VST3, AU if present, CLAP if present),  
   **or** copy manually:

| Format | Copy to |
|--------|---------|
| `VST3/Fringe.vst3` | `~/Library/Audio/Plug-Ins/VST3/` |
| `AU/Fringe.component` | `~/Library/Audio/Plug-Ins/Components/` |
| `CLAP/Fringe.clap` | `~/Library/Audio/Plug-Ins/CLAP/` |
| `Standalone/Fringe.app` | `/Applications` or anywhere |

4. Rescan plugins in your DAW.
5. Load **Edwin Rosero: Fringe** as an **instrument**.

### Gatekeeper

If macOS blocks the plugin:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Fringe.vst3
xattr -cr ~/Library/Audio/Plug-Ins/Components/Fringe.component
xattr -cr ~/Library/Audio/Plug-Ins/CLAP/Fringe.clap
```

Notarized builds (Developer ID) should open without this.

### Apple Silicon

- Use the **universal** zip, or  
- Use Rosetta with the Intel zip, or  
- Build from source on your machine.

## Windows

1. Unzip `Fringe-*-Windows-x64.zip`.
2. Copy `VST3/Fringe.vst3` to:
   - `C:\Program Files\Common Files\VST3\`  
   or your host’s VST3 path.
3. Copy `CLAP/Fringe.clap` to your CLAP path if the host supports CLAP  
   (e.g. `%COMMONPROGRAMFILES%\CLAP\`).
4. Run `Standalone/Fringe.exe` optionally.
5. Rescan plugins.

## Linux

1. Unzip `Fringe-*-Linux-x86_64.zip`.
2. Copy VST3 bundle to `~/.vst3/` (or `/usr/local/lib/vst3/`).
3. Copy CLAP to `~/.clap/` (or system CLAP path).
4. Ensure jack/ALSA deps for Standalone as needed.
5. Rescan host plugins.

## Renoise

1. Preferences → Plug-ins → enable **VST3** (and CLAP if available) → Rescan  
2. Instrument → Plugin → **Edwin Rosero: Fringe**  
3. Focus the editor for Space / QWERTY  

## First play

| Input | Action |
|-------|--------|
| **SOURCE** | Continuous field |
| **Space** | Wavefront pulse |
| **Z–P** | Pentatonic keys |
| Drag **SRC/DET** | Move probes |
| **CINE/SCI** | View mode |

## Build from source

See [README.md](README.md). Package with:

```bash
./scripts/package_release.sh
```
