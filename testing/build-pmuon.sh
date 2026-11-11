#!/bin/sh
# Build testing/src/pmuon into bin/pmuon.ko against the stock goodluckOS kernel
# tree (Docker image glos-kernel:ready, made by
# ../goodluckOS-mali-driver/make-kernel-image.sh).
set -eu
cd "$(dirname "$0")"
mkdir -p bin
docker run --rm -v "$PWD/src/pmuon":/m:ro -v "$PWD/bin":/out "${KERNEL_IMAGE:-glos-kernel:ready}" sh -c '
    cp -r /m /tmp/m && make -C /src/linux M=/tmp/m ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules > /tmp/build.log 2>&1;
    rc=$?; [ $rc = 0 ] || tail -20 /tmp/build.log; cat /tmp/m/pmuon.ko > /out/pmuon.ko 2>/dev/null; exit $rc'
ls -l bin/pmuon.ko
