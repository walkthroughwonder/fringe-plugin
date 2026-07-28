#!/usr/bin/env bash
# Build Fringe and create a distributable zip for the current platform.
#
# Env:
#   FRINGE_VERSION          override version
#   FRINGE_BUILD_DIR        default: build
#   FRINGE_UNIVERSAL=1      macOS: -DCMAKE_OSX_ARCHITECTURES=arm64;x86_64
#   FRINGE_FORCE_AU=1       require AU (macOS + Xcode)
#   FRINGE_BUILD_CLAP=0     disable CLAP
#   APPLE_DEVELOPER_ID=...  enable Developer ID + optional notarize
#   SKIP_BUILD=1            package existing build only
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VERSION="${FRINGE_VERSION:-$(grep -E '^project\(Fringe VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')}"
BUILD_DIR="${FRINGE_BUILD_DIR:-build}"
OUT_DIR="$ROOT/dist"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

export PATH="${HOME}/.local/cmake/bin:${HOME}/.local/bin:/usr/local/bin:${PATH}"

OS="$(uname -s)"
ARCH="$(uname -m)"
case "$OS" in
  Darwin) PLATFORM="macOS" ;;
  Linux)  PLATFORM="Linux" ;;
  MINGW*|MSYS*|CYGWIN*) PLATFORM="Windows" ;;
  *) PLATFORM="$OS" ;;
esac

# Universal label
ARCH_LABEL="$ARCH"
if [[ "${FRINGE_UNIVERSAL:-0}" == "1" ]] || [[ "${CMAKE_OSX_ARCHITECTURES:-}" == *"arm64"*"x86_64"* ]] || [[ "${CMAKE_OSX_ARCHITECTURES:-}" == *"x86_64"*"arm64"* ]]; then
  ARCH_LABEL="universal"
fi

ZIP_NAME="Fringe-${VERSION}-${PLATFORM}-${ARCH_LABEL}.zip"
STAGE="$OUT_DIR/stage-Fringe-${VERSION}-${PLATFORM}-${ARCH_LABEL}"

echo "==> Fringe ${VERSION} package (${PLATFORM}/${ARCH_LABEL})"

CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Release -DFRINGE_COPY_PLUGIN_AFTER_BUILD=ON)
if [[ "${FRINGE_UNIVERSAL:-0}" == "1" && "$PLATFORM" == "macOS" ]]; then
  CMAKE_ARGS+=(-DCMAKE_OSX_ARCHITECTURES=arm64\;x86_64)
fi
if [[ "${FRINGE_FORCE_AU:-0}" == "1" ]]; then
  CMAKE_ARGS+=(-DFRINGE_FORCE_AU=ON)
fi
if [[ "${FRINGE_BUILD_CLAP:-1}" == "0" ]]; then
  CMAKE_ARGS+=(-DFRINGE_BUILD_CLAP=OFF)
fi

if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
  cmake -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"
  cmake --build "$BUILD_DIR" --config Release -j"$JOBS"
fi

# Artefact roots (JUCE multi-config vs single-config)
ART="$BUILD_DIR/Fringe_artefacts"
if [[ -d "$ART/Release" ]]; then
  ART_REL="$ART/Release"
else
  ART_REL="$ART"
fi

find_bundle() {
  # find_bundle <name-pattern>
  local pattern="$1"
  find "$ART_REL" -maxdepth 3 \( -name "$pattern" -o -name "${pattern}.so" -o -name "${pattern}.clap" \) 2>/dev/null | head -1
}

VST3="$(find "$ART_REL" -type d -name 'Fringe.vst3' 2>/dev/null | head -1 || true)"
APP="$(find "$ART_REL" -type d -name 'Fringe.app' 2>/dev/null | head -1 || true)"
AU="$(find "$ART_REL" -type d -name 'Fringe.component' 2>/dev/null | head -1 || true)"
CLAP="$(find "$ART_REL" \( -type d -name 'Fringe.clap' -o -type f -name 'Fringe.clap' -o -name 'Fringe.so' \) 2>/dev/null | head -1 || true)"
# Linux VST3 is a directory; Windows is Fringe.vst3
if [[ -z "$VST3" ]]; then
  VST3="$(find "$ART_REL" -iname 'Fringe.vst3' 2>/dev/null | head -1 || true)"
fi

echo "VST3: ${VST3:-missing}"
echo "APP:  ${APP:-missing}"
echo "AU:   ${AU:-missing}"
echo "CLAP: ${CLAP:-missing}"

if [[ -z "$VST3" && -z "$APP" && -z "$AU" && -z "$CLAP" ]]; then
  echo "ERROR: no plugin artefacts found under $ART_REL" >&2
  find "$ART_REL" 2>/dev/null | head -40 || true
  exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE"

copy_docs() {
  cp "$ROOT/INSTALL.md" "$STAGE/" 2>/dev/null || true
  cp "$ROOT/CHANGELOG.md" "$STAGE/"
  cp "$ROOT/LICENSE" "$STAGE/"
  cp "$ROOT/README.md" "$STAGE/"
  cp "$ROOT/docs/KNOWN_ISSUES.md" "$STAGE/" 2>/dev/null || true
  cp "$ROOT/docs/RENOISE.md" "$STAGE/" 2>/dev/null || true
  cp "$ROOT/docs/SIGNING_AND_NOTARIZATION.md" "$STAGE/" 2>/dev/null || true
}
copy_docs

SIGN_LIST=()

