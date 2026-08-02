#!/usr/bin/env bash
# Import Developer ID .p12 into a temporary keychain (GitHub Actions / CI).
#
# Env:
#   APPLE_CERTIFICATE_BASE64   base64 of Developer ID Application .p12
#   APPLE_CERTIFICATE_PASSWORD password used when exporting the .p12
#
# Optional:
#   APPLE_KEYCHAIN_PATH        default: $RUNNER_TEMP/fringe-signing.keychain-db
#   APPLE_KEYCHAIN_PASSWORD    default: random
#
# After this, codesign can find the identity named in APPLE_DEVELOPER_ID.
set -euo pipefail

if [[ -z "${APPLE_CERTIFICATE_BASE64:-}" ]]; then
  echo "No APPLE_CERTIFICATE_BASE64 — skip cert import"
  exit 0
fi

if [[ -z "${APPLE_CERTIFICATE_PASSWORD:-}" ]]; then
  echo "ERROR: APPLE_CERTIFICATE_PASSWORD required when APPLE_CERTIFICATE_BASE64 is set" >&2
  exit 1
fi

CERT_PATH="${RUNNER_TEMP:-/tmp}/fringe-certificate.p12"
KEYCHAIN_PATH="${APPLE_KEYCHAIN_PATH:-${RUNNER_TEMP:-/tmp}/fringe-signing.keychain-db}"
KEYCHAIN_PASSWORD="${APPLE_KEYCHAIN_PASSWORD:-$(openssl rand -base64 24)}"

echo "$APPLE_CERTIFICATE_BASE64" | base64 --decode > "$CERT_PATH"
echo "Decoded certificate → $CERT_PATH ($(wc -c < "$CERT_PATH") bytes)"

# Create / unlock keychain
security delete-keychain "$KEYCHAIN_PATH" 2>/dev/null || true
security create-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN_PATH"
security set-keychain-settings -lut 21600 "$KEYCHAIN_PATH"
security unlock-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN_PATH"

security import "$CERT_PATH" \
  -P "$APPLE_CERTIFICATE_PASSWORD" \
  -A -t cert -f pkcs12 \
  -k "$KEYCHAIN_PATH"

# Allow codesign to use the private key without GUI prompt
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$KEYCHAIN_PASSWORD" "$KEYCHAIN_PATH"

# Prefer this keychain
security list-keychains -d user -s "$KEYCHAIN_PATH" $(security list-keychains -d user | sed -e s/\"//g)

echo "Identities available:"
security find-identity -v -p codesigning

# Export for package scripts that unlock again if needed
export APPLE_KEYCHAIN_PATH
export APPLE_KEYCHAIN_PASSWORD
echo "APPLE_KEYCHAIN_PATH=$KEYCHAIN_PATH" >> "${GITHUB_ENV:-/dev/null}" 2>/dev/null || true

rm -f "$CERT_PATH"
echo "Certificate imported."
