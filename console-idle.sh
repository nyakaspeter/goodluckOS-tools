#!/bin/sh
# Is the console free for a test run? Exits 0 when nobody is playing: no app
# running under appd (test runs stop theirs when done, so a running app is
# the player's) and no pad input for 5 seconds. Prints why it is busy.
#   ../goodluckOS-tools/console-idle.sh && <test run>
set -eu
ROOT=$(cd "$(dirname "$0")" && pwd)
out=$(cd "$ROOT" && GLOS_TIMEOUT=20 python3 -I console.py '
# cgroup files report size 0, so read one: any line means a process
if read -r x < /sys/fs/cgroup/appd/app/cgroup.procs 2>/dev/null; then echo BUSY-APP; fi
cat /dev/input/event2 > /dev/shm/padwatch & p=$!; sleep 5; kill $p
[ -s /dev/shm/padwatch ] && echo BUSY-PAD; rm -f /dev/shm/padwatch; echo CHECKED' 2>&1)
case "$out" in
*CHECKED*) ;;
*) echo "console not answering"; exit 2 ;;
esac
case "$out" in *BUSY-APP*) echo "an app is running (the player's)"; exit 1 ;; esac
case "$out" in *BUSY-PAD*) echo "the pad is in use"; exit 1 ;; esac
echo "console idle"
