#!/usr/bin/env bash
# Porpoise - build the title: the Porpoise host, the Dolphin core and RADV.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Derived from Mihawk's PS5 RetroArch tools/build-title.sh (GPL-3.0-or-later),
# with RetroArch removed: src/ is Porpoise's own host plus the platform layer
# carried over from that project (core loader, threads, memory, crash report).
#
#   PS5_VULKAN_DIR         Mihawk's PS5_Vulkan checkout with RADV built (release); ../PS5_Vulkan
#   PS5_PAYLOAD_SDK_FORK   Mihawk's PS5_PayloadSDK checkout; ../PS5_PayloadSDK
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"

sdk="$root/.deps/native/ps5-payload-sdk"
bash "$root/tools/setup-native-dependencies.sh" >/dev/null
[[ -x $sdk/bin/prospero-lld ]] || { echo "error: no SDK at $sdk" >&2; exit 2; }
vulkan_dir="${PS5_VULKAN_DIR:-$root/../PS5_Vulkan}"

echo "==> [porpoise] step 1/3: the Dolphin core"
core_names=(dolphin)
bash "$root/tools/build-dolphin.sh"
core_files=("$root/build/cores/stage/cores/dolphin_libretro.so")
python3 "$root/tools/core-imports.py" "${core_files[@]}"

echo "==> [porpoise] step 2/3: the title"
radv_archive=${RADV_ARCHIVE:-$vulkan_dir/.deps/native/radv-release/lib/libvulkan_radeon.ps5.a}
[[ -f $radv_archive ]] || { echo "error: RADV is not built: $radv_archive" >&2; exit 2; }
source "$vulkan_dir/tools/radv-link.sh"
radv_link_recipe "$vulkan_dir" "$sdk" "$radv_archive" || exit 2
vulkan_flags="--no-dynamic-linker -z nodynamic-undefined-weak"
for flag in "${radv_link_flags[@]}"; do
    case $flag in
        --wrap=malloc | --wrap=calloc | --wrap=realloc | --wrap=free | --wrap=posix_memalign | \
        --wrap=aligned_alloc | --wrap=memalign | --wrap=malloc_usable_size | --wrap=reallocf | \
        --wrap=reallocarray | --wrap=getline | --wrap=getdelim) ;;
        *) vulkan_flags+=" $flag" ;;
    esac
done
vulkan_flags+=" ${radv_link_inputs[*]:5}"
linker_script="$vulkan_dir/tooling/psbc/ps5-pie-unwind.ld"

# The build identity: every input that decides what eboot.bin is.
python3 - "$root" "$radv_archive" <<'PY'
import hashlib, pathlib, sys
root = pathlib.Path(sys.argv[1])
inputs = sorted(p for p in (root / "src").rglob("*") if p.is_file())
inputs += [root / n for n in ("tools/build-porpoise.sh", "build/core_imports.inc",
                              ".deps/native/ps5-payload-sdk/.ps5-sdk-revision",
                              "build/cores/stage/cores/dolphin_libretro.so", "tools/build.sh")]
inputs.append(pathlib.Path(sys.argv[2]))
digest = hashlib.sha256()
for path in inputs:
    digest.update(path.name.encode() + b"\0")
    digest.update(hashlib.sha256(path.read_bytes()).digest())
identity = digest.hexdigest()
(root / "build/title_build_identity.h").write_text(
    '#define PS5_RETROARCH_BUILD_ID "Porpoise build identity: ' + identity + '"\n')
print("==> [porpoise] build identity: " + identity)
PY

directory_wrap_flags="--wrap=opendir --wrap=readdir --wrap=closedir --wrap=fdopendir --wrap=openat --wrap=unlinkat --wrap=fchmodat"
directory_wrap_flags+=" --wrap=realpath --wrap=getcwd --wrap=mkdir --wrap=open --wrap=fopen"

PS5_PAYLOAD_SDK="$sdk" \
PS5_CLANG=/usr/bin/clang \
PYTHONPATH="$root/tooling/pystub${PYTHONPATH:+:$PYTHONPATH}" \
APP_DEFINITIONS="PS5_RETROARCH_RADV PORPOISE" \
APP_INCLUDE_PATHS="third_party third_party/libretro third_party/vulkan build" \
APP_STATIC_ARCHIVES="" \
APP_SDK_ARCHIVES="libps5platform.a" \
APP_VULKAN_ARCHIVES="$radv_archive" \
APP_EXTRA_OBJECTS="" \
APP_LINK_FLAGS="$vulkan_flags --wrap=malloc --wrap=calloc --wrap=realloc --wrap=free $directory_wrap_flags" \
APP_LINKER_SCRIPT="$linker_script" \
    make app

title_id=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' \
    "$root/sce_sys/param.json")
dist="$root/dist/$title_id"
[[ -f $dist/eboot.bin ]] || { echo "error: no eboot.bin under $dist" >&2; exit 2; }

echo "==> [porpoise] step 3/3: staging"
cp -a -- "$root/config/LEGAL.txt" "$dist/LEGAL.txt"
mkdir -p "$dist/cores" "$dist/info" "$dist/system" "$dist/content" "$dist/porpoise"
cp -- "$root/build/cores/stage/cores/dolphin_libretro.so" "$dist/cores/"
cp -- "$root/build/cores/stage/info/dolphin_libretro.info" "$dist/info/"
cp -- "$root/build/cores/stage/info/dolphin_libretro.info" "$dist/cores/"
rm -rf -- "$dist/system/dolphin-emu"
cp -a -- "$root/build/cores/stage/system/dolphin-emu" "$dist/system/dolphin-emu"
cp -a -- "$root/config/options.ini" "$dist/porpoise/options.ini"

python3 "$root/tools/stage-notices.py" "$dist" --driver radv --vulkan-dir "$vulkan_dir" \
    --sdk-fork "${PS5_PAYLOAD_SDK_FORK:-$root/../PS5_PayloadSDK}"
bash "$root/tools/check-manifest.sh" --record
printf '==> [porpoise] built %s (%s files, eboot.bin %s bytes)\n' \
    "$dist" "$(find "$dist" -type f | wc -l)" "$(stat -c %s "$dist/eboot.bin")"
(cd "$root/dist" && rm -f "Porpoise-$title_id.zip" && zip -qr -X "Porpoise-$title_id.zip" "$title_id")
echo "==> [porpoise] packaged dist/Porpoise-$title_id.zip"
