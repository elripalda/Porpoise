<p align="center"><img src="https://github.com/elripalda/Porpoise/raw/main/docs/images/banner.png" alt="Porpoise" width="100%"></p>

**Porpoise 1.5.1.** Fixes for "Porpoise can't reach /data", and games on USB and external drives found more easily.

## What's fixed

### Getting out of the sandbox
If Porpoise starts inside the app sandbox, it can't see `/data` or any USB drive. This update makes getting out of it much more dependable:
- **Keeps asking.** Porpoise now asks the jailbreak daemon again for a few seconds instead of once. This targets the "worked the first time, then couldn't read /data" reports, where the daemon wasn't ready yet or the first try lost a timing race.
- **Works with more daemons:** etaHEN, OnionHEN, and both the classic and the newer **Lapy** daemon.
- **Clear instructions on screen** when it still can't get out.

**What to set up, depending on what you run:**
- **etaHEN:** turn on **Legacy Command Server** in etaHEN's Toolbox settings. etaHEN's own app list is built in and can't be edited to add Porpoise, so adding PPSA99764 to a whitelist won't help.
- **OnionHEN:** add `PPSA99764` to `exact_title_ids` under `[app_jailbreak]` in `/data/OnionHEN/config.ini`, then restart the console or reload OnionHEN.
- **Lapy daemon:** load it before opening Porpoise.

If a launch still lands in the sandbox, open Porpoise once more.

### Games on USB and external drives
- USB drives, PS5 extended storage and the usual game folders are now searched **four folders deep**, so games sorted into subfolders (like `games/Wii/...`) are found without adding the folder by hand.
- **How to play from a drive:**
  1. Format it **exFAT** (FAT32 can't hold files over 4 GB; the PS5 doesn't read NTFS).
  2. Copy your games onto it, up to four folders deep.
  3. Plug it into the PS5, make sure your jailbreak daemon is set up as above, and open Porpoise.
  4. Plugged it in after opening Porpoise? Use **Settings → Games → Search for games now**.

  Full steps, including PS5 extended storage, are in the [README](https://github.com/elripalda/Porpoise#playing-from-a-usb-or-external-drive).

## Install or update

**From 1.1 or 1.5:** open **Settings → About → Updates** and install 1.5.1 from there.

**By hand or new install:**
1. Download **`Porpoise-1.5.1.zip`** below and unzip it. You get a folder named **`PPSA99764`**.
2. If you're updating, delete `/data/homebrew/PPSA99764/` on the PS5 first. Your games, saves, covers and settings live in `/data/porpoise/` and are kept.
3. Copy the **`PPSA99764`** folder into **`/data/homebrew/`** over FTP or with PS5 Upload.
4. For a new install, register it like your other homebrew (for example with ShadowMountPlus).

Still stuck? [Open an issue](https://github.com/elripalda/Porpoise/issues) with your firmware, which HEN or daemon you run, and `trace.txt` from `/data/homebrew/PPSA99764/`.

## Legal

Porpoise contains **no games and no BIOS or firmware files**. The only key it carries is the Wii disc key Dolphin itself includes, used to read a Wii disc's own banner. Play only games you own, from backups you made yourself. Porpoise is not affiliated with Nintendo or Sony. Emulation is provided by [Dolphin](https://dolphin-emu.org) (GPL-2.0-or-later). Porpoise is GPL-3.0-or-later; every component's licence and source revision is listed in `licenses/README.txt` inside the zip.

`Porpoise-1.5.1.zip` SHA-256: `6178e4573fbe89c60fd8b8f0d285d655e7d09cbf3687d6f84e47cfc7d8e0cf75`
