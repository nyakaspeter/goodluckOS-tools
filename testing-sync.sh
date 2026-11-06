#!/bin/sh
# Copy testing/bin/ (the console-side bench and profiling tools) into a game
# folder on the console, /home/player/ports/<id>/glos/testing/, for a test run.
# Game folders never ship it; testing-clean.sh removes it again.
#   ../goodluckOS-tools/testing-sync.sh <id>      (id: gtasa, lcs, jk2, ...)
set -eu
id=${1:?usage: testing-sync.sh <id of /home/player/ports/<id>>}
ROOT=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d); mkdir "$tmp/testing" && cp "$ROOT"/testing/bin/* "$tmp/testing/"
(cd "$tmp" && COPYFILE_DISABLE=1 tar --no-xattrs -cf testing.tar testing)
pkill -9 -x ptpcamerad 2>/dev/null || true   # macOS grabs the console's MTP interface after the first transfer
(cd "$ROOT" && python3 -I mtp_send.py / "$tmp/testing.tar" | tail -1)
(cd "$ROOT" && GLOS_TIMEOUT=60 python3 -I console.py "cd /home/player/ports/$id/glos && rm -rf testing && tar xf /home/player/testing.tar 2>/dev/null; rm -f /home/player/testing.tar; chmod 755 testing/*; ls testing | tr '\n' ' '")
rm -rf "$tmp"
