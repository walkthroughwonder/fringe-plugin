#!/usr/bin/env bash
# Build Fringe and create a distributable zip for macOS Intel.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VERSION="${FRINGE_VERSION:-$(grep -E '^project\(Fringe VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')}"
ARCH="$(uname -m)"
OUT_DIR="$ROOT/dist"
STAGE="$OUT_DIR/stage-Fringe-${VERSION}"
ZIP_NAME="Fringe-${VERSION}-macOS-${ARCH}.zip"

echo "==> Fringe ${VERSION} package (${ARCH})"

export PATH="${HOME}/.local/cmake/bin:${HOME}/.local/bin:${PATH}"

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

VST3_SRC="$ROOT/build/Fringe_artefacts/Release/VST3/Fringe.vst3"
APP_SRC="$ROOT/build/Fringe_artefacts/Release/Standalone/Fringe.app"

if [[ ! -d "$VST3_SRC" || ! -d "$APP_SRC" ]]; then
  echo "ERROR: build artefacts missing" >&2
  exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/VST3" "$STAGE/Standalone"

# Copy artefacts
ditto "$VST3_SRC" "$STAGE/VST3/Fringe.vst3"
ditto "$APP_SRC" "$STAGE/Standalone/Fringe.app"

# Docs
cp "$ROOT/INSTALL.md" "$STAGE/"
cp "$ROOT/CHANGELOG.md" "$STAGE/"
cp "$ROOT/LICENSE" "$STAGE/"
cp "$ROOT/README.md" "$STAGE/"
cp "$ROOT/docs/KNOWN_ISSUES.md" "$STAGE/"
cp "$ROOT/docs/RENOISE.md" "$STAGE/" 2>/dev/null || true

# Clear quarantine + ad-hoc sign for friendlier Gatekeeper (still not notarized)
xattr -cr "$STAGE" 2>/dev/null || true
codesign --force --deep --sign - "$STAGE/VST3/Fringe.vst3" 2>/dev/null || true
codesign --force --deep --sign - "$STAGE/Standalone/Fringe.app" 2>/dev/null || true

# Install helper
cat > "$STAGE/Install-VST3.command" << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/Fringe.vst3"
ditto "VST3/Fringe.vst3" "$HOME/Library/Audio/Plug-Ins/VST3/Fringe.vst3"
xattr -cr "$HOME/Library/Audio/Plug-Ins/VST3/Fringe.vst3" 2>/dev/null || true
echo "Installed Fringe.vst3 to ~/Library/Audio/Plug-Ins/VST3/"
echo "Rescan plugins in your DAW. Press Enter to close."
read -r _
EOF
chmod +x "$STAGE/Install-VST3.command"

# Zip
mkdir -p "$OUT_DIR"
ZIP_PATH="$OUT_DIR/$ZIP_NAME"
rm -f "$ZIP_PATH"
(
  cd "$OUT_DIR"
  ditto -c -k --sequesterRsrc --keepParent "$(basename "$STAGE")" "$ZIP_NAME"
)

# Also install locally for the developer
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/Fringe.vst3"
ditto "$VST3_SRC" "$HOME/Library/Audio/Plug-Ins/VST3/Fringe.vst3"
xattr -cr "$HOME/Library/Audio/Plug-Ins/VST3/Fringe.vst3" 2>/dev/null || true
codesign --force --deep --sign - "$HOME/Library/Audio/Plug-Ins/VST3/Fringe.vst3" 2>/dev/null || true

echo ""
echo "==> Package ready:"
ls -lh "$ZIP_PATH"
echo ""
echo "Contents:"
find "$STAGE" -maxdepth 2 | sed 's|^|  |'
echo ""
echo "Publish with:"
echo "  gh release create v${VERSION} \"$ZIP_PATH\" --title \"Fringe ${VERSION}\" --notes-file CHANGELOG.md"
