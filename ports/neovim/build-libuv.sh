#!/bin/bash
# Cross-builds libuv 1.48.0 for bobliu (generic POSIX poll backend) and the uvtest programs.
#   ports/neovim/build-libuv.sh [workdir]      default workdir: ~/bobliu-os-work/nvim-port
# Needs: ./iso.sh run once (sysroot + libc.a), the i686-elf toolchain on PATH.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
WORK="${1:-$HOME/bobliu-os-work/nvim-port}"
export PATH="$HOME/opt/cross/bin:$PATH"
CC="$ROOT/toolchain/i686-bobliu-gcc"
mkdir -p "$WORK"; cd "$WORK"

if [ ! -d libuv ]; then
  git clone -q --depth 1 --branch v1.48.0 https://github.com/libuv/libuv
  (cd libuv && git apply "$HERE/patches/libuv-0001-spawn-use-pipes.patch")
fi

OUT="$WORK/build/libuv"; mkdir -p "$OUT"
cd libuv
# -D__GNU__ selects libuv's generic POSIX path (uv/posix.h, posix-poll.c, no epoll/kqueue)
SRCS="src/fs-poll.c src/idna.c src/inet.c src/random.c src/strscpy.c src/strtok.c src/thread-common.c \
src/threadpool.c src/timer.c src/uv-common.c src/uv-data-getter-setters.c src/version.c \
src/unix/async.c src/unix/core.c src/unix/dl.c src/unix/fs.c src/unix/getaddrinfo.c src/unix/getnameinfo.c \
src/unix/loop-watcher.c src/unix/loop.c src/unix/pipe.c src/unix/poll.c src/unix/process.c \
src/unix/random-devurandom.c src/unix/signal.c src/unix/stream.c src/unix/tcp.c src/unix/thread.c \
src/unix/tty.c src/unix/udp.c src/unix/no-proctitle.c src/unix/posix-hrtime.c src/unix/posix-poll.c \
src/unix/no-fsevents.c"
for f in $SRCS; do
  $CC -c -O2 -Iinclude -Isrc -D_GNU_SOURCE -D__GNU__ "$f" -o "$OUT/$(echo "$f" | tr / _ | tr -d .).o"
done
rm -f "$WORK/build/libuv.a"
i686-elf-ar rcs "$WORK/build/libuv.a" "$OUT"/*.o
echo "built $WORK/build/libuv.a"

for t in uvtest uvtest2; do
  $CC -O2 -D__GNU__ -Iinclude -o "$WORK/build/$t" "$HERE/tests/$t.c" "$WORK/build/libuv.a"
  cp "$WORK/build/$t" "$ROOT/initrd/bin/$t"      # initrd/bin is gitignored; rerun ./iso.sh to pack it
done
echo "installed uvtest, uvtest2 into initrd/bin (run ./iso.sh to pack)"
