# Install Fringe 1.1

**Load Fringe as an instrument**, not an audio effect.

Try the sound first (no install):  
**https://edwinrosero.com/fringe/?play=1**

---

## macOS

### From release zip

1. Download the **macOS-universal** zip (preferred) or **macOS-x86_64**.
2. Unzip.
3. Run **`Install-Plugins.command`** (double-click; installs VST3, AU, CLAP),  
   **or** copy manually:

| Format | Copy to |
|--------|---------|
| `VST3/Fringe.vst3` | `~/Library/Audio/Plug-Ins/VST3/` |
| `AU/Fringe.component` | `~/Library/Audio/Plug-Ins/Components/` |
| `CLAP/Fringe.clap` | `~/Library/Audio/Plug-Ins/CLAP/` |
| `Standalone/Fringe.app` | `/Applications` or anywhere |

4. Rescan plugins in your DAW.
5. Load **Edwin Rosero: Fringe** as an **instrument**.

### Gatekeeper (unsigned / ad-hoc builds)

Current public zips may be **ad-hoc signed** until Apple notarization secrets are configured. If macOS blocks the plugin:

1. System Settings → **Privacy & Security** → **Open Anyway**, or  
2. Terminal:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Fringe.vst3
xattr -cr ~/Library/Audio/Plug-Ins/Components/Fringe.component
xattr -cr ~/Library/Audio/Plug-Ins/CLAP/Fringe.clap
xattr -cr /Applications/Fringe.app   # if you use Standalone
```

If the zip contains **`NOTARIZED.txt`**, Gatekeeper should accept the plugins without the above.

### Apple Silicon

- Prefer the **universal** zip  
- Or build from source on your machine  

---

## Windows

1. Unzip `Fringe-*-Windows-x64.zip`.
2. Copy `VST3/Fringe.vst3` to:
   - `C:\Program Files\Common Files\VST3\`  
   or your host’s VST3 path.
3. Copy `CLAP/Fringe.clap` to your CLAP path if supported  
   (e.g. `%COMMONPROGRAMFILES%\CLAP\`).
4. Optionally run `Standalone/Fringe.exe`.
5. Rescan plugins. Load as **instrument**.

---

## Linux

1. Unzip `Fringe-*-Linux-x86_64.zip`.
2. Copy VST3 to `~/.vst3/` (or `/usr/local/lib/vst3/`).
3. Copy CLAP to `~/.clap/`.
4. Rescan host plugins. Load as **instrument**.

---

## Hosts

| Host | Tips |
|------|------|
| **Renoise** | Prefs → Plug-ins → VST3 → Rescan. **Instrument** → Plugin → Fringe. Click editor for Space/QWERTY. |
| **Reaper** | Insert virtual instrument on track. |
| **Logic** | AU component; may prefer notarized builds. |
| **Ableton** | VST3 instrument rack. |
| **Bitwig** | VST3 or CLAP. |

After an update, **remove and re-insert** Fringe if the host cached an old version.

Full checklist for release testing: [docs/HOST_SMOKE.md](docs/HOST_SMOKE.md).

---

## First play

| Input | Action |
|-------|--------|
| **SOURCE** | Continuous field |
| **Space** | Wavefront pulse (click UI first) |
| **Z–P** | Pentatonic keys |
| Drag **SRC/DET** | Move probes |
| **CINE/SCI** | View mode |
| Double Slit | Interference preset |

---

## Build from source

See [README.md](README.md). Package with:

```bash
./scripts/package_release.sh
```

Notarized macOS: [docs/SIGNING_AND_NOTARIZATION.md](docs/SIGNING_AND_NOTARIZATION.md).
