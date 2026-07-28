# Known Issues — Fringe 1.1

## Signing
- Without Apple Developer secrets, macOS builds are **ad-hoc signed** (Gatekeeper warnings).
- Notarization requires paid Apple Developer Program + secrets (see `SIGNING_AND_NOTARIZATION.md`).

## Platforms
- **Local Intel Mac without Xcode**: no AU (CLT only); no true universal binary (needs arm64 SDK/toolchain).
- **CI macos-14**: universal + AU + CLAP when workflow runs successfully.
- **Windows/Linux**: shipped via CI; local packaging supported via `package_release.sh`.

## CLAP
- Uses unofficial [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions).
- Host support varies; VST3 remains the primary format.
- Parameter ranges / wrapper type follow the extension’s JUCE mapping.

## AU
- Requires **full Xcode** (not Command Line Tools alone).
- Enable with Xcode installed; CI sets `FRINGE_FORCE_AU=ON`.

## Host / runtime
- Load as **instrument**, not audio effect.
- Editor needs keyboard focus for Space / QWERTY.
- Hosts may cache old plugin state after updates — re-insert Fringe.

## Performance
- FDTD is heavier than simple synths; raise buffer size if needed.
- Universal builds are larger and slightly slower to compile.

## Roadmap
- Default notarized universal release once Apple credentials are configured in CI  
- Additional host validation (Logic, Ableton, Bitwig, Reaper matrix)  
- Optional quality tiers / scene pads  
