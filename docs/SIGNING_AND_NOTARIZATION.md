# Signing & Notarization (macOS)

Fringe 1.1 ships **unsigned or ad-hoc signed** builds by default (open-source CI without secrets).

To ship Gatekeeper-clean binaries you need an **Apple Developer Program** membership.

## 1. Developer ID Application certificate

In [developer.apple.com](https://developer.apple.com) → Certificates → create  
**Developer ID Application**.

Install it in Keychain. Note the exact identity string, e.g.:

```text
Developer ID Application: Edwin Rosero (AB12CD34EF)
```

## 2. Environment variables

```bash
export APPLE_DEVELOPER_ID="Developer ID Application: Your Name (TEAMID)"
export APPLE_TEAM_ID="TEAMID"

# Option A — API key (best for CI)
export APPLE_API_KEY_ID="XXXXXXXXXX"
export APPLE_API_ISSUER="xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
export APPLE_API_KEY_PATH="$HOME/private_keys/AuthKey_XXXXXXXXXX.p8"

# Option B — app-specific password
export APPLE_ID="you@email.com"
export APPLE_APP_SPECIFIC_PASSWORD="xxxx-xxxx-xxxx-xxxx"
```

## 3. Sign & notarize artefacts

```bash
./scripts/package_release.sh          # builds stage + zip
./scripts/sign_and_notarize_macos.sh \
  dist/stage-*/VST3/Fringe.vst3 \
  dist/stage-*/Standalone/Fringe.app \
  dist/stage-*/AU/Fringe.component \
  dist/stage-*/CLAP/Fringe.clap
# re-zip after notarize if needed
```

Or set credentials **before** `package_release.sh` — it calls the sign script automatically.

## 4. GitHub Actions secrets

For the release workflow, add repository secrets:

| Secret | Purpose |
|--------|---------|
| `APPLE_DEVELOPER_ID` | codesign identity string |
| `APPLE_TEAM_ID` | team id |
| `APPLE_API_KEY_ID` | notarytool API key id |
| `APPLE_API_ISSUER` | issuer UUID |
| `APPLE_API_KEY_BASE64` | base64 of the `.p8` file |

When these are present, CI signs and notarizes macOS artefacts before upload.

## 5. Without a paid account

- Local **ad-hoc** sign (`codesign -s -`)
- Users run `xattr -cr` / *Open Anyway*
- Documented in `KNOWN_ISSUES.md`

## Hardened Runtime

Production sign uses `--options runtime` (required for notarization).  
JUCE Standalone/plugins generally work with hardened runtime; if a host rejects the plugin, file an issue with the host name and macOS version.
