#!/bin/sh
# Flash a goodluckOS boot image (android_boot.img) to an SD card, with backup
# and read-back verification. Only the boot region at sector 172032 is touched.
# Usage: flash-boot.sh /dev/diskN path/to/android_boot.img
#        flash-boot.sh /dev/diskN boot-backup-*.img      (to restore)
set -eu
DISK=$1
IMG=$2
RAW=$(echo "$DISK" | sed 's|/dev/disk|/dev/rdisk|')
SECTOR=172032
HERE=$(cd "$(dirname "$0")" && pwd)
BACKUP="$HERE/boot-backup-$(date +%Y%m%d-%H%M%S).img"
SECTORS=$(( ($(stat -f %z "$IMG") + 511) / 512 ))

[ "$(head -c 8 "$IMG")" = "ANDROID!" ] || { echo "$IMG is not an Android boot image"; exit 1; }

echo "Backing up current boot region to $BACKUP"
sudo dd if="$RAW" of="$BACKUP" bs=512 skip=$SECTOR count=16384 2>/dev/null
sudo chown "$(id -u)" "$BACKUP"
[ "$(head -c 8 "$BACKUP")" = "ANDROID!" ] || { echo "No boot image at sector $SECTOR on $DISK - wrong card? Nothing written."; exit 1; }

diskutil unmountDisk "$DISK" >/dev/null
echo "Writing $IMG ($SECTORS sectors) at sector $SECTOR"
sudo dd if="$IMG" of="$RAW" bs=512 seek=$SECTOR conv=notrunc 2>/dev/null
sync
if sudo dd if="$RAW" bs=512 skip=$SECTOR count=$SECTORS 2>/dev/null | head -c "$(stat -f %z "$IMG")" | cmp -s - "$IMG"; then
    echo "Verified OK"
else
    echo "VERIFY FAILED - restore with: $0 $DISK $BACKUP"
    exit 1
fi
diskutil eject "$DISK" >/dev/null && echo "Ejected $DISK - FLASH_DONE"
