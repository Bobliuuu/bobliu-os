#!/bin/bash
# Fetches and cross-builds neovim's pure-C dependencies for bobliu into $WORK/prefix/{include,lib}.
#   ports/neovim/build-deps.sh [workdir]      default workdir: ~/bobliu-os-work/nvim-port
# Libraries: msgpack-c, unibilium, libvterm, lpeg, luv (+ lua-compat-5.3), tree-sitter and the six
# bundled parsers. Versions are the ones pinned by neovim v0.10.4 (cmake.deps/deps.txt).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
WORK="${1:-$HOME/bobliu-os-work/nvim-port}"
export PATH="$HOME/opt/cross/bin:$PATH"
CC="$ROOT/toolchain/i686-bobliu-gcc"
AR=i686-elf-ar
LUAINC="$ROOT/lua/src"
LIBUVINC="$WORK/libuv/include"
SRC="$WORK/deps-src"; PREFIX="$WORK/prefix"; OBJ="$WORK/build/deps"
mkdir -p "$SRC" "$PREFIX/include" "$PREFIX/lib" "$OBJ"
FAILED=0

fetch() {   # name url
  [ -d "$SRC/$1" ] && return
  mkdir "$SRC/$1" && curl -sL "$2" | tar xz -C "$SRC/$1" --strip-components=1
}
fetch msgpack    https://github.com/msgpack/msgpack-c/archive/c-6.0.1.tar.gz
fetch unibilium  https://github.com/neovim/unibilium/archive/d72c3598e7ac5d1ebf86ee268b8b4ed95c0fa628.tar.gz
fetch libvterm   https://github.com/neovim/libvterm/archive/0a15c6e983b0db7ef8276e0792414a805d01bdaf.tar.gz
fetch luv        https://github.com/luvit/luv/releases/download/1.48.0-2/luv-1.48.0-2.tar.gz
fetch lpeg       https://github.com/neovim/deps/raw/d495ee6f79e7962a53ad79670cb92488abe0b9b4/opt/lpeg-1.1.0.tar.gz
fetch tree-sitter https://github.com/tree-sitter/tree-sitter/archive/v0.22.6.tar.gz
fetch ts-c        https://github.com/tree-sitter/tree-sitter-c/archive/v0.21.3.tar.gz
fetch ts-lua      https://github.com/tree-sitter-grammars/tree-sitter-lua/archive/v0.1.0.tar.gz
fetch ts-vim      https://github.com/neovim/tree-sitter-vim/archive/v0.4.0.tar.gz
fetch ts-vimdoc   https://github.com/neovim/tree-sitter-vimdoc/archive/v3.0.0.tar.gz
fetch ts-query    https://github.com/tree-sitter-grammars/tree-sitter-query/archive/v0.4.0.tar.gz
fetch ts-markdown https://github.com/MDeiml/tree-sitter-markdown/archive/v0.2.3.tar.gz

# lib NAME "extra cc flags" file...   (paths relative to the current dir); prints errors, never aborts
lib() {
  local name="$1" flags="$2"; shift 2
  local d="$OBJ/$name"; rm -rf "$d"; mkdir -p "$d"
  local objs=() i=0 ok=1
  for f in "$@"; do
    i=$((i+1)); local o="$d/$i-$(basename "$f" .c).o"
    if ! $CC -c -O2 -w $flags "$f" -o "$o" 2> "$d/$i.err"; then
      echo "FAIL $name: $f"; head -5 "$d/$i.err"; ok=0; continue
    fi
    objs+=("$o")
  done
  rm -f "$PREFIX/lib/lib$name.a"; $AR rcs "$PREFIX/lib/lib$name.a" "${objs[@]}"
  [ $ok = 1 ] && echo "ok   lib$name.a (${#objs[@]} objects)" || { echo "BAD  lib$name.a"; FAILED=1; }
}

cd "$SRC/msgpack"
# the two headers cmake would generate: little-endian i686
sed -e 's/@MSGPACK_ENDIAN_BIG_BYTE@/0/' -e 's/@MSGPACK_ENDIAN_LITTLE_BYTE@/1/' cmake/sysdep.h.in > include/msgpack/sysdep.h
cp cmake/pack_template.h.in include/msgpack/pack_template.h
cp -r include/msgpack include/msgpack.h "$PREFIX/include/"
lib msgpack-c "-Iinclude" src/objectc.c src/unpack.c src/version.c src/vrefbuffer.c src/zone.c

