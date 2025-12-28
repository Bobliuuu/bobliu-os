#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/myos.kernel isodir/boot/myos.kernel
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "bobliu" {
	multiboot /boot/myos.kernel
	boot
}
EOF
i686-elf-grub-mkrescue -o bobliu.iso isodir