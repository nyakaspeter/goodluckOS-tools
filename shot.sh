#!/bin/sh
# Screenshot of the console's framebuffer into a PNG (default shot.png),
# through fbshot in a game folder's glos/testing/ (testing-sync.sh first).
#   ../goodluckOS-tools/shot.sh <id> [file.png] [scale-down factor, default 1]
set -eu
id=${1:?usage: shot.sh <id> [file.png] [factor]}
ROOT=$(cd "$(dirname "$0")" && pwd)
out=${2:-shot.png}; f=${3:-1}
(cd "$ROOT" && GLOS_TIMEOUT=60 python3 -I console.py "/home/player/ports/$id/glos/testing/fbshot $f | gzip | base64 -w0; echo") |
	tr -d '\r\n' | sed 's/^[^H]*\(H4sI\)/\1/' | base64 -d | gunzip > "$out.ppm"
sips -s format png "$out.ppm" --out "$out" >/dev/null && rm "$out.ppm"
echo "$out"
