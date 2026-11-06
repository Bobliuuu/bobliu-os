#!/bin/bash
# Copies a minimal neovim runtime into initrd/share/nvim/runtime (initrd/share is gitignored; rerun ./iso.sh to pack).
# The full runtime is ~30 MB and the initrd is loaded as one boot module, so only what startup and the
# common cases need is staged: Lua modules, filetype detection, core syntax scripts and a few languages.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
WORK="${1:-$HOME/bobliu-os-work/nvim-port}"
RT="$WORK/neovim/runtime"
DEST="$ROOT/initrd/share/nvim/runtime"
rm -rf "$DEST"; mkdir -p "$DEST/syntax" "$DEST/colors" "$DEST/plugin"
cp -r "$RT/lua" "$DEST/"
cp "$RT"/filetype.lua "$RT"/ftplugin.vim "$RT"/indent.vim "$RT"/ftoff.vim "$RT"/ftplugof.vim "$RT"/indoff.vim "$RT"/scripts.vim "$DEST/" 2>/dev/null || true
for s in syntax synload nosyntax c cpp lua vim sh python markdown make json text conf; do
  [ -f "$RT/syntax/$s.vim" ] && cp "$RT/syntax/$s.vim" "$DEST/syntax/"
done
cp "$RT"/colors/*.vim "$DEST/colors/" 2>/dev/null || true
echo "staged $(du -sh "$DEST" | cut -f1) into initrd/share/nvim/runtime"
