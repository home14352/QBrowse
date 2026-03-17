#!/usr/bin/env bash
set -euo pipefail
sudo pacman -S --needed base-devel cmake ninja pkgconf sqlite libarchive xdg-utils qt6-base qt6-svg qt6-networkauth qt6-webengine
