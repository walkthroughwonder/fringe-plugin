# Signing & Notarization (macOS)

Fringe 1.1 CI ships **ad-hoc signed** macOS builds until Apple secrets are configured.  
After secrets are set, the same workflow produces **Developer ID + notarized** universal zips.

You need an **Apple Developer Program** membership ($99/year).

---

## Quick path (once you have the membership)

### 1. Create certificates & keys

1. [developer.apple.com](https://developer.apple.com) → **Certificates** → create  
   **Developer ID Application**.
2. Install the cert in Keychain Access on your Mac.
3. Export it as a **.p12** (set a strong password).  
   Note the exact identity string, e.g.:

   ```text
   Developer ID Application: Edwin Rosero (AB12CD34EF)
   ```

4. [App Store Connect](https://appstoreconnect.apple.com) → Users and Access → **Integrations** →  
   **App Store Connect API** → generate a key with **Developer** access.  
   Download `AuthKey_XXXXXXXXXX.p8` (once only). Save **Key ID** and **Issuer ID**.

### 2. Add GitHub repository secrets

```bash
cd /path/to/fringe-plugin

# Identity (exact string from Keychain)
gh secret set APPLE_DEVELOPER_ID -b "Developer ID Application: Your Name (TEAMID)"
gh secret set APPLE_TEAM_ID -b "TEAMID"

# Exported .p12 for CI codesign
base64 -i DeveloperID.p12 | gh secret set APPLE_CERTIFICATE_BASE64
gh secret set APPLE_CERTIFICATE_PASSWORD   # paste password when prompted

# notarytool API key
gh secret set APPLE_API_KEY_ID -b "XXXXXXXXXX"
gh secret set APPLE_API_ISSUER -b "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
base64 -i AuthKey_XXXXXXXXXX.p8 | gh secret set APPLE_API_KEY_BASE64
```

| Secret | Purpose |
|--------|---------|
| `APPLE_DEVELOPER_ID` | codesign identity string |
| `APPLE_TEAM_ID` | 10-char team id |
| `APPLE_CERTIFICATE_BASE64` | base64 of Developer ID `.p12` |
| `APPLE_CERTIFICATE_PASSWORD` | password for that `.p12` |
| `APPLE_API_KEY_ID` | notarytool key id |
| `APPLE_API_ISSUER` | issuer UUID |
| `APPLE_API_KEY_BASE64` | base64 of the `.p8` file |

### 3. Ship a notarized release

```bash
# Tag (or re-run Build workflow on existing tag)
git tag v1.1.1   # or bump as needed
git push origin v1.1.1
# Watch: Actions → Build → macos-universal should sign + notarize
```

Or local:

```bash
export APPLE_DEVELOPER_ID="Developer ID Application: …"
export APPLE_TEAM_ID="…"
export APPLE_API_KEY_ID="…"
export APPLE_API_ISSUER="…"
export APPLE_API_KEY_PATH="$HOME/private_keys/AuthKey_….p8"

FRINGE_UNIVERSAL=1 FRINGE_FORCE_AU=1 ./scripts/package_release.sh
```

`package_release.sh` signs + notarizes stage bundles, then zips.

---

## Local-only (no CI)

```bash
export APPLE_DEVELOPER_ID="Developer ID Application: Your Name (TEAMID)"
export APPLE_TEAM_ID="TEAMID"
# Option A — API key
export APPLE_API_KEY_ID=…
export APPLE_API_ISSUER=…
export APPLE_API_KEY_PATH=~/private_keys/AuthKey_….p8
# Option B — app-specific password
# export APPLE_ID=you@email.com
# export APPLE_APP_SPECIFIC_PASSWORD=xxxx-xxxx-xxxx-xxxx

./scripts/package_release.sh
# or only re-sign existing stage:
./scripts/sign_and_notarize_macos.sh \
  dist/stage-*/VST3/Fringe.vst3 \
  dist/stage-*/Standalone/Fringe.app \
  dist/stage-*/AU/Fringe.component \
  dist/stage-*/CLAP/Fringe.clap
```

Entitlements: `resources/Fringe.entitlements` (hardened runtime).

---

## Verify a notarized build

```bash
codesign -dv --verbose=4 Fringe.vst3
spctl -a -vv -t install Fringe.vst3   # may vary for plugins
xcrun stapler validate Fringe.app     # standalone app
```

Users should **not** need `xattr -cr` for notarized builds.

---

## Without a paid account

- CI uses **ad-hoc** sign (`codesign -s -`)
- Document *Open Anyway* / `xattr -cr` in INSTALL.md and the website
- See `KNOWN_ISSUES.md`

## Hardened Runtime

Production sign uses `--options runtime` (required for notarization).  
If a host rejects the plugin after notarization, file an issue with host name + macOS version.
