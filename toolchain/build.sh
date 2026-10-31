#!/bin/sh
# builds the user-space pieces of the toolchain into the sysroot
set -e
cd "$(dirname "$0")/.."
mkdir -p sysroot/usr/lib
i686-elf-gcc -m32 -c toolchain/crt0.S -o sysroot/usr/lib/crt0.o
