#!/bin/sh
set -e
. ./build.sh

# user programs, built with the bobliu toolchain wrapper
./toolchain/build.sh
mkdir -p initrd/bin
for src in user/*.c; do
  prog=$(basename $src .c)
  ./toolchain/i686-bobliu-gcc -O2 -o initrd/bin/$prog $src
done

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/myos.kernel isodir/boot/myos.kernel
COPYFILE_DISABLE=1 tar --no-xattrs --exclude=._* -C initrd -cf isodir/boot/initrd.tar .
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "bobliu (user shell)" {
    multiboot /boot/myos.kernel init=/bin/sh
    module   /boot/initrd.tar initrd
}
menuentry "bobliu (kernel shell)" {
    multiboot /boot/myos.kernel
    module   /boot/initrd.tar initrd
}
menuentry "bobliu (noshell)" {
    multiboot /boot/myos.kernel noshell
    module   /boot/initrd.tar initrd
}
EOF
i686-elf-grub-mkrescue -o bobliu.iso isodir