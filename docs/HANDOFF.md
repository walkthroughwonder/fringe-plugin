# Fringe handoff — 2026-08-09

Status of **Fringe** (web instrument + native plugin + product sites) for the next session or collaborator.

---

## One-line summary

**Fringe 1.1.0 is shipped** (multi-platform plugin zips, product sites, web/plugin chooser).  
**macOS notarization is wired but not activated** — this machine has no Developer ID in Keychain and the GitHub repo has no Apple secrets yet. User reports they **already have a Developer ID** (likely portal-only or on another machine).

---

## Live URLs

| URL | What |
|-----|------|
| https://edwinrosero.com/fringe/ | **Chooser gate** — Play in browser vs Get plugin |
| https://edwinrosero.com/fringe/?play=1 | Skip chooser → web instrument |
| https://edwinrosero.com/fringe-plugin/ | Product / download site (Netlify portfolio) |
| https://walkthroughwonder.github.io/fringe-plugin/ | Same product site (GitHub Pages) |
| https://github.com/walkthroughwonder/fringe-plugin | Plugin source + Releases |
| https://github.com/walkthroughwonder/fringe-plugin/releases/tag/v1.1.0 | Download zips |
| https://github.com/walkthroughwonder/edwinrosero-portfolio- | Portfolio hosting edwinrosero.com |

---

## Repos & local paths

| Repo | Local path | Remote |
|------|------------|--------|
| Plugin | `~/Documents/fringe-plugin` | `walkthroughwonder/fringe-plugin` |
| Portfolio | Often `/tmp/edwinrosero-portfolio` (re-clone as needed) | `walkthroughwonder/edwinrosero-portfolio-` |

**Plugin `main` tip (as of handoff):** `e6fb618` — notarization pipeline + install/demo polish.  
**Uncommitted on disk:** `scripts/setup_apple_signing.sh` (interactive secret helper) — commit if still untracked.

**Portfolio tip:** includes `/fringe` chooser (`bd10e79` era) + blue CTAs + “try browser first” site copy. Deploy via Netlify site id `6659f81b-9cfb-492c-8012-15913d2b8b55` (`edwinrosero-portfolio` → edwinrosero.com).

---

## Product state

### Plugin (1.1.0)

- Formats: **VST3, AU (CI/Xcode), CLAP, Standalone**
- Platforms: **macOS universal** (arm64+x86_64), **Windows x64**, **Linux x86_64**
- Musical factory defaults (warm mid/bass), **20:9** editor, Space = wavefront, FDTD field
- Load as **instrument**, not effect (critical for Renoise etc.)
- macOS public zips are **ad-hoc signed** → Gatekeeper / `xattr -cr` (see INSTALL.md)

### Web instrument

- Original at `/fringe/` under portfolio; deferred load via chooser (`import('./main.js')` after pick)
- Session remembers `fringe-mode=web`; `?play=1` / `?plugin=1` deep links

### Sites

- Primary CTAs **blue** (not gold) on product pages
- Hero: Download + **Try in browser first** → `edwinrosero.com/fringe/?play=1`
- Portfolio homepage Fringe card: “web or plugin” → `/fringe/`

---

## What’s done (recent work)

1. Plugin 1.0 → 1.1 multi-platform CI + packaging (`package_release.sh`)
2. Product website under `website/` + Pages workflow + portfolio mirror `/fringe-plugin/`
3. `/fringe` chooser (browser vs plugin); Netlify prod deploy for portfolio
4. Blue Get/Download CTAs
5. Notarization **pipeline** hardened:
   - `scripts/sign_and_notarize_macos.sh` + entitlements `resources/Fringe.entitlements`
   - `scripts/import_apple_cert_ci.sh` — import `.p12` on GHA runners
   - `scripts/setup_apple_signing.sh` — interactive `gh secret set` helper
   - CI `build.yml` macos job wires all secrets; writes `NOTARIZED.txt` when notarized
   - Docs: `SIGNING_AND_NOTARIZATION.md`, `HOST_SMOKE.md`, INSTALL Gatekeeper notes

---

## Blocked / next action (highest priority)

### Notarize macOS (user has Developer ID, not on this Mac)

**Verified empty:**

```text
security find-identity -v -p codesigning  →  0 valid identities
gh secret list (fringe-plugin)           →  no Apple secrets
```

**Required GitHub secrets** (repo `walkthroughwonder/fringe-plugin`):

| Secret | Purpose |
|--------|---------|
| `APPLE_DEVELOPER_ID` | Exact identity string |
| `APPLE_TEAM_ID` | 10-char team id |
| `APPLE_CERTIFICATE_BASE64` | base64 of Developer ID Application **.p12** |
| `APPLE_CERTIFICATE_PASSWORD` | .p12 password |
| `APPLE_API_KEY_ID` | App Store Connect API key id |
| `APPLE_API_ISSUER` | Issuer UUID |
| `APPLE_API_KEY_BASE64` | base64 of **AuthKey_*.p8** |

