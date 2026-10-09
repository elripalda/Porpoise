#!/usr/bin/env bash
# Porpoise for Windows: the launcher and the platform layer (desktop/), cross-
# compiled with llvm-mingw, beside SDL 3 and the Dolphin core built for
# Windows (tools/build-windows-core.sh). Output: dist/windows/Porpoise and
# dist/Windows-Porpoise-<version>.zip.
#
#   MINGW=<llvm-mingw dir> SDL3=<SDL3-x.y.z/x86_64-w64-mingw32> CORE=<dolphin_libretro.dll>
#   bash tools/build-windows.sh
#
# Copyright (C) 2026 Ruben (Project Porpoise)
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
MINGW=${MINGW:-$HOME/win/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64}
SDL3=${SDL3:-$HOME/win/SDL3-3.4.18/x86_64-w64-mingw32}
CORE=${CORE:-$HOME/win/core-build/dolphin_libretro.dll}
JOBS=${JOBS:-$(nproc)}
cxx="$MINGW/bin/x86_64-w64-mingw32-clang++"
cc="$MINGW/bin/x86_64-w64-mingw32-clang"
obj="$root/build/windows/obj"
out="$root/dist/windows/Porpoise"
mkdir -p "$obj" "$out"

# The PS5's own pieces stay out: the console's loader, memory, threads,
# sampler and crash report, its HTTP and jailbreak (desktop/ has their stand-ins).
ps5_only="core_imports_ps5 core_loader_ps5 core_threads_ps5 crash_report memory_diagnostics memory_ps5 memory_status
permissions_ps5 sampler_ps5 thread_probe porpoise_jailbreak porpoise_http vulkan_trace libc_shims locale_shims
overflow_heap platform_wraps present_clock_ps5vk radv_icd_ps5 porpoise_notify porpoise_netfs"
sources=()
for f in "$root"/src/*.cpp "$root"/src/*.c; do
    base=$(basename "${f%.*}")
    [[ " $(echo $ps5_only) " == *" $base "* ]] && continue
    sources+=("$f")
done
sources+=("$root/desktop/porpoise_platform.cpp" "$root/desktop/porpoise_http_desktop.cpp" "$root/desktop/porpoise_notify_desktop.cpp"
          "$root/desktop/porpoise_netfs_desktop.cpp")

[[ -f $root/build/title_build_identity.h ]] ||
    echo '#define PS5_RETROARCH_BUILD_ID "Porpoise build identity: windows"' > "$root/build/title_build_identity.h"

common=(-DPORPOISE_DESKTOP -DPORPOISE -D_USE_MATH_DEFINES -DNOMINMAX -O2 -g0
    -include "$root/desktop/desktop_compat.h"
    -I"$root/src" -I"$root/desktop" -I"$root/desktop/include" -I"$root/third_party" -I"$root/third_party/libretro"
    -I"$root/third_party/vulkan" -I"$root/third_party/stb" -I"$root/build" -I"$SDL3/include"
    -w)
echo "==> [windows] compiling ${#sources[@]} files"
objects=()
pids=()
failed=0
for f in "${sources[@]}"; do
    o="$obj/$(basename "$f").o"
    objects+=("$o")
    if [[ ! -f $o || $f -nt $o || $root/desktop/desktop_compat.h -nt $o ]]; then
        if [[ $f == *.c ]]; then
            "$cc" -std=c11 "${common[@]}" -c "$f" -o "$o" &
        else
            "$cxx" -std=c++20 "${common[@]}" -c "$f" -o "$o" &
        fi
        pids+=($!)
        if (( ${#pids[@]} >= JOBS )); then
            wait "${pids[0]}" || failed=1
            pids=("${pids[@]:1}")
        fi
    fi
done
for p in "${pids[@]}"; do wait "$p" || failed=1; done
(( failed == 0 )) || { echo "error: compiling failed" >&2; exit 1; }

"$MINGW/bin/x86_64-w64-mingw32-windres" -I"$root/desktop" "$root/desktop/porpoise.rc" -O coff -o "$obj/porpoise.res.o"
objects+=("$obj/porpoise.res.o")
echo "==> [windows] linking Porpoise.exe"
"$cxx" -o "$out/Porpoise.exe" "${objects[@]}" "$SDL3/lib/libSDL3.dll.a" -lwinhttp -lws2_32 -lpthread -mwindows \
    -static -Wl,--subsystem,windows

echo "==> [windows] staging"
cp -f "$SDL3/bin/SDL3.dll" "$out/"
mkdir -p "$out/cores" "$out/assets" "$out/system" "$out/data"
[[ -f $CORE ]] && cp -f "$CORE" "$out/cores/dolphin_libretro.dll"
# Both Porpoise.exe and the core carry their C++ runtime; only Windows' own
# UCRT (built into Windows 10 and 11) and SDL3.dll are needed beside them.
rm -f "$out/libc++.dll" "$out/libunwind.dll" "$out/libwinpthread-1.dll"
rsync -a --delete "$root/assets/" "$out/assets/"
if [[ -d $root/build/cores/stage/system ]]; then
    rsync -a --delete "$root/build/cores/stage/system/" "$out/system/"
fi
# Licenses: Porpoise's, Dolphin's, SDL's and the compiler runtime's.
mkdir -p "$out/licenses"
cp -f "$root/LICENSE" "$out/licenses/Porpoise-GPL-3.0.txt"
cp -f "$root/config/LEGAL.txt" "$out/LEGAL.txt"
[[ -f $root/.deps/dolphin-src/COPYING ]] && cp -f "$root/.deps/dolphin-src/COPYING" "$out/licenses/Dolphin-GPL-2.0.txt"
cp -f "$(dirname "$SDL3")/LICENSE.txt" "$out/licenses/SDL3-zlib.txt"
cp -f "$MINGW/LICENSE.TXT" "$out/licenses/llvm-mingw-runtime.txt"
cp -f "$root/desktop/README-Windows.txt" "$out/README.txt"

version=${PORPOISE_WINDOWS_VERSION:-2.0-test}
zip="$root/dist/Windows-Porpoise-$version.zip"
rm -f "$zip"
(cd "$root/dist/windows" && zip -qr -9 "$zip" Porpoise -x 'Porpoise/data/*' 'Porpoise/trace.txt' 'Porpoise/porpoise/*')
echo "==> [windows] $out"
echo "==> [windows] $zip ($(stat -c %s "$zip") bytes)"
