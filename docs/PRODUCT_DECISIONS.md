# Fringe Plugin — Product Decisions

Locked 2026-07-27 (owner: Edwin Rosero / implementer defaults after “up to you”).

| ID | Question | Decision |
|----|----------|----------|
| OQ1 | Pricing / JUCE license | **Free portfolio instrument**, source **GPL-3.0**. Use JUCE under its **GPL** terms (no commercial JUCE license required while the plugin remains open source). |
| OQ2 | Brand | Keep name **Fringe**; manufacturer **Edwin Rosero**; codes `EdRo` / `Frng`. |
| OQ3 | Bright worklet sonification | **Deferred** — not in v1; ScriptProcessor path only. |
| OQ4 | QWERTY in editor | **Yes when editor focused** (demo-friendly); MIDI is primary. |
| OQ5 | Program change | **6 optical factory presets** as host programs; user banks later. |
| OQ6 | AU fourcc | Manufacturer `EdRo`, plugin `Frng`. |
| OQ7 | Max grid 768 | **No in v1** — Max ≡ High 512×256. |
| OQ8 | Sub-osc vs filter | **Sub bypasses LPF** (match web). |
| OQ9 | CLAP for 1.0 | **VST3 + Standalone** ship in 1.0 zip; **AU** when Xcode available; CLAP post-1.0. |
| OQ14 | 1.0 platform scope | **macOS Intel zip** for 1.0.0; universal/notarized builds are 1.1 goals. |
| OQ15 | 1.0 distribution | GitHub Releases + GPL source; portfolio free download. |
| OQ10 | Shared math with web | **Separate repo**; later `constants.json` export. |
| OQ11 | CI Silicon releases | **Yes** — GitHub Actions macOS for release; author may build on Intel. |
| OQ12 | Factory content | **Algorithmic only** (no sample packs). |
| OQ13 | Enhanced MIDI default | **Parity (10 ms pulse) default**; Enhanced optional. |

See also: `~/Documents/fringe-plugin-ultraplan.md`