**Playbook:**

1. Install/export Developer ID Application on a Mac that has the private key → `.p12`
2. Create/download App Store Connect API key → `.p8` + Key ID + Issuer
3. Run:

   ```bash
   cd ~/Documents/fringe-plugin
   ./scripts/setup_apple_signing.sh
   # or follow docs/SIGNING_AND_NOTARIZATION.md
   ```

4. Confirm identity:

   ```bash
   security find-identity -v -p codesigning
   ```

5. Ship notarized build:

   ```bash
   git tag v1.1.1 && git push origin v1.1.1
   # or: gh workflow run Build.yml
   ```

6. Verify zip contains `NOTARIZED.txt`; users should not need `xattr -cr`
7. Update product site download links if tag is not `v1.1.0`

Full detail: **`docs/SIGNING_AND_NOTARIZATION.md`**

---

## After notarization

1. **Host smoke matrix** — `docs/HOST_SMOKE.md` (Renoise, Reaper, Logic, Ableton, Bitwig, Standalone)
2. Optional: short public demo clip on product site
3. Optional: factory preset bank polish / quality tiers (roadmap in `KNOWN_ISSUES.md`)

---

## Ops notes

### Deploy portfolio (edwinrosero.com)

Netlify is **not** always auto-deploying from Git. Manual:

```bash
# Node/netlify may need reinstall under /tmp if session tools expired
export PATH="/tmp/netlify-tools/bin:/tmp/node-v22.17.0-darwin-x64/bin:$PATH"
# or reinstall node + netlify-cli

cd /tmp && gh repo clone walkthroughwonder/edwinrosero-portfolio- edwinrosero-portfolio
cd edwinrosero-portfolio
# sync website from plugin if needed:
# cp ~/Documents/fringe-plugin/website/{index.html,styles.css} fringe-plugin/
netlify login   # if needed
netlify link --id 6659f81b-9cfb-492c-8012-15913d2b8b55
netlify deploy --prod --dir=.
```

Account used previously: **edwinrosero@gmail.com** (team “eds visualizer”).

### Deploy plugin GitHub Pages

Auto on push to `main` when `website/**` changes (`.github/workflows/pages.yml`).

### Tooling on this Mac (as last used)

- `gh` at `~/.local/bin/gh` (logged in)
- No Homebrew/node permanently; node/netlify often installed under `/tmp` for deploys
- No Developer ID identities in login keychain

### Netlify site map (relevant)

- `edwinrosero-portfolio` → https://edwinrosero.com  
  Site ID: `6659f81b-9cfb-492c-8012-15913d2b8b55`

---

## Key files (plugin)

```
CMakeLists.txt
scripts/package_release.sh
scripts/sign_and_notarize_macos.sh
scripts/import_apple_cert_ci.sh
scripts/setup_apple_signing.sh
resources/Fringe.entitlements
.github/workflows/build.yml      # multi-platform + notarize hooks
.github/workflows/pages.yml      # product site
website/index.html
website/styles.css
INSTALL.md
docs/SIGNING_AND_NOTARIZATION.md
docs/HOST_SMOKE.md
docs/KNOWN_ISSUES.md
docs/PRODUCT_DECISIONS.md
docs/RENOISE.md
```

## Key files (portfolio)

```
fringe/index.html          # chooser + deferred main.js
fringe/*.js                # web instrument
fringe-plugin/             # product site mirror
index.html                 # homepage Fringe card
netlify.toml
```

---

## Known pitfalls

- Renoise: **instrument** slot, not FX; editor needs focus for Space/QWERTY
- Hosts cache old plugin state — re-insert after update
- Local Intel Mac without full Xcode: no AU; CI macos-14 builds AU + universal
- CI codesign **requires** `APPLE_CERTIFICATE_BASE64` (.p12), not just identity string
- Portfolio Netlify may 404 new paths until a manual `--prod` deploy
- Do not commit `.p12` / `.p8` / passwords

---

## Suggested first messages for next agent

1. “Check `security find-identity` and `gh secret list` on fringe-plugin; if secrets present, cut `v1.1.1` notarized.”
2. “If user provides .p12 + .p8 paths, run `./scripts/setup_apple_signing.sh` then tag release.”
3. “After notarized zip ships, update website download links and run HOST_SMOKE.md.”

---

## Contact / ownership

- Product / domain: **edwinrosero.com** (Edwin Rosero)
- GitHub org/user: **walkthroughwonder**
- License: **GPL-3.0** (JUCE GPL path)
