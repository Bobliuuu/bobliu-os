#!/bin/bash
# Milestone 4: run neovim's code generators on the HOST (they need LuaJIT and native tools) and leave
# the generated headers in $WORK/neovim/build for the cross-build to include.
#   ports/neovim/gen-nvim-sources.sh [workdir]      default workdir: ~/bobliu-os-work/nvim-port
# Host requirements: cmake, ninja, luajit, a C compiler, and Homebrew's libuv, luv, lpeg,
# unibilium and tree-sitter (msgpack-c and libvterm are built from neovim's bundled recipes).
set -e
WORK="${1:-$HOME/bobliu-os-work/nvim-port}"
BREW="${BREW_PREFIX:-/opt/homebrew}"
mkdir -p "$WORK"; cd "$WORK"
[ -d neovim ] || git clone -q --depth 1 --branch v0.10.4 https://github.com/neovim/neovim
cd neovim

# host-only dependencies that Homebrew does not ship
cmake -S cmake.deps -B .deps -G Ninja -DUSE_BUNDLED=OFF -DUSE_BUNDLED_MSGPACK=ON -DUSE_BUNDLED_LIBVTERM=ON \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$BREW" >/dev/null
cmake --build .deps >/dev/null

# COMPILE_LUA=OFF: embed Lua sources as text. LuaJIT bytecode would be unreadable by bobliu's plain Lua 5.1.
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCOMPILE_LUA=OFF -DENABLE_LIBINTL=OFF \
      -DLUA_PRG="$(command -v luajit)" -DDEPS_PREFIX="$PWD/.deps/usr" \
      -DCMAKE_PREFIX_PATH="$PWD/.deps/usr;$BREW;$BREW/opt/libuv;$BREW/opt/luv;$BREW/opt/lpeg;$BREW/opt/unibilium;$BREW/opt/tree-sitter" >/dev/null
ninja -C build generated-sources

echo "generated headers: $(find build -name '*.generated.h' | wc -l)"
