#!/usr/bin/env bash
# The Dolphin libretro core for Porpoise on Windows: the same patched tree as
# the PS5's (.deps/dolphin-src with patches/dolphin/ps5-port.patch), cross-
# compiled with llvm-mingw. Output: $OUT/Binaries/dolphin_libretro.dll.
#
#   MINGW=<llvm-mingw dir> bash tools/build-windows-core.sh
#
# Notes for clang + libc++ on MinGW (libretro's own CI uses GCC/MXE):
# - a few Dolphin headers lean on GCC's transitive includes, so <vector>,
#   <string>, <cstdint> and <algorithm> are included up front;
# - Dolphin builds some libraries with C++ exceptions and the rest without,
#   and libc++ tags symbols differently for each; _LIBCPP_NO_ABI_TAG keeps one
#   name for both.
# Copyright (C) 2026 Ruben (Project Porpoise)
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
MINGW=${MINGW:-$HOME/win/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64}
OUT=${OUT:-$HOME/win/core-build}
JOBS=${JOBS:-$(nproc)}
toolchain="$OUT.toolchain.cmake"
mkdir -p "$OUT"
cat > "$toolchain" <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER $MINGW/bin/x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER $MINGW/bin/x86_64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER $MINGW/bin/x86_64-w64-mingw32-windres)
set(CMAKE_AR $MINGW/bin/llvm-ar)
set(CMAKE_RANLIB $MINGW/bin/llvm-ranlib)
set(CMAKE_FIND_ROOT_PATH $MINGW/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF
export GIT_CEILING_DIRECTORIES=$(dirname "$OUT")
cmake -S "$root/.deps/dolphin-src" -B "$OUT" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-include vector -include string -include cstdint -include algorithm -D_LIBCPP_NO_ABI_TAG" \
    -DLIBRETRO=ON -DENABLE_X11=OFF -DENABLE_EGL=OFF -DUSE_SYSTEM_LIBS=OFF \
    -DUSE_MGBA=OFF -DENABLE_LLVM=OFF -DENCODE_FRAMEDUMPS=OFF -DUSE_UPNP=OFF \
    -DENABLE_SDL=OFF -DENABLE_CLI_TOOL=OFF -DENABLE_TESTS=OFF -DENABLE_QT=OFF \
    -DENABLE_AUTOUPDATE=OFF -DENABLE_ANALYTICS=OFF -DUSE_DISCORD_PRESENCE=OFF \
    -DUSE_RETRO_ACHIEVEMENTS=OFF > "$OUT/configure.log"
cmake --build "$OUT" --target dolphin_libretro --parallel "$JOBS"
echo "==> [windows core] $OUT/Binaries/dolphin_libretro.dll"