if [[ -n "$VST3" ]]; then
  mkdir -p "$STAGE/VST3"
  if [[ -d "$VST3" ]]; then
    ditto "$VST3" "$STAGE/VST3/Fringe.vst3" 2>/dev/null || cp -a "$VST3" "$STAGE/VST3/Fringe.vst3"
  else
    cp -a "$VST3" "$STAGE/VST3/"
  fi
  SIGN_LIST+=("$STAGE/VST3/Fringe.vst3")
fi

if [[ -n "$APP" && -d "$APP" ]]; then
  mkdir -p "$STAGE/Standalone"
  ditto "$APP" "$STAGE/Standalone/Fringe.app" 2>/dev/null || cp -a "$APP" "$STAGE/Standalone/Fringe.app"
  SIGN_LIST+=("$STAGE/Standalone/Fringe.app")
fi

if [[ -n "$AU" && -d "$AU" ]]; then
  mkdir -p "$STAGE/AU"
  ditto "$AU" "$STAGE/AU/Fringe.component" 2>/dev/null || cp -a "$AU" "$STAGE/AU/Fringe.component"
  SIGN_LIST+=("$STAGE/AU/Fringe.component")
fi

if [[ -n "$CLAP" ]]; then
  mkdir -p "$STAGE/CLAP"
  if [[ -d "$CLAP" ]]; then
    ditto "$CLAP" "$STAGE/CLAP/Fringe.clap" 2>/dev/null || cp -a "$CLAP" "$STAGE/CLAP/Fringe.clap"
  else
    # .clap file or .so
    base="$(basename "$CLAP")"
    cp -a "$CLAP" "$STAGE/CLAP/$base"
  fi
  if [[ -e "$STAGE/CLAP/Fringe.clap" ]]; then
    SIGN_LIST+=("$STAGE/CLAP/Fringe.clap")
  fi
fi

# macOS install helpers
if [[ "$PLATFORM" == "macOS" ]]; then
  cat > "$STAGE/Install-Plugins.command" << 'EOF'
#!/bin/bash
set -e
cd "$(dirname "$0")"
if [[ -d VST3/Fringe.vst3 ]]; then
  mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
  rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/Fringe.vst3"
  ditto "VST3/Fringe.vst3" "$HOME/Library/Audio/Plug-Ins/VST3/Fringe.vst3"
  xattr -cr "$HOME/Library/Audio/Plug-Ins/VST3/Fringe.vst3" 2>/dev/null || true
  echo "Installed VST3 -> ~/Library/Audio/Plug-Ins/VST3/Fringe.vst3"
fi
if [[ -d AU/Fringe.component ]]; then
  mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
  rm -rf "$HOME/Library/Audio/Plug-Ins/Components/Fringe.component"
  ditto "AU/Fringe.component" "$HOME/Library/Audio/Plug-Ins/Components/Fringe.component"
  xattr -cr "$HOME/Library/Audio/Plug-Ins/Components/Fringe.component" 2>/dev/null || true
  echo "Installed AU  -> ~/Library/Audio/Plug-Ins/Components/Fringe.component"
fi
if [[ -d CLAP/Fringe.clap ]]; then
  mkdir -p "$HOME/Library/Audio/Plug-Ins/CLAP"
  rm -rf "$HOME/Library/Audio/Plug-Ins/CLAP/Fringe.clap"
  ditto "CLAP/Fringe.clap" "$HOME/Library/Audio/Plug-Ins/CLAP/Fringe.clap"
  xattr -cr "$HOME/Library/Audio/Plug-Ins/CLAP/Fringe.clap" 2>/dev/null || true
  echo "Installed CLAP -> ~/Library/Audio/Plug-Ins/CLAP/Fringe.clap"
fi
echo "Rescan plugins in your DAW. Press Enter to close."
read -r _
EOF
  chmod +x "$STAGE/Install-Plugins.command"
fi

# Sign / notarize
if [[ "$PLATFORM" == "macOS" ]]; then
  if [[ ${#SIGN_LIST[@]} -gt 0 ]]; then
    bash "$ROOT/scripts/sign_and_notarize_macos.sh" "${SIGN_LIST[@]}"
  fi
fi

# Zip
mkdir -p "$OUT_DIR"
ZIP_PATH="$OUT_DIR/$ZIP_NAME"
rm -f "$ZIP_PATH"
if [[ "$PLATFORM" == "macOS" ]]; then
  (
    cd "$OUT_DIR"
    ditto -c -k --sequesterRsrc --keepParent "$(basename "$STAGE")" "$ZIP_NAME"
  )
else
  (
    cd "$OUT_DIR"
    if command -v zip >/dev/null; then
      rm -f "$ZIP_NAME"
      zip -r "$ZIP_NAME" "$(basename "$STAGE")"
    else
      tar -czf "${ZIP_NAME%.zip}.tar.gz" "$(basename "$STAGE")"
      ZIP_PATH="$OUT_DIR/${ZIP_NAME%.zip}.tar.gz"
    fi
  )
fi

# Local install convenience (macOS)
if [[ "$PLATFORM" == "macOS" && -n "$VST3" ]]; then
  mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
  rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/Fringe.vst3"
  ditto "$VST3" "$HOME/Library/Audio/Plug-Ins/VST3/Fringe.vst3"
fi

echo ""
echo "==> Package ready:"
ls -lh "$ZIP_PATH" 2>/dev/null || ls -lh "$OUT_DIR"/Fringe-${VERSION}-* | tail -5
echo ""
echo "Publish:"
echo "  gh release create v${VERSION} dist/Fringe-${VERSION}-*.zip --title \"Fringe ${VERSION}\" --notes-file CHANGELOG.md"
