#!/bin/sh
# builds doom (doomgeneric, vendored in doom/src) into initrd/bin/doom and puts the IWAD in initrd/share.
# the wad is not in the repo: drop freedoom1.wad (or any doom iwad renamed to a known name) into doom/wad/.
set -e
cd "$(dirname "$0")"
CC=../toolchain/i686-bobliu-gcc
mkdir -p build ../initrd/bin ../initrd/share
objs=""
for n in $(cat objs.txt); do
  if [ ! -f build/$n.o ] || [ src/$n.c -nt build/$n.o ] || [ ../libc/include/stdio.h -nt build/$n.o ]; then
    $CC -O2 -w -c -DNORMALUNIX -Isrc src/$n.c -o build/$n.o
  fi
  objs="$objs build/$n.o"
done
$CC -O2 -w -c -DNORMALUNIX -Isrc bobliu.c -o build/bobliu.o
$CC -O2 -s -o ../initrd/bin/doom $objs build/bobliu.o 2>&1 | grep -v "RWX" || true
for w in wad/*.wad; do [ -f "$w" ] && cp "$w" ../initrd/share/; done
exit 0
