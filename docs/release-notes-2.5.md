<p align="center"><img src="https://github.com/elripalda/Porpoise-Dolphin-Emulator-for-PS5/raw/Main/docs/images/banner.png" alt="Porpoise" width="100%"></p>

**Porpoise 2.5** brings the new sound from 2.5 Beta 1 to everyone, plus a few new things:

- **Smoother sound:** when a game runs below full speed, its sound comes through Dolphin's own mixer, as on a PC, instead of crackling. **Sound presets** in **Settings → Audio**, and an **Audio** tab in the in-game menu. *Classic (2.1)* brings back the old sound.
- **GameCube controller for Wii games** that take one, such as Super Smash Bros. Brawl and Mario Kart Wii.
- **Graphics mods:** Dolphin's built-in bloom, depth of field and HUD mods for about fifty games, in each game's Graphics settings.
- **Controller extras** in **Settings → Controls**: light bar colors, a turbo button, quick save buttons and, in beta, a GameCube-style trigger click.
- Optimizations and fixes for reported bugs.

> [!WARNING]
> **Porpoise is still in early development on the PS5.** If you run into bugs or glitches in a game, please report them on the **[RIPALDA Discord](https://discord.gg/GgDE5Vynyu)**, with the game's name, and we'll get them fixed as soon as we can.

## Install or update

> [!NOTE]
> **Having trouble with PS5 Upload?** If it doesn't work for you, copy Porpoise over FTP instead (in binary mode, see below). Still stuck? Reach out on the **[Discord](https://discord.gg/GgDE5Vynyu)** and we'll help you out.

**Use ShadowMountPlus 1.7 beta 4 or newer.** With older versions, Porpoise can close the moment it opens on some setups (firmware 12.x with the Lapy JB Daemon or LegacyJB).

**From 2.0 or later:** **Settings → About → Updates**.

**By hand:** download **`Porpoise-2.5.zip`**, delete `/data/homebrew/PPSA99764/` on the PS5, and copy the **`PPSA99764`** folder into **`/data/homebrew/`**. With FTP, use binary transfers (FileZilla: Transfer → Transfer type → Binary), or some text files won't copy. Your games, saves and settings in `/data/porpoise/` are kept.

Porpoise contains no games, BIOS or firmware. Play only games you own. Not affiliated with Nintendo, Sony or RetroAchievements. Licenses are in `licenses/README.txt` inside the zip.

`Porpoise-2.5.zip` SHA-256: `a6de0af1d2b427cad5f69a32203407115d91b71e8291f92acf2545a5850a7600`
