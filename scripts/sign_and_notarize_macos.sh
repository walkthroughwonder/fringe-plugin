#!/usr/bin/env bash
# Sign + notarize macOS Fringe artefacts with Developer ID.
#
# Required env (from Apple Developer account):
#   APPLE_DEVELOPER_ID   e.g. "Developer ID Application: Edwin Rosero (TEAMID)"
#   APPLE_TEAM_ID        10-char team id
#
# Auth (one of):
#   A) App-specific password:
#        APPLE_ID                  apple-id@email.com
#        APPLE_APP_SPECIFIC_PASSWORD
#   B) API key (preferred for CI):
#        APPLE_API_KEY_ID
#        APPLE_API_ISSUER
#        APPLE_API_KEY_PATH        path to AuthKey_XXX.p8
#
# Usage:
#   ./scripts/sign_and_notarize_macos.sh path/to/Fringe.vst3 [path/to/Fringe.app] [path/to/Fringe.component]
#
# Without APPLE_DEVELOPER_ID the script falls back to ad-hoc signing (not notarized).
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <artefact> [artefact...]" >&2
  exit 1
fi

IDENTITY="${APPLE_DEVELOPER_ID:-}"
TEAM="${APPLE_TEAM_ID:-}"

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

  echo "Developer ID sign: $path"
  codesign --force --deep --options runtime --timestamp \
    --sign "$IDENTITY" "$path"
  codesign --verify --verbose=2 "$path"
}

notarize_one() {
  local path="$1"
  if [[ -z "$IDENTITY" ]]; then
    echo "Skip notarize (no Developer ID): $path"
    return 0
  fi

  local zip
  zip="$(mktemp -t fringe-nota).zip"
  ditto -c -k --keepParent "$path" "$zip"

  local args=(notarytool submit "$zip" --wait --team-id "$TEAM")
  if [[ -n "${APPLE_API_KEY_ID:-}" && -n "${APPLE_API_ISSUER:-}" && -n "${APPLE_API_KEY_PATH:-}" ]]; then
    args+=(--key "$APPLE_API_KEY_PATH" --key-id "$APPLE_API_KEY_ID" --issuer "$APPLE_API_ISSUER")
  elif [[ -n "${APPLE_ID:-}" && -n "${APPLE_APP_SPECIFIC_PASSWORD:-}" ]]; then
    args+=(--apple-id "$APPLE_ID" --password "$APPLE_APP_SPECIFIC_PASSWORD")
  else
    echo "ERROR: set APPLE_API_KEY_* or APPLE_ID + APPLE_APP_SPECIFIC_PASSWORD for notarization" >&2
    rm -f "$zip"
    exit 1
  fi

  echo "Notarizing: $path"
  xcrun "${args[@]}"
  rm -f "$zip"

  # Staple when possible (apps and some bundles)
  if xcrun stapler staple "$path" 2>/dev/null; then
    echo "Stapled: $path"
  else
    echo "Note: stapler staple not applicable or failed for $path (zip distribution still OK after notary success)"
  fi
}

for art in "$@"; do
  sign_one "$art"
done

if [[ -n "$IDENTITY" ]]; then
  for art in "$@"; do
    notarize_one "$art"
  done
  echo "Sign + notarize complete."
else
  echo "Ad-hoc only. Export APPLE_DEVELOPER_ID (+ notarize credentials) for production signing."
fi
