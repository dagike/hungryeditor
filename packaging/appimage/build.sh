#!/usr/bin/env bash
# Build a self-contained AppImage.
#
# Needs, on top of the usual build dependencies: patchelf (used by
# linuxdeploy to rewrite bundled binaries' RPATHs) and either FUSE2 or the
# willingness to run the downloaded tools with --appimage-extract-and-run
# (this script already does, so FUSE is not required).
#
# Usage: packaging/appimage/build.sh [build-dir]
#
# First run downloads linuxdeploy, linuxdeploy-plugin-qt and appimagetool
# into packaging/appimage/tools/ (gitignored) and reuses them afterward.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-$ROOT/build/appimage}"
TOOLS_DIR="$ROOT/packaging/appimage/tools"
APPDIR="$BUILD_DIR/AppDir"

mkdir -p "$TOOLS_DIR"

fetch() {
    local url="$1" out="$2"
    if [ ! -x "$out" ]; then
        echo "Fetching $(basename "$out")..."
        curl -L --fail -o "$out" "$url"
        chmod +x "$out"
    fi
}

# Pinned to specific tags, not the "continuous" rolling release: neither
# linuxdeploy project publishes stable SemVer tags, only dated alpha
# snapshots, but even those are still a fixed, reproducible target — an
# upstream rebuild of "continuous" can otherwise change what CI fetches
# without any change on our side. Bump these deliberately when needed.
fetch "https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage" \
    "$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
# Saved without the usual "-x86_64.AppImage" suffix: linuxdeploy discovers
# plugins by looking for a "linuxdeploy-plugin-<name>" executable on PATH.
fetch "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage" \
    "$TOOLS_DIR/linuxdeploy-plugin-qt"
fetch "https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage" \
    "$TOOLS_DIR/appimagetool-x86_64.AppImage"

cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --target hungryeditor -j"$(nproc)"

rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr

VERSION="$(cd "$ROOT" && git describe --tags --always 2>/dev/null || echo dev)"
export PATH="$TOOLS_DIR:$PATH"
export VERSION

"$TOOLS_DIR/linuxdeploy-x86_64.AppImage" --appimage-extract-and-run \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/hungryeditor" \
    --desktop-file "$ROOT/packaging/linux/hungryeditor.desktop" \
    --icon-file "$ROOT/resources/icons/hungryeditor.svg" \
    --plugin qt

"$TOOLS_DIR/appimagetool-x86_64.AppImage" --appimage-extract-and-run \
    "$APPDIR" "$BUILD_DIR/hungryeditor-${VERSION}-x86_64.AppImage"

echo "Built: $BUILD_DIR/hungryeditor-${VERSION}-x86_64.AppImage"
