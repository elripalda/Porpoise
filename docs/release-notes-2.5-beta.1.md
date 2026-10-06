<p align="center"><img src="https://github.com/elripalda/Porpoise-Dolphin-Emulator-for-PS5/raw/Main/docs/images/banner.png" alt="Porpoise" width="100%"></p>

**Porpoise 2.5 Beta 1** is a test version focused on sound. Features in a beta are still being tuned; if you'd rather stay on a finished version, keep 2.1.1.

- **Smoother sound:** when a game runs below full speed, its sound now comes through Dolphin's own mixer, as Dolphin plays it on a PC, which covers the gaps instead of crackling.
- **Sound presets** in **Settings → Audio**: Smooth, Responsive, Extra smooth and Classic (2.1), with the audio buffer and gap filling on their own rows.
- An **Audio** tab in the in-game menu: volume, mute, the sound preset and the rest, while you play.
- Games no longer run faster than their own speed.
- Optimizations and fixes for reported bugs.

> [!WARNING]
> **Porpoise is still in early development on the PS5.** If you run into bugs or glitches in a game, please report them on the **[RIPALDA Discord](https://discord.gg/GgDE5Vynyu)**, with the game's name, and we'll get them fixed as soon as we can.

## Install or update

> [!NOTE]
> **Having trouble with PS5 Upload?** As of October 6, 2026, some players couldn't install this update with PS5 Upload. If it doesn't work for you, copy Porpoise over FTP instead (in binary mode, see below). Still stuck? Reach out on the **[Discord](https://discord.gg/GgDE5Vynyu)** and we'll help you out.

**From 2.0 or later:** turn on **Settings → About → Beta updates**, then **Settings → About → Updates**.

**By hand:** download **`Porpoise-2.5-beta.1.zip`**, delete `/data/homebrew/PPSA99764/` on the PS5, and copy the **`PPSA99764`** folder into **`/data/homebrew/`**. With FTP, use binary transfers (FileZilla: Transfer → Transfer type → Binary), or some text files won't copy. Your games, saves and settings in `/data/porpoise/` are kept.

Porpoise contains no games, BIOS or firmware. Play only games you own. Not affiliated with Nintendo, Sony or RetroAchievements. Licenses are in `licenses/README.txt` inside the zip.

`Porpoise-2.5-beta.1.zip` SHA-256: `(set after the build)`
