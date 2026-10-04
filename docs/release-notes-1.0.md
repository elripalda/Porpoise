<p align="center"><img src="https://github.com/elripalda/Porpoise/raw/main/docs/images/banner.png" alt="Porpoise" width="100%"></p>

**The first release of Porpoise:** a native GameCube and Wii emulator for jailbroken PS5, powered by Dolphin, with a launcher made for the TV and the DualSense.

## Highlights

- **Cover-flow library** of glass box-art tiles that finds your games on its own: the usual folders, USB and extended drives, plus any folder you add. Plays `.iso`, `.gcm`, `.rvz`, `.ciso`, `.gcz`, `.wbfs` and `.wia`.
- **Automatic box art** from GameTDB: front covers, **back covers** and disc label art, along with game details (description, developer, publisher, release date, genre, players and rating).
- **Details page:** Square flies the box in, Triangle flips to the back cover, the right stick turns the box, **L2 / R2 swipe between games**, and the disc spins alongside.
- **Up to four players**, one per signed-in PS5 user. Controllers can join at any time, even mid-game.
- **Button layouts:** GameCube (the default), PlayStation (Cross A, Circle B), or **Custom**, set on a picture of the DualSense in *Customize buttons*. Any game can have its own layout.
- **Memory cards:** Slot A and Slot B with save icons and banners. Copy and delete saves, each with a confirmation.
- **Settings for everything:** video, graphics, audio, controls, system, games and interface, plus **per-game settings**, each with a reset.
- **In-game menu** (Options + touch pad): resume, quick video and volume settings, Quit to library, Close Porpoise.
- **Smooth play:** frames locked to your TV's refresh, stutter-free asynchronous ubershaders, clean 48 kHz sound, never above 60 fps.
- **Original menu music and sound effects** by @elripalda, each with its own switch and volume.
- **English, Español, Français, Português.** Porpoise follows your PS5's language. More languages are coming in a future update.

## Install

You need a jailbroken PS5 with **etaHEN** and **kstuff** loaded, and a way to register homebrew (for example **ShadowMountPlus**).

1. Download **`Porpoise-1.0.zip`** below and unzip it on your computer. You get a folder named **`PPSA99764`**.
2. Connect to your PS5 with FTP (for example etaHEN's FTP server and a client like FileZilla) or with PS5 Upload.
3. Copy the whole **`PPSA99764`** folder into **`/data/homebrew/`**, so you end up with `/data/homebrew/PPSA99764/`.
4. Register it the same way as your other homebrew (for example with ShadowMountPlus). The Porpoise tile appears on your home screen.
5. Copy your own game backups into **`/data/porpoise/games/`** (Porpoise creates it on first launch), or onto a USB drive, and open Porpoise.

Box art and game details download automatically while the console is online.

**Updating later:** delete `/data/homebrew/PPSA99764/` first, then upload the new folder. Your games, saves, covers and settings live in `/data/porpoise/` and stay put.

Full guide: [README](https://github.com/elripalda/Porpoise#readme) · Build from source: [BUILDING.md](https://github.com/elripalda/Porpoise/blob/main/BUILDING.md) · [Credits](https://github.com/elripalda/Porpoise/blob/main/CREDITS.md)

## Legal

Porpoise contains **no games, no BIOS or firmware files and no keys**. Play only games you own, from backups you made yourself. Porpoise is not affiliated with Nintendo or Sony. Emulation is provided by [Dolphin](https://dolphin-emu.org) (GPL-2.0-or-later). Porpoise is GPL-3.0-or-later; every component's licence and source revision is listed in `licenses/README.txt` inside the zip.

`Porpoise-1.0.zip` SHA-256: `92282136b2c6953722e7736be28b5e2b5a7e6b89e6d717e0f51424f1e664358e`
