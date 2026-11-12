#!/bin/sh
# Cross-compile the console-side test tools from testing/src/ into testing/bin/
# (armhf, Cortex-A7), in any of the ports' Debian builder images:
#   IMAGE=gtasa-builder:bookworm testing/build.sh
#   fbshot          framebuffer screenshot (PPM on stdout; shot.sh)
#   drmshot         screenshot of the scanned-out DRM buffer (a KMS app such
#                   as the launcher, which /dev/fb0 doesn't show; root)
#   fbprobe         read pixels of /dev/fb0 (benches detect the HUD with it)
#   padpress        inject gamepad presses (written to the pad's evdev node)
#   vpad            a uinput clone of the pad that can hold buttons and
#                   combinations (the real pad's driver releases injected ones)
#   psample         ptrace PC sampler per thread (PSAMPLE_SO=<base> counts a
#                   loader's anonymous mapping of the game library)
#   pcstack         who is on the stack while a thread is inside one function
#                   (PC range): chains of return addresses into named libraries
#   stackscan       return addresses into the game on each thread's stack
#   absinfo         evdev axis ranges and raw values (static)
#   memscan         search a process's memory
#   pin             pin a thread to a core
#   perfstat        hardware counters of one thread: cycles, instructions,
#                   cache, TLB and branch misses (perf_event_open; root)
#   pmuread         with pmuon.ko loaded (src/pmuon, built by build-pmuon.sh
#                   against the kernel tree): a core's cycle and event counters
#   tlbcal          with pmuon.ko: cycles and TLB refills per access over N
#                   pages of data and of code (what a TLB miss costs here)
#   glslcheck       compile shader files with the active GLES 2 driver and
#                   print its verdict (run through a port's glos-gpu)
#   ioctlcount.so, divcount.so, memcmpcount.so   LD_PRELOAD counters (ioctls
#                   per kind; 64-bit divisions by caller; memcmp calls,
#                   bytes and bytes compared by caller)
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
$C $F -fno-builtin -shared -fPIC -o bin/memcmpcount.so src/memcmpcount.c -ldl
for t in fbshot drmshot fbprobe padpress vpad psample pcstack stackscan memscan pin; do $C $F -o bin/$t src/$t.c; done
$C $F -o bin/glslcheck src/glslcheck.c -ldl
$C $F -o bin/perfstat src/perfstat.c
$C $F -o bin/pmuread src/pmuread.c
$C $F -o bin/tlbcal src/tlbcal.c
$C $F -static -o bin/absinfo src/absinfo.c'
ls -l bin
