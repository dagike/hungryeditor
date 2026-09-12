#!/usr/bin/env bash
# Records portfolio clips of the real app and encodes them to MP4 + GIF.
#
#   tests/helpers/record_demo.sh                 # every scene
#   tests/helpers/record_demo.sh themes           # just one
#   BUILD_DIR=build/linux-debug tests/helpers/record_demo.sh
#
# Needs a built demo_recorder:
#   cmake --preset linux-release
#   cmake --build --preset linux-release --target demo_recorder
# and ffmpeg on PATH. Output lands in demo-recordings/ at the repo root
# (git-ignored — it's regeneratable, not committed).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build/linux-release}"
RECORDER="$BUILD_DIR/bin/demo_recorder"
OUT_DIR="$REPO_ROOT/demo-recordings"

FPS="${FPS:-15}"
WIDTH="${WIDTH:-1280}"
HEIGHT="${HEIGHT:-800}"
GIF_WIDTH="${GIF_WIDTH:-720}"
GIF_FPS="${GIF_FPS:-8}"
GIF_SECONDS="${GIF_SECONDS:-15}"
KEEP_FRAMES="${KEEP_FRAMES:-0}"

if [ ! -x "$RECORDER" ]; then
    echo "demo_recorder not built. Run:" >&2
    echo "  cmake --preset linux-release" >&2
    echo "  cmake --build --preset linux-release --target demo_recorder" >&2
    exit 1
fi
if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "ffmpeg not found on PATH" >&2
    exit 1
fi

if [ "$#" -gt 0 ]; then
    SCENES=("$@")
else
    mapfile -t SCENES < <("$RECORDER" --list | awk '{print $1}')
fi

mkdir -p "$OUT_DIR"

for scene in "${SCENES[@]}"; do
    echo "== $scene =="
    FRAMES="$OUT_DIR/frames/$scene"
    rm -rf "$FRAMES"
    mkdir -p "$FRAMES"

    "$RECORDER" --scene "$scene" --out "$FRAMES" --fps "$FPS" --width "$WIDTH" --height "$HEIGHT"

    if [ -z "$(ls -A "$FRAMES" 2>/dev/null)" ]; then
        echo "no frames written for scene '$scene'" >&2
        exit 1
    fi

    ffmpeg -y -framerate "$FPS" -i "$FRAMES/frame_%05d.png" \
        -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2:flags=lanczos,format=yuv420p" \
        -c:v libx264 -preset slow -crf 18 -tune stillimage -profile:v high -r 30 \
        -color_primaries bt709 -color_trc bt709 -colorspace bt709 -color_range tv \
        -movflags +faststart "$OUT_DIR/$scene.mp4"

    FILTERS="fps=$GIF_FPS,scale=$GIF_WIDTH:-1:flags=lanczos"
    PALETTE="$OUT_DIR/$scene-palette.png"
    ffmpeg -y -framerate "$FPS" -i "$FRAMES/frame_%05d.png" -t "$GIF_SECONDS" \
        -vf "$FILTERS,palettegen=stats_mode=diff:max_colors=192" -update 1 "$PALETTE"
    ffmpeg -y -framerate "$FPS" -i "$FRAMES/frame_%05d.png" -i "$PALETTE" -t "$GIF_SECONDS" \
        -lavfi "$FILTERS[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle" \
        -loop 0 "$OUT_DIR/$scene.gif"
    rm -f "$PALETTE"

    if [ "$KEEP_FRAMES" != "1" ]; then
        rm -rf "$FRAMES"
    fi
done

echo
ls -lh "$OUT_DIR"/*.mp4 "$OUT_DIR"/*.gif 2>/dev/null || true
