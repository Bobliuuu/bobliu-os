#!/bin/sh
set -e
. ./iso.sh

qemu-system-$(./target-triplet-to-arch.sh $HOST) -cdrom bobliu.iso -drive file=disk.img,format=raw,if=ide -no-reboot -no-shutdown -d int,cpu_reset -D qemu.log