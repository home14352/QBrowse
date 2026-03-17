# Сборка на Arch Linux

## Зависимости

```bash
sudo pacman -S --needed base-devel cmake ninja pkgconf sqlite libarchive xdg-utils qt6-base qt6-svg qt6-networkauth qt6-webengine
```

## Сборка

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/qbrowse
```

## Установка в систему/пользователю

```bash
cmake --install build --prefix "$HOME/.local"
update-desktop-database "$HOME/.local/share/applications" || true
```

или:

```bash
./scripts/install-system.sh
```
