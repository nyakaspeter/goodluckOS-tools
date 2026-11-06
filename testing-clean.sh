#!/bin/sh
# Remove the console-side testing tools and a bench's temporary start script
# from a game folder on the console (the opposite of testing-sync.sh).
#   ../goodluckOS-tools/testing-clean.sh <id>
set -eu
id=${1:?usage: testing-clean.sh <id of /home/player/ports/<id>>}
ROOT=$(cd "$(dirname "$0")" && pwd)
(cd "$ROOT" && GLOS_TIMEOUT=60 python3 -I console.py "rm -rf /home/player/ports/$id/glos/testing /home/player/ports/$id/start.bench.sh; ls /home/player/ports/$id/glos")
