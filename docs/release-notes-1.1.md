<p align="center"><img src="https://github.com/elripalda/Porpoise/raw/main/docs/images/banner.png" alt="Porpoise" width="100%"></p>

**Porpoise 1.1, the quality-of-life update.** Recommended settings you can switch on per game, save states, setups, fast forward, a full in-game menu, your own button layouts, nine screen filters and eight borders, favourites and play time, twelve languages, and updates from inside Porpoise.

## What's new

- **Recommended settings you can switch on.** Supported games come with recommended settings: each game's settings open with Dolphin's own fixes for it and Porpoise's picks tested on PS5, each with an on/off switch (green is on). The first pick is for *WWE Day of Reckoning 2*, which ran at about 6 fps: it keeps EFB copies and texture decoding on the GPU. A game's Dolphin settings now also stay in force when you change graphics in the in-game menu.
- **A real in-game menu.** Options + touch pad, then **L1 / R1** between four tabs: **Game**, **Video**, **Graphics** and **Controls**. Nearly every setting is there, takes effect at once, and is saved for that game.
- **Save states.** Three slots per game, each with a picture of the moment that fits the game's real shape (4:3 or 16:9). Pick a slot to save or load; saving over a used slot asks first, and a spinner shows while it's written. *Fast save states* keeps Dolphin's texture cache out of them, so saving takes a moment. Start a game straight from a slot on its Details page.
- **Setups.** Save a game's Video and Graphics settings as a setup and use it on any other game.
- **Fast forward.** 2x or 4x from the in-game menu.
- **Your own button layouts.** Four of them (*My layout 1–4*) beside GameCube and PlayStation. Any game can use any layout, and *Customize buttons* also works over a paused game.
- **New controller art and button icons everywhere**, from Zacksly's *PS5 Button Icons and Controls*. The Controls tab shows the DualSense with a line to every button and the GameCube button it plays.
- **Nine screen filters:** Smooth, Sharp, Sharpen, CRT, Arcade CRT, VHS, and new **Soft VHS**, **8-bit** and **Pocket**, each with a strength.
- **Eight borders** for 4:3 games (widescreen off): **Porpoise**, Porpoise glass, Midnight, **Frost**, **Carbon**, and three cabinets — Arcade, **Retro** and **Synthwave**. Make your own: a 1920×1080 PNG in `/data/porpoise/borders/`.
- **Updates from inside Porpoise:** *Settings → About → Updates* checks GitHub and installs a new Porpoise, checking every file before anything is replaced.
- **Favourites and play time.** Options stars a game. Sort by *Most played* or *Favourites first*.
- **DualSense light bar** in each player's colour, and a **QR code** in About for reporting bugs.
- **Fixes:** Porpoise asks the HEN to leave the app sandbox when it starts inside it — the cause of "only a few folders in the folder browser", no `/data/porpoise` folder and games in `/data/games` not showing. Shaders compile on four threads instead of one, for less stutter.

## Languages

**Twelve languages**, each with its flag in *Settings → Interface → Language*: English, Español (España), **Español (Latinoamérica)**, Français, **Deutsch**, **Italiano**, **Nederlands**, **Polski**, Português (Portugal), **Português (Brasil)**, **Русский** and **日本語**. Game descriptions come in Spanish, French, Portuguese, Italian, German and Dutch where GameTDB has them. The translations are first drafts; corrections are welcome (see the README).

## Install or update

1. Download **`Porpoise-1.1.zip`** below and unzip it. You get a folder named **`PPSA99764`**.
2. **Updating from 1.0:** delete `/data/homebrew/PPSA99764/` on the PS5 first. Your games, saves, covers and settings live in `/data/porpoise/` and stay put. Custom buttons from 1.0 become *My layout 1*. *If your 1.0 couldn't reach `/data`* (your games were in the app's own `porpoise/games` folder), copy the new files over the old folder instead of deleting it; 1.1 moves your things to `/data/porpoise` itself.
3. Copy the **`PPSA99764`** folder into **`/data/homebrew/`** over FTP (for example etaHEN's FTP server) or PS5 Upload.
4. New install: register it like your other homebrew (for example with ShadowMountPlus). If Porpoise says it can't reach `/data`, add **PPSA99764** to your HEN's list of apps to jailbreak.

From 1.1 on, *Settings → About → Updates* updates Porpoise for you.

You need a jailbroken PS5 with **etaHEN** and **kstuff** loaded. Full guide: [README](https://github.com/elripalda/Porpoise#readme) · [Changelog](https://github.com/elripalda/Porpoise/blob/main/CHANGELOG.md) · [Credits](https://github.com/elripalda/Porpoise/blob/main/CREDITS.md)

## Thanks

The Dolphin core on PS5: **[Mihawk (mihawk-99)](https://github.com/mihawk-99)**, whose PS5 RetroArch, PS5_Vulkan and PS5_Mesa ports Porpoise is built on.

Controller art and button icons: *PS5 Button Icons and Controls* by **[Zacksly](https://zacksly.itch.io)** ([@_Zacksly](https://twitter.com/_Zacksly)), licensed under CC BY 3.0 and adapted for Porpoise (rendered to PNG, PlayStation logo and labels removed).

## Legal

Porpoise contains **no games, no BIOS or firmware files and no keys**. Play only games you own, from backups you made yourself. Porpoise is not affiliated with Nintendo or Sony. Emulation is provided by [Dolphin](https://dolphin-emu.org) (GPL-2.0-or-later). Porpoise is GPL-3.0-or-later; every component's licence and source revision is listed in `licenses/README.txt` inside the zip.

`Porpoise-1.1.zip` SHA-256: `<filled in at release>`
