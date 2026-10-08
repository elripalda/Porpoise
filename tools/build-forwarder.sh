#!/usr/bin/env bash
# Porpoise - builds the home screen forwarder (tools/forwarder/forwarder.cpp)
# into <out>/eboot.bin: a signed title executable Porpoise copies into each
# forwarder it makes. Runs after tools/build.sh (its host tool, C runtime
# objects and linker script).
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
out=${1:?usage: tools/build-forwarder.sh <output folder>}
sdk_root="$root/.deps/native/ps5-payload-sdk"
native="$root/tooling/native"
build="$root/build/forwarder"
tool="$root/build/host/ps5-native-tool"
[[ -x $tool && -f $root/build/obj/app_crt.o && -f $root/build/obj/app_cpp_runtime.o ]] || {
    echo "build-forwarder: run tools/build.sh first" >&2; exit 2; }
mkdir -p "$build" "$out"
PS5_PAYLOAD_SDK="$sdk_root" sh "$root/tooling/prospero-clang18" \
    -std=c++20 -O2 -Wall -Wextra -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
    -c "$root/tools/forwarder/forwarder.cpp" -o "$build/forwarder.o"
"$sdk_root/bin/prospero-lld" -T "$native/ps5-pie.ld" -L "$native" --eh-frame-hdr --error-limit=0 \
    --gc-sections --version-script "$native/app-symbols.map" --exclude-libs=ALL \
    -e _start -o "$build/forwarder-pie.elf" \
    "$root/build/obj/app_crt.o" "$root/build/obj/app_cpp_runtime.o" "$build/forwarder.o" \
    --as-needed "$sdk_root"/target/lib/*.so
"$tool" link --in "$build/forwarder-pie.elf" --out "$build/eboot.elf" \
    --stub-dir "$sdk_root/target/lib" --module-sdk 0x02000009 \
    --companion-sdk 0x08050001 --file-name eboot.elf
"$tool" self --sign --in "$build/eboot.elf" --out "$out/eboot.bin" --magic 0x1D3D154F
"$tool" self --inspect --file "$out/eboot.bin" >/dev/null
printf '==> [forwarder] %s (%s bytes)\n' "$out/eboot.bin" "$(stat -c %s "$out/eboot.bin")"
# The home launcher: a payload for the jailbreak's ELF loader (an app can't
# start another app), built as the notification relay is.
PS5_PAYLOAD_SDK="$sdk_root" "$sdk_root/bin/prospero-clang" -Wall -Werror -O2 \
    -o "$build/home-launcher.elf" "$root/tools/home-launcher/home_launcher.c" -lSceSystemService -lSceUserService
PS5_PAYLOAD_SDK="$sdk_root" "$sdk_root/bin/prospero-strip" "$build/home-launcher.elf"
cp "$build/home-launcher.elf" "$out/home-launcher.elf"
printf '==> [forwarder] %s (%s bytes)\n' "$out/home-launcher.elf" "$(stat -c %s "$out/home-launcher.elf")"
