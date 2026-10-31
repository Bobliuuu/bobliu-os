#!/bin/sh
set -e
. ./build.sh

# user programs (needs headers in sysroot, build.sh installs them)
i686-elf-gcc -ffreestanding -nostdlib -O2 -m32 -fno-pic -fno-stack-protector \
  -I sysroot/usr/include -T initrd/hello.ld -o initrd/hello initrd/hello.c

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/myos.kernel isodir/boot/myos.kernel
COPYFILE_DISABLE=1 tar --no-xattrs --exclude=._* -C initrd -cf isodir/boot/initrd.tar .
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "bobliu (shell)" {
    multiboot /boot/myos.kernel
    module   /boot/initrd.tar initrd
}
menuentry "bobliu (noshell)" {
    multiboot /boot/myos.kernel noshell
    module   /boot/initrd.tar initrd
}
EOF
i686-elf-grub-mkrescue -o bobliu.iso isodir