# Loading Fringe in Renoise (macOS)

Fringe is a **VST3 instrument** (synth), not an effect.

## Install location

```
~/Library/Audio/Plug-Ins/VST3/Fringe.vst3
```

**From release zip:** double-click `Install-VST3.command`, or copy `VST3/Fringe.vst3` into the folder above.

**From source:**

```bash
export PATH="$HOME/.local/cmake/bin:$PATH"
cd ~/Documents/fringe-plugin
./scripts/package_release.sh
# installs VST3 locally and builds dist/Fringe-*-macOS-*.zip
```

## In Renoise

1. **Edit → Preferences → Plug-ins / Misc**
2. Ensure **VST3** scanning is enabled (Renoise 3.4+).
3. Click **Rescan** (or Rescan previously failed).
4. Close Preferences.

### Load as instrument

1. Select an empty instrument slot (left instrument box).
2. Open **Instrument Settings** (or the Plugin tab on the instrument).
3. Choose **Plugin** / **VST**.
4. Find **Fringe** under vendor **Edwin Rosero** (or search “Fringe”).
5. Click to load — the editor window should open with the wave field.

### MIDI

- Draw notes in the Pattern Editor on that track’s instrument column, **or**
- Route an external MIDI keyboard to the instrument.

### Tips

- If Fringe is missing: quit Renoise fully, confirm the `.vst3` path above exists, rescan again.
- First open can be slow (plugin init + wave sim).
- Use **SOURCE** on in the editor for continuous field; play notes for pulses.
- CPU: lower buffer size in Renoise if glitchy; Fringe is heavier than a simple synth.

## Versions

- **Renoise 3.4+** recommended for VST3.
- Older Renoise may only see VST2 (not built) or AU (needs full Xcode to build on this Mac).
