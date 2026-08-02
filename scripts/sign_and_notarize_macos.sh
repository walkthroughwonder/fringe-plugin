#!/usr/bin/env bash
# Sign + notarize macOS Fringe artefacts with Developer ID.
#
# Required env (from Apple Developer account):
#   APPLE_DEVELOPER_ID   e.g. "Developer ID Application: Edwin Rosero (TEAMID)"
#   APPLE_TEAM_ID        10-char team id
#
# Auth (one of):
#   A) App-specific password:
#        APPLE_ID
#        APPLE_APP_SPECIFIC_PASSWORD
#   B) API key (preferred for CI):
#        APPLE_API_KEY_ID
#        APPLE_API_ISSUER
#        APPLE_API_KEY_PATH        path to AuthKey_XXX.p8
#
# Optional:
#   FRINGE_ENTITLEMENTS  path to .entitlements (default: resources/Fringe.entitlements)
#   SKIP_NOTARIZE=1      sign only
#
# Usage:
#   ./scripts/sign_and_notarize_macos.sh path/to/Fringe.vst3 [path/to/Fringe.app] ...
#
# Without APPLE_DEVELOPER_ID the script falls back to ad-hoc signing (not notarized).
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <artefact> [artefact...]" >&2
  exit 1
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IDENTITY="${APPLE_DEVELOPER_ID:-}"
TEAM="${APPLE_TEAM_ID:-}"
ENTITLEMENTS="${FRINGE_ENTITLEMENTS:-$ROOT/resources/Fringe.entitlements}"

sign_one() {
  local path="$1"
  if [[ ! -e "$path" ]]; then
    echo "SKIP missing: $path"
    return 0
  fi

  xattr -cr "$path" 2>/dev/null || true

  if [[ -z "$IDENTITY" ]]; then
    echo "Ad-hoc sign (no APPLE_DEVELOPER_ID): $path"
    codesign --force --deep --sign - "$path"
    return 0
  fi

  local ent_args=()
  if [[ -f "$ENTITLEMENTS" ]]; then
    ent_args=(--entitlements "$ENTITLEMENTS")
    echo "Using entitlements: $ENTITLEMENTS"
  fi

  echo "Developer ID sign: $path"
  # Sign nested Mach-O first for .app bundles when possible
  if [[ -d "$path" && "$path" == *.app ]]; then
    # Shell out nested binaries (best-effort; --deep still applied)
    while IFS= read -r -d '' bin; do
      codesign --force --options runtime --timestamp \
        "${ent_args[@]}" --sign "$IDENTITY" "$bin" 2>/dev/null || true
    done < <(find "$path/Contents" -type f -perm +111 -print0 2>/dev/null || true)
  fi

  codesign --force --deep --options runtime --timestamp \
    "${ent_args[@]}" --sign "$IDENTITY" "$path"
  codesign --verify --verbose=2 "$path"
  spctl --assess --type execute -v "$path" 2>/dev/null || \
    echo "  (spctl execute assess skipped/failed — normal for plugin bundles)"
}

notarize_one() {
  local path="$1"
  if [[ -z "$IDENTITY" ]]; then
    echo "Skip notarize (no Developer ID): $path"
    return 0
  fi
  if [[ "${SKIP_NOTARIZE:-0}" == "1" ]]; then
    echo "Skip notarize (SKIP_NOTARIZE=1): $path"
    return 0
  fi

  local zip
  zip="$(mktemp -t fringe-nota).zip"
  ditto -c -k --keepParent "$path" "$zip"

  local args=(notarytool submit "$zip" --wait --team-id "$TEAM")
  if [[ -n "${APPLE_API_KEY_ID:-}" && -n "${APPLE_API_ISSUER:-}" && -n "${APPLE_API_KEY_PATH:-}" ]]; then
    if [[ ! -f "$APPLE_API_KEY_PATH" ]]; then
      echo "ERROR: APPLE_API_KEY_PATH not found: $APPLE_API_KEY_PATH" >&2
      rm -f "$zip"
      exit 1
    fi
    args+=(--key "$APPLE_API_KEY_PATH" --key-id "$APPLE_API_KEY_ID" --issuer "$APPLE_API_ISSUER")
  elif [[ -n "${APPLE_ID:-}" && -n "${APPLE_APP_SPECIFIC_PASSWORD:-}" ]]; then
    args+=(--apple-id "$APPLE_ID" --password "$APPLE_APP_SPECIFIC_PASSWORD")
  else
    echo "ERROR: set APPLE_API_KEY_* or APPLE_ID + APPLE_APP_SPECIFIC_PASSWORD for notarization" >&2
    rm -f "$zip"
    exit 1
  fi

  if [[ -z "$TEAM" ]]; then
    echo "ERROR: APPLE_TEAM_ID is required for notarization" >&2
    rm -f "$zip"
    exit 1
  fi

  echo "Notarizing: $path"
  if ! xcrun "${args[@]}"; then
    echo "ERROR: notarytool failed for $path" >&2
    echo "Tip: xcrun notarytool log <submission-id> --team-id \$APPLE_TEAM_ID ..." >&2
    rm -f "$zip"
    exit 1
  fi
  rm -f "$zip"

  # Staple when possible (apps and some bundles)
  if xcrun stapler staple "$path" 2>/dev/null; then
    echo "Stapled: $path"
  else
    echo "Note: stapler staple not applicable for $path (zip dist is fine after notary success)"
  fi
}

echo "==> Fringe sign/notarize"
if [[ -n "$IDENTITY" ]]; then
  echo "Identity: $IDENTITY"
  echo "Team:     ${TEAM:-<unset>}"
else
  echo "Mode:     ad-hoc (set APPLE_DEVELOPER_ID for production)"
fi

for art in "$@"; do
  sign_one "$art"
done

if [[ -n "$IDENTITY" ]]; then
  for art in "$@"; do
    if [[ -e "$art" ]]; then
      notarize_one "$art"
    fi
  done
  echo "Sign + notarize complete."
else
  echo "Ad-hoc only. Export APPLE_DEVELOPER_ID (+ notarize credentials) for production signing."
fi
