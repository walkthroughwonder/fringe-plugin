#!/usr/bin/env bash
# Interactive helper: load Developer ID materials and set GitHub secrets for Fringe CI.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "=== Fringe Apple signing setup ==="
echo ""
echo "Need:"
echo "  1) Developer ID Application certificate (.p12 export)"
echo "  2) App Store Connect API key (.p8) for notarytool"
echo ""

# Check local identities first
echo "Current codesign identities on this Mac:"
security find-identity -v -p codesigning || true
echo ""

read -r -p "Path to Developer ID .p12 (or press Enter to skip import): " P12
if [[ -n "${P12}" && -f "${P12}" ]]; then
  read -r -s -p "Password for that .p12: " P12_PASS
  echo ""
  security import "$P12" -k ~/Library/Keychains/login.keychain-db -P "$P12_PASS" -T /usr/bin/codesign -T /usr/bin/security || true
  echo "Identities after import:"
  security find-identity -v -p codesigning
fi

echo ""
echo "Pick the exact identity string (copy/paste from list above)."
read -r -p "APPLE_DEVELOPER_ID: " DEVELOPER_ID
read -r -p "APPLE_TEAM_ID (10 chars): " TEAM_ID

if [[ -z "${P12:-}" || ! -f "${P12:-}" ]]; then
  read -r -p "Path to .p12 for CI (required for GitHub Actions): " P12
fi
read -r -s -p "Password for .p12: " P12_PASS
echo ""

read -r -p "Path to AuthKey_XXXX.p8: " P8
read -r -p "APPLE_API_KEY_ID: " KEY_ID
read -r -p "APPLE_API_ISSUER (UUID): " ISSUER

if ! command -v gh >/dev/null; then
  echo "ERROR: gh CLI required" >&2
  exit 1
fi

echo ""
echo "Setting GitHub secrets on walkthroughwonder/fringe-plugin ..."
gh secret set APPLE_DEVELOPER_ID -b "$DEVELOPER_ID"
gh secret set APPLE_TEAM_ID -b "$TEAM_ID"
base64 -i "$P12" | gh secret set APPLE_CERTIFICATE_BASE64
printf '%s' "$P12_PASS" | gh secret set APPLE_CERTIFICATE_PASSWORD
gh secret set APPLE_API_KEY_ID -b "$KEY_ID"
gh secret set APPLE_API_ISSUER -b "$ISSUER"
base64 -i "$P8" | gh secret set APPLE_API_KEY_BASE64

echo ""
echo "Secrets set. Local shell exports (this session only):"
export APPLE_DEVELOPER_ID="$DEVELOPER_ID"
export APPLE_TEAM_ID="$TEAM_ID"
export APPLE_API_KEY_ID="$KEY_ID"
export APPLE_API_ISSUER="$ISSUER"
export APPLE_API_KEY_PATH="$P8"
echo "  APPLE_DEVELOPER_ID=$DEVELOPER_ID"
echo "  APPLE_TEAM_ID=$TEAM_ID"
echo "  APPLE_API_KEY_PATH=$P8"
echo ""
echo "Next: tag a release or re-run Build workflow."
echo "  gh workflow run Build.yml"
echo "  # or: git tag v1.1.1 && git push origin v1.1.1"