cd "$SRC/unibilium"
cp unibilium.h "$PREFIX/include/"
printf '#define TERMINFO_DIRS "/share/terminfo"\n' > "$OBJ/unibilium-config.h"
lib unibilium "-I. -include $OBJ/unibilium-config.h" unibilium.c uninames.c uniutil.c

cd "$SRC/libvterm"
cp include/*.h "$PREFIX/include/"
lib vterm "-Iinclude -Isrc -std=gnu99" src/encoding.c src/keyboard.c src/mouse.c src/parser.c src/pen.c src/screen.c src/state.c src/unicode.c src/vterm.c

cd "$SRC/lpeg"
lib lpeg "-I$LUAINC" lpcap.c lpcode.c lpcset.c lpprint.c lptree.c lpvm.c

cd "$SRC/luv"
lib luv "-I$LUAINC -I$LIBUVINC -Ideps/lua-compat-5.3/c-api -Isrc -D_GNU_SOURCE -D__GNU__ -DLUV_USE_COMPAT=1" src/luv.c deps/lua-compat-5.3/c-api/compat-5.3.c
cp src/luv.h "$PREFIX/include/" 2>/dev/null || true

cd "$SRC/tree-sitter"
cp lib/include/tree_sitter/api.h "$PREFIX/include/tree_sitter.api.h"
mkdir -p "$PREFIX/include/tree_sitter" && cp lib/include/tree_sitter/api.h "$PREFIX/include/tree_sitter/"
lib tree-sitter "-Ilib/include -Ilib/src -D_POSIX_C_SOURCE=200112L -D_DEFAULT_SOURCE -std=gnu99" lib/src/lib.c

# bundled parsers: one archive, each parser's symbols are distinct (tree_sitter_<lang>)
cd "$SRC"
lib ts-parsers "-w -I$SRC/ts-c/src -I$SRC/ts-lua/src -I$SRC/ts-vim/src -I$SRC/ts-vimdoc/src -I$SRC/ts-query/src -I$SRC/ts-markdown/tree-sitter-markdown/src -I$SRC/ts-markdown/tree-sitter-markdown-inline/src" \
  ts-c/src/parser.c \
  ts-lua/src/parser.c ts-lua/src/scanner.c \
  ts-vim/src/parser.c ts-vim/src/scanner.c \
  ts-vimdoc/src/parser.c \
  ts-query/src/parser.c \
  ts-markdown/tree-sitter-markdown/src/parser.c ts-markdown/tree-sitter-markdown/src/scanner.c \
  ts-markdown/tree-sitter-markdown-inline/src/parser.c ts-markdown/tree-sitter-markdown-inline/src/scanner.c

# Lua 5.1 as a library (the interpreter's own objects minus main), then the link test
mkdir -p "$ROOT/lua/build"; "$ROOT/lua/build.sh" >/dev/null 2>&1 || true
rm -f "$PREFIX/lib/liblua.a"
$AR rcs "$PREFIX/lib/liblua.a" $(ls "$ROOT"/lua/build/*.o | grep -v '/lua\.o$')
if [ -f "$WORK/build/libuv.a" ]; then
  $CC -O2 -w -D__GNU__ -I"$PREFIX/include" -I"$LUAINC" -I"$LIBUVINC" -o "$WORK/build/depstest" "$HERE/tests/depstest.c" \
    "$PREFIX/lib/libluv.a" "$WORK/build/libuv.a" "$PREFIX/lib/liblpeg.a" "$PREFIX/lib/liblua.a" \
    "$PREFIX/lib/libts-parsers.a" "$PREFIX/lib/libtree-sitter.a" "$PREFIX/lib/libvterm.a" \
    "$PREFIX/lib/libunibilium.a" "$PREFIX/lib/libmsgpack-c.a" 2>&1 | grep -v RWX || true
  [ -f "$WORK/build/depstest" ] && cp "$WORK/build/depstest" "$ROOT/initrd/bin/depstest" && echo "installed depstest into initrd/bin"
fi

[ $FAILED = 0 ] && echo "all dependencies built into $PREFIX" || { echo "some dependencies failed"; exit 1; }
