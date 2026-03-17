#!/usr/bin/env bash
set -euo pipefail
prefix="${1:-$HOME/.local}"
cmake --install build --prefix "$prefix"
update-desktop-database "$prefix/share/applications" 2>/dev/null || true
xdg-icon-resource forceupdate 2>/dev/null || true
