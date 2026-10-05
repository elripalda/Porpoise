# Building Porpoise

Porpoise cross-compiles on **Linux** (or WSL2) into a PS5 title folder,
`dist/PPSA99764/`, and a zip of it. You don't need a console to build, but you
do need one to run the result.

## What you need

- **clang 18** (`clang`, `clang++`, `lld`, `llvm-ar`), **cmake** 3.20+,
  **make**, **python3** (with Pillow), **git**, **wget**, **curl**, **unzip** and
  **zip**. On Ubuntu 24.04:

  ```sh
  sudo apt install clang-18 lld-18 llvm-18 cmake make python3 python3-pil git wget curl unzip zip
  ```

- Three sibling checkouts next to this repository:

  ```
  work/
  ├── Porpoise/          this repository
  ├── PS5_PayloadSDK/    https://github.com/mihawk-99/PS5_PayloadSDK
  └── PS5_Vulkan/        https://github.com/mihawk-99/PS5_Vulkan  (with PS5_Mesa beside it)
  ```

  - **PS5_PayloadSDK** is the PS5 payload SDK fork. Porpoise pins its revision
    in `tools/setup-native-dependencies.sh` and installs it into `.deps/native/`
    on the first build. Point `PS5_PAYLOAD_SDK_FORK` at it if it lives elsewhere.
  - **PS5_Vulkan** provides Vulkan on PS5 through Mesa's RADV. Build the release
    driver once, following its README:

    ```sh
    cd ../PS5_Vulkan && tools/build-radv.sh release
    ```

    That produces `.deps/native/radv-release/lib/libvulkan_radeon.ps5.a`. Point
    `PS5_VULKAN_DIR` at the checkout if it isn't `../PS5_Vulkan`.

## Build

```sh
make            # or: bash tools/build-porpoise.sh
```

The build runs in three steps:

1. **The Dolphin core.** `tools/build-dolphin.sh` clones
   [libretro/dolphin](https://github.com/libretro/dolphin) at the pinned revision
   (with its submodules, a large download the first time), applies
   [`patches/dolphin/ps5-port.patch`](patches/dolphin/ps5-port.patch) and
   cross-builds `dolphin_libretro.so`. A stamp skips this step until its inputs
   change; `PS5_FORCE_CORES=1` rebuilds it anyway.
2. **The title.** `make app` compiles `src/`, links it with RADV and the SDK,
   and turns it into a signed `eboot.bin` with the generated `libc.prx`.
3. **Staging.** The core, Dolphin's `Sys` data, the assets, the license notices
   (`licenses/`) and a file manifest are staged into `dist/PPSA99764/`, which is
   then zipped to `dist/Porpoise-PPSA99764.zip`.

The notices in `licenses/` record the source revision of every part. Build from
a clean, committed tree so they point at a revision people can find.
For a release, set `PORPOISE_RELEASE_TAG` (for example `PORPOISE_RELEASE_TAG=v1.0 make`)
so the tag is written into `licenses/README.txt` as well.

## Working on Porpoise

| Path | What |
|---|---|
| `src/porpoise_main.cpp` | Startup and the launcher loop |
| `src/porpoise_core.*` | The libretro host: loads Dolphin, runs frames, audio pacing, the pause loop |
| `src/porpoise_vk.*`, `src/porpoise_audio.*`, `src/porpoise_pad.*` | Video, sound and controller |
| `src/porpoise_covers.*`, `src/porpoise_gametdb.*` | Box art and game info from GameTDB |
| `src/porpoise_sound.*` | Menu music and sound effects |
| `src/ui_*.cpp` | The launcher UI: library, details, memory cards, settings, in-game menu, translations |
| `shaders/` | The UI's glass and text shaders |
| `assets/` | Fonts, logo, music and sounds (copied into the app as they are) |
| `sce_sys/` | Title ID, name, icon and backgrounds |
| `patches/dolphin/` | The PS5 port of Dolphin, as one patch |
| `tools/ui-preview/` | Renders the launcher's screens on a desktop with Vulkan, for design work |

To change the Dolphin port, edit `.deps/dolphin-src`, build with
`DOLPHIN_DEV=1 bash tools/build-dolphin.sh`, then write the patch back:

```sh
git -C .deps/dolphin-src diff > patches/dolphin/ps5-port.patch
```

## Installing your build

Copy `dist/PPSA99764/` to `/data/homebrew/PPSA99764/` on the console. If an older
copy is registered, delete it first (see [Updating](README.md#updating)).
