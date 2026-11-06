#!/bin/bash
# Milestone 5: cross-compile neovim's C sources for bobliu. Needs build-libuv.sh, build-deps.sh and
# gen-nvim-sources.sh run first.
#   ports/neovim/build-nvim.sh [workdir] [-k]      -k: only report compile errors, do not link
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
WORK="${1:-$HOME/bobliu-os-work/nvim-port}"
export PATH="$HOME/opt/cross/bin:$PATH"
CC="$ROOT/toolchain/i686-bobliu-gcc"
NV="$WORK/neovim"; PREFIX="$WORK/prefix"; OUT="$WORK/build/nvim"
mkdir -p "$OUT"
CFLAGS="-O2 -w -std=gnu99 -fsigned-char -DHAVE_UNIBILIUM -DINCLUDE_GENERATED_DECLARATIONS -D_GNU_SOURCE -D__GNU__ \
 -I$HERE/nvim-config -I$NV/build/src/nvim/auto -I$NV/build/include -I$NV/src \
 -I$PREFIX/include -I$ROOT/lua/src -I$WORK/libuv/include"
build_one() {
  f="$1"; o="$OUT/$(echo "$f" | tr / _ | sed 's/\.c$//').o"
  if [ -f "$o" ] && [ "$o" -nt "$NV/$f" ] && [ -z "${FORCE:-}" ]; then echo "ok  $f"; return 0; fi
  if $CC -c $CFLAGS "$NV/$f" -o "$o" 2> "$o.err"; then echo "ok  $f"; rm -f "$o.err"
  else echo "ERR $f"; rm -f "$o"; fi
}
export -f build_one; export CC CFLAGS NV OUT
cat "$HERE/nvim-sources.txt" | xargs -P 6 -I{} bash -c 'build_one {}' | sort | uniq -c | awk '{print $2}' | sort | uniq -c
echo "failed files:"; ls "$OUT"/*.err 2>/dev/null | sed 's#.*/##; s#\.o\.err##' | tr '\n' ' '; echo

[ "${1:-}" = "-k" ] || [ "${2:-}" = "-k" ] && exit 0
# link
PREFIXLIB="$PREFIX/lib"
OBJS=$(ls "$OUT"/*.o 2>/dev/null)
$CC -O2 -o "$WORK/build/nvim.bin" $OBJS \
  "$PREFIXLIB/libluv.a" "$WORK/build/libuv.a" "$PREFIXLIB/liblpeg.a" "$PREFIXLIB/liblua.a" \
  "$PREFIXLIB/libts-parsers.a" "$PREFIXLIB/libtree-sitter.a" "$PREFIXLIB/libvterm.a" \
  "$PREFIXLIB/libunibilium.a" "$PREFIXLIB/libmsgpack-c.a" 2> "$WORK/build/nvim-link.log"
grep -c "undefined reference" "$WORK/build/nvim-link.log" | sed 's/^/undefined references: /'
