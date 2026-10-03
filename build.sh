#!/bin/zsh
set -e

mkdir -p build

emcc main.c -o build/index.html \
  -sUSE_SDL=2 \
  -sUSE_SDL_TTF=2 \
  -sUSE_SDL_MIXER=2 \
  -sALLOW_MEMORY_GROWTH=1 \
  --preload-file assets@/assets \
  -O2

echo ""
echo "Готово: build/index.html"
echo "Локально: cd build && python3 -m http.server 8000"
