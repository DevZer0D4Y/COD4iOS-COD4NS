#!/usr/bin/env bash
# Transcode the game's Bink movies to MP4 so the Apple port can play them.
#
# No Bink runtime exists for iOS, so ports/ios/engine/cinematic_apple.cpp plays video/<name>.mp4
# instead of video/<name>.bik. This converts the files in place, next to the originals, using
# ffmpeg (brew install ffmpeg). Already-converted files are skipped, so re-running is cheap.
#
# Usage:
#   ports/ios/scripts/convert_videos.sh <game-dir> [name ...]
#
# With no names, the startup and menu movies are converted (the whole set is ~2.5 GB of source
# material and takes a long time). Pass names without extension to convert specific movies, or
# "all" for everything.
set -euo pipefail

GAME_DIR="${1:?usage: convert_videos.sh <game-dir> [name ...]}"
shift || true
VIDEO_DIR="$GAME_DIR/main/video"
[ -d "$VIDEO_DIR" ] || { echo "No video directory at $VIDEO_DIR" >&2; exit 1; }
command -v ffmpeg >/dev/null || { echo "ffmpeg not found (brew install ffmpeg)" >&2; exit 1; }

# Legal screens, the main-menu attract loop and the campaign intro.
DEFAULT_NAMES=(atvi iw_logo infinity_ward attract intro_movie)

names=("$@")
if [ ${#names[@]} -eq 0 ]; then
    names=("${DEFAULT_NAMES[@]}")
elif [ "${names[0]}" = "all" ]; then
    names=()
    for file in "$VIDEO_DIR"/*.bik; do
        names+=("$(basename "$file" .bik)")
    done
fi

converted=0
for name in "${names[@]}"; do
    src="$VIDEO_DIR/$name.bik"
    dst="$VIDEO_DIR/$name.mp4"
    if [ ! -f "$src" ]; then
        echo "skip $name (no $name.bik)"
        continue
    fi
    if [ -f "$dst" ] && [ "$dst" -nt "$src" ]; then
        echo "have $name.mp4"
        continue
    fi
    echo "converting $name ..."
    # H.264 high profile with yuv420p, which VideoToolbox decodes on every iOS device; audio to
    # AAC. -nostdin keeps ffmpeg from swallowing the terminal when run from another script.
    nice -n 19 ffmpeg -nostdin -loglevel error -y -i "$src" \
        -c:v libx264 -preset veryfast -crf 20 -pix_fmt yuv420p -movflags +faststart \
        -c:a aac -b:a 160k "$dst"
    converted=$((converted + 1))
done
echo "done ($converted converted)"
