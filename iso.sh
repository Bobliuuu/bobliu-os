#!/bin/sh
set -e
. ./build.sh

# user programs, built with the bobliu toolchain wrapper
./toolchain/build.sh
mkdir -p initrd/bin initrd/tmp initrd/dev initrd/disk
for src in user/*.c; do
  prog=$(basename $src .c)
  ./toolchain/i686-bobliu-gcc -O2 -o initrd/bin/$prog $src
done
# vendored third-party programs
./toolchain/i686-bobliu-gcc -O2 -o initrd/bin/kilo third_party/kilo/kilo.c

# doom (doomgeneric port); the iwad is picked up from doom/wad/
./doom/build.sh
./lua/build.sh

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

# scratch disk for the ata driver (qemu.sh attaches it); never overwritten once it exists
if [ ! -f disk.img ]; then
  dd if=/dev/zero of=disk.img bs=1024 count=4096 2>/dev/null
  printf 'bobliu disk: sector 0\n' | dd of=disk.img conv=notrunc 2>/dev/null
fi
