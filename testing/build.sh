#!/bin/sh
# Cross-compile the console-side test tools from testing/src/ into testing/bin/
# (armhf, Cortex-A7), in any of the ports' Debian builder images:
#   IMAGE=gtasa-builder:bookworm testing/build.sh
#   fbshot          framebuffer screenshot (PPM on stdout; shot.sh)
#   drmshot         screenshot of the scanned-out DRM buffer (a KMS app such
#                   as the launcher, which /dev/fb0 doesn't show; root)
#   fbprobe         read pixels of /dev/fb0 (benches detect the HUD with it)
#   padpress        inject gamepad presses (uinput)
#   psample         ptrace PC sampler per thread (PSAMPLE_SO=<base> counts a
#                   loader's anonymous mapping of the game library)
#   stackscan       return addresses into the game on each thread's stack
#   absinfo         evdev axis ranges and raw values (static)
#   memscan         search a process's memory
#   pin             pin a thread to a core
#   ioctlcount.so, divcount.so   LD_PRELOAD counters (ioctls per kind; 64-bit
#                   divisions by caller)
# testing-sync.sh copies bin/ to a game folder on the console while a test
# needs it; testing-clean.sh takes it off again. Never shipped.
set -eu
cd "$(dirname "$0")"
mkdir -p bin
docker run --rm -v "$PWD":/w -w /w "${IMAGE:-gtasa-builder:bookworm}" sh -c '
F="-O2 -mcpu=cortex-a7 -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"
C=arm-linux-gnueabihf-gcc
$C $F -shared -fPIC -o bin/ioctlcount.so src/ioctlcount.c -ldl
$C $F -shared -fPIC -o bin/divcount.so src/divcount.c -ldl
for t in fbshot drmshot fbprobe padpress psample stackscan memscan pin; do $C $F -o bin/$t src/$t.c; done
$C $F -static -o bin/absinfo src/absinfo.c'
ls -l bin
