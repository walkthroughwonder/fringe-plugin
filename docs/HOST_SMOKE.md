# Host smoke checklist — Fringe 1.1

Run after each release candidate. Load Fringe as an **instrument** (not an effect).

## Matrix

| Host | Format | Load | MIDI | Space wavefront | Editor focus | Notes |
|------|--------|------|------|-----------------|--------------|-------|
| Renoise | VST3 | ☐ | ☐ | ☐ | ☐ | Prefs → Plug-ins → Rescan |
| Reaper | VST3 | ☐ | ☐ | ☐ | ☐ | FX → Instruments |
| Logic Pro | AU | ☐ | ☐ | ☐ | ☐ | Needs notarized AU for App Store Logic |
| Ableton Live | VST3 | ☐ | ☐ | ☐ | ☐ | Use VST3, not AU first pass |
| Bitwig | VST3 / CLAP | ☐ | ☐ | ☐ | ☐ | |
| Standalone | .app | ☐ | ☐ | ☐ | ☐ | Audio device permissions |

## Pass criteria

1. Plugin appears after rescan under **Edwin Rosero: Fringe**.
2. Default patch produces mid/bass tone with **SOURCE** on.
3. **Space** fires a wavefront (editor must have keyboard focus — click the UI).
4. Drag **SRC** / **DET** moves probes; field redraws.
5. Double Slit preset loads without crash.
6. Close/reopen editor; state restores from host.

## Failures to log

- Host name + version + OS version  
- Format (VST3 / AU / CLAP)  
- Console / crash log snippet  
- Whether ad-hoc or notarized build  

File: GitHub Issues on `walkthroughwonder/fringe-plugin`.
