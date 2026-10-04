<p align="center"><img src="https://github.com/elripalda/Porpoise/raw/main/docs/images/banner.png" alt="Porpoise" width="100%"></p>

**Porpoise 1.1 (build 14).** This update adds save states, an in-game menu, recommended settings per game (beta), more filters and borders, twelve languages, a reworked folder browser and updates from inside Porpoise.

## Changes

### Games and folders
- **Folder browser:** *Settings → Games → Add a game folder* opens on the whole console at `/data`. Folders show how many games they hold, and game files are listed under them. **Square** adds the folder you're in (add as many as you like), **Triangle** lists your drives and Porpoise's games folder, and **Cross** on a game on a USB drive copies it to the console.
- **Sandbox fix:** if the console starts Porpoise inside the app sandbox (only a few folders visible, no `/data/porpoise`, games not found), Porpoise asks the HEN to let it out. With etaHEN 2.4B or later, add **PPSA99764** to etaHEN's app jailbreak list (or turn on *Legacy CMD Server*) for this to work.

### In-game menu (Options + touch pad)
- Four tabs, switched with L1 / R1: **Game**, **Video**, **Graphics** and **Controls**. Changes apply at once and are saved for that game.
- **Save states:** three slots per game, with pictures. Saving over a used slot asks first, and a spinner shows while the state is written. *Fast save states* (on by default) makes saving take a moment instead of several seconds. You can also start a game from a slot on its details page.
- **Setups:** save a game's Video and Graphics settings and use them on other games.
- **Fast forward:** 2x or 4x.

### Game settings
- **Recommended (beta):** a game's settings start with Dolphin's own fixes for it and Porpoise's picks tested on PS5, each with an on/off switch. Only a few games have picks so far; more will be added as games are tested. The first is for *WWE Day of Reckoning 2*.
- A game's Dolphin settings now stay in place when you change graphics from the in-game menu.

### Controls
- **PlayStation is now the default layout** (Cross A, Circle B). The **GameCube** layout matches Dolphin's (Circle A, Cross B). Four layouts of your own (*My layout 1–4*); 1.0's custom buttons become *My layout 1*.
- New controller art and button icons from Zacksly's *PS5 Button Icons and Controls*.
- The DualSense light bar shows each player's colour.

### Picture
- Screen filters: Smooth, Sharp, Sharpen, CRT, Arcade CRT, VHS, Soft VHS, 8-bit and Pocket, each with a strength.
- Borders for 4:3 games: Porpoise, Porpoise glass, Midnight, Frost, Carbon, and Arcade, Retro and Synthwave cabinets. You can add your own 1920×1080 PNGs in `/data/porpoise/borders/`.

### Languages
English, Español (España), **Español (Latinoamérica)**, Français, **Deutsch**, **Italiano**, **Nederlands**, **Polski**, Português (Portugal), **Português (Brasil)**, **Русский** and **日本語**, each with its flag. The new translations are first drafts; corrections are welcome (see the README).

### Library and other changes
- Favourites (Options) and play time, with *Most played* and *Favourites first* sorts.
- **Updates:** *Settings → About → Updates* checks GitHub and installs a newer Porpoise. Every file is checked before anything is replaced.
- Shaders compile on four threads instead of one, for less stutter.
- A QR code in About for reporting bugs.
- About now credits **Mihawk (mihawk-99)**, who brought the Dolphin core to the PS5.

## Install or update

1. Download **`Porpoise-1.1.zip`** below and unzip it. You get a folder named **`PPSA99764`**.
2. **From 1.0:** delete `/data/homebrew/PPSA99764/` on the PS5 first. Your games, saves, covers and settings live in `/data/porpoise/` and are kept. If your 1.0 couldn't reach `/data` (your games were in the app's own `porpoise/games` folder), copy the new files over the old folder instead of deleting it.
3. Copy the **`PPSA99764`** folder into **`/data/homebrew/`** over FTP or with PS5 Upload.
4. New install: register it like your other homebrew (for example with ShadowMountPlus).

From 1.1 on, *Settings → About → Updates* can update Porpoise for you.

You need a jailbroken PS5 with **etaHEN** and **kstuff**. More in the [README](https://github.com/elripalda/Porpoise#readme), the [changelog](https://github.com/elripalda/Porpoise/blob/main/CHANGELOG.md) and the [credits](https://github.com/elripalda/Porpoise/blob/main/CREDITS.md).

## Thanks

**[Mihawk (mihawk-99)](https://github.com/mihawk-99)**, whose PS5 RetroArch, PS5_Vulkan and PS5_Mesa ports Porpoise is built on. Controller art and button icons: *PS5 Button Icons and Controls* by **[Zacksly](https://zacksly.itch.io)** ([@_Zacksly](https://twitter.com/_Zacksly)), CC BY 3.0, adapted for Porpoise.

## Legal

Porpoise contains **no games, no BIOS or firmware files and no keys**. Play only games you own, from backups you made yourself. Porpoise is not affiliated with Nintendo or Sony. Emulation is provided by [Dolphin](https://dolphin-emu.org) (GPL-2.0-or-later). Porpoise is GPL-3.0-or-later; every component's licence and source revision is listed in `licenses/README.txt` inside the zip.

`Porpoise-1.1.zip` SHA-256: `<filled in at release>`
