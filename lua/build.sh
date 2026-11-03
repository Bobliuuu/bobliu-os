#!/bin/sh
# builds lua 5.1.5 (plain C, no readline/dlopen/popen) into initrd/bin/lua
set -e
cd "$(dirname "$0")"
CC=../toolchain/i686-bobliu-gcc
mkdir -p build ../initrd/bin
objs=""
for f in src/*.c; do
  n=$(basename $f .c)
  if [ ! -f build/$n.o ] || [ $f -nt build/$n.o ] || [ ../libc/include/stdio.h -nt build/$n.o ]; then
    $CC -O2 -w -c -Isrc $f -o build/$n.o
  fi
  objs="$objs build/$n.o"
done
$CC -O2 -s -o ../initrd/bin/lua $objs 2>&1 | grep -v "RWX" || true
exit 0
