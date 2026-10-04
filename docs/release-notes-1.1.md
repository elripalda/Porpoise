<p align="center"><img src="https://github.com/elripalda/Porpoise/raw/main/docs/images/banner.png" alt="Porpoise" width="100%"></p>

**Porpoise 1.1, the quality-of-life update.** Save states, fast forward, a full in-game menu, your own button layouts, screen filters and borders, favourites and play time, recommended settings per game, and two new languages.

## What's new

- **A real in-game menu.** Options + touch pad, then **L1 / R1** between four tabs: **Game**, **Video**, **Graphics** and **Controls**. Nearly every setting is there, takes effect at once, and is saved for that game.
- **Save states.** Three slots per game, each with a picture of the moment. Save and load from the in-game menu, or start a game straight from a slot on its Details page.
- **Fast forward.** 2x or 4x from the in-game menu.
- **Your own button layouts.** Four of them (*My layout 1–4*) beside GameCube and PlayStation. Any game can use any layout, and *Customize buttons* also works over a paused game.
- **New controller art and button icons everywhere**, from Zacksly's *PS5 Button Icons and Controls*. The Controls tab shows the DualSense with a line to every button and the GameCube button it plays.
- **Screen filters:** Smooth, Sharp, Sharpen, **CRT**, **Arcade CRT** and **VHS**, each with a strength.
- **Borders** for 4:3 games (widescreen off): *Porpoise glass*, *Midnight* and an *Arcade cabinet* with a curved screen. Make your own: a 1920×1080 PNG in `/data/porpoise/borders/`.
- **Favourites and play time.** Options stars a game. Sort by *Most played* or *Favourites first*.
- **Recommended settings.** Each game's settings list Dolphin's own fixes for it and Porpoise's tested picks, which you can apply with one press.
- **DualSense light bar** in each player's colour.
- **Update notice** when a newer Porpoise is out, and a **QR code** in About for reporting bugs.

## Languages

English, Español, Français, Português, and new in 1.1: **Italiano** and **日本語**. Each has its flag in *Settings → Interface → Language*. Game descriptions now come in Spanish, French, Portuguese and Italian where GameTDB has them.

## Install or update

1. Download **`Porpoise-1.1.zip`** below and unzip it. You get a folder named **`PPSA99764`**.
2. **Updating from 1.0:** delete `/data/homebrew/PPSA99764/` on the PS5 first. Your games, saves, covers and settings live in `/data/porpoise/` and stay put. Custom buttons from 1.0 become *My layout 1*.
3. Copy the **`PPSA99764`** folder into **`/data/homebrew/`** over FTP (for example etaHEN's FTP server) or PS5 Upload.
4. New install: register it like your other homebrew (for example with ShadowMountPlus).

You need a jailbroken PS5 with **etaHEN** and **kstuff** loaded. Full guide: [README](https://github.com/elripalda/Porpoise#readme) · [Changelog](https://github.com/elripalda/Porpoise/blob/main/CHANGELOG.md) · [Credits](https://github.com/elripalda/Porpoise/blob/main/CREDITS.md)

## Thanks

Controller art and button icons: *PS5 Button Icons and Controls* by **[Zacksly](https://zacksly.itch.io)** ([@_Zacksly](https://twitter.com/_Zacksly)), licensed under CC BY 3.0 and adapted for Porpoise (rendered to PNG, PlayStation logo and labels removed).

## Legal

Porpoise contains **no games, no BIOS or firmware files and no keys**. Play only games you own, from backups you made yourself. Porpoise is not affiliated with Nintendo or Sony. Emulation is provided by [Dolphin](https://dolphin-emu.org) (GPL-2.0-or-later). Porpoise is GPL-3.0-or-later; every component's licence and source revision is listed in `licenses/README.txt` inside the zip.

`Porpoise-1.1.zip` SHA-256: `<filled in at release>`
