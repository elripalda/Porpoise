<p align="center"><img src="https://github.com/elripalda/Porpoise-Dolphin-Emulator-for-PS5/raw/Main/docs/images/banner.png" alt="Porpoise" width="100%"></p>

**Porpoise 2.7** is a big one:

- **Home screen tiles:** give any game its own tile on the PS5 home screen that opens straight into it (a game's settings → **Home screen**).
- **Tile art editor:** see the tile's icon and background as the home screen shows them. Use the cover, the back of the box, a screenshot, the title screen or your own picture; crop and zoom it, or set it whole over a blur, white, black or a Porpoise pattern.
- **Settings wait for Apply** (Square), ask before you leave with changes, and offer a restart for the few that need one, in the menus and in-game.
- **Inverted sticks:** the control stick and the C-stick, up-down, left-right or both, for each game.
- **Scanlines, Shadow mask and LCD** screen filters.
- **First-start setup** questions, and a **Diagnostic test** in Settings → About.
- **Optimizations and fixes for reported bugs.**

### Also in 2.7
- **Faster save states** (about half a second to save), with pictures of the whole image at any resolution.
- **Download covers again**, for every game or just one, and the cover glides into the launch screen.
- **Faster start:** games are found in the background, and system folders are skipped.
- **PAL games** that switch to 60 Hz are followed.
- **Stay in the sandbox** (Settings → Games) for consoles where freeing Porpoise closes it, and Porpoise asks what to do if it closed last time.
- **Revolution** shows every game, GameCube and Wii.
- Other apps can start a game in Porpoise (`--rom`, thanks to MartinPham).

### Catching up from an older version?
- **2.5:** smoother sound and sound presets, an Audio tab in-game, the GameCube controller for Wii games, Dolphin's graphics mods, and controller extras (light bar colors, turbo, quick save buttons).
- **2.1 and 2.1.1:** RetroAchievements with PS5-style trophy popups and achievement lists, your own cheats folder, custom buttons for every Wii controller, true widescreen per game, Porpoise's folder on extended storage or USB, and Online (beta) for WiiLink.

> [!TIP]
> ### 💙 Enjoying Porpoise?
> If you've gotten some enjoyment out of this project, donations and love are always appreciated. Every bit helps keep Porpoise growing. Thank you!
>
> [![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/ripalda)

> [!WARNING]
> **Porpoise is still in early development on the PS5.** If you run into bugs or glitches in a game, please report them on the **[RIPALDA Discord](https://discord.gg/GgDE5Vynyu)**, with the game's name, and we'll get them fixed as soon as we can.

## Install or update

> [!NOTE]
> **Having trouble with PS5 Upload?** If it doesn't work for you, copy Porpoise over FTP instead (in binary mode, see below). Still stuck? Reach out on the **[Discord](https://discord.gg/GgDE5Vynyu)** and we'll help you out.

**Use ShadowMountPlus 1.7 beta 4 or newer.** With older versions, Porpoise can close the moment it opens on some setups (firmware 12.x with the Lapy JB Daemon or LegacyJB).

**From 2.0 or later:** **Settings → About → Updates**.

**By hand:** download **`Porpoise-2.7.zip`**, delete `/data/homebrew/PPSA99764/` on the PS5, and copy the **`PPSA99764`** folder into **`/data/homebrew/`**. With FTP, use binary transfers (FileZilla: Transfer → Transfer type → Binary), or some text files won't copy. Your games, saves and settings in `/data/porpoise/` are kept.

Porpoise contains no games, BIOS or firmware. Play only games you own. Not affiliated with Nintendo, Sony or RetroAchievements. Licenses are in `licenses/README.txt` inside the zip.

`Porpoise-2.7.zip` SHA-256: `dbb895934d84a2445dc36cbcabdc0c299359aa76ddb540c8b4791da5e400891c`
