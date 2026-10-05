<p align="center"><img src="https://github.com/elripalda/Porpoise/raw/main/docs/images/banner.png" alt="Porpoise" width="100%"></p>

**Porpoise 2.0** is the biggest update yet. Games run faster and smoother on a newer Dolphin and a newer PS5 graphics driver. There are fifteen themes, with colors, fonts and layouts to mix. WiiWare, Virtual Console and channels now play, along with homebrew apps. There are new sound and system options, the DualSense's microphone and speaker work in games, and sixteen languages are included.

## Performance

Measured on the same PS5 with Porpoise's own logs (`core.log` and `trace.txt`):

| | 1.5.1 | 2.0 |
|---|---|---|
| **WWE Day of Reckoning 2**, in a match | 24 fps (40% of full speed) | **[TBD]** |
| Time each frame waits on the speakers | about 6 ms | **[TBD]** |
| Menus ready when Porpoise opens | 5.7 s, at every start | **[TBD]** after the first start |
| Frames timed to the TV's refresh | never (0 of 120) | **[TBD]** |
| Picture drawn for the TV | 3840×2160 | 1920×1080 (1440p or 4K if you choose) |

- **Much faster in 3D-heavy games.** Recent Dolphin rounds every single-precision multiply-add exactly as the GameCube does, and on the PS5 that correction costs a lot in games full of 3D math. It's now off, as it was in Dolphin before 2025. **Settings → System → Exact multiply-add** turns it back on for any game that needs it.
- **The emulator gets cores of its own.** The emulated CPU and its graphics each run on a PS5 core that nothing else busy shares (Settings → System → *Emulator on its own cores*).
- **V-Sync on the TV's own refresh.** Frames wait on the TV's vblank instead of Porpoise's own timer. A late frame doesn't wait for the next one, so a game that runs just short of 60 doesn't drop to 30.
- **No more waiting on the sound.** 1.5.1 held almost every frame back for the speakers. Not anymore.
- **1080p output by default.** The new graphics driver lets Porpoise send a 1080p picture instead of drawing every frame at 4K. **Settings → Video → Output resolution** offers 1440p and 4K.
- **Newer Dolphin core**, which writes new shader pipelines in the background instead of in the middle of a frame, so games stutter less the first time they draw something new.
- **Faster, smoother menus.** Porpoise opens about five seconds sooner after its first start. Covers load in the background and fade in, and Memory Cards opens without a hitch.
- **No more freeze when you quit to the library.** The game fades to black while Porpoise saves your spot, and the menus fade back in once they're ready.

## A new look

- **Fifteen themes**, in Settings → Interface → Theme: Porpoise, Revolution, **Star Cube**, OLED, Minimal, Cube, Broadcast, Terminal, Depth, Aurora, Aero, Dot Matrix, Synthwave, Paper and Crystal. Each has its own room behind the menus.
- **Star Cube** has a home of its own. A big glass cube turns slowly on black while small cubes drift around it. Its edges are Games, Calendar, Memory Cards and Settings.
- **Fifteen color palettes and eight fonts** that work with any theme. Choosing a theme brings its own colors and font, and you can change either afterward.
- **Eight library views:** Cover flow, Wheel, Disc flow, Shelf, Box, List, Stack and Helix. Memory Cards has **four views**: Cards, Blocks, By game and Cubes.
- **Liquid glass** panels and buttons, smoother motion everywhere, and an **in-game menu that matches your theme**.
- **A fresh start:** the first start offers three looks to begin with. **Settings → Interface → Reinitialize Porpoise** starts over at any time. Your games, folders, memory cards, saves and save states are kept.

## More to play

- **WiiWare, Virtual Console and channels:** `.wad` files you made from your own Wii show up in the library with their names and banners. A new **Channels** filter lists just them.
- **Homebrew apps:** an app folder (`boot.dol` or `boot.elf` with `meta.xml` and `icon.png`) shows up with its name and icon.
- **WBFS and GCZ games are recognized.** Wii games in `.wbfs` are no longer listed as GameCube. If a game is ever detected wrong, set it in its settings under **Controls → Console**.
- **Wii multiplayer:** in Wii games, every other controller is another player's Wii Remote, with its own pointer and motion.
- **Fast forward buttons:** in any game, **touch pad + R1** steps through off, 2x and 4x, and **touch pad + R2** fast-forwards while you hold it.
- **Quick resume** *(beta, off)*: leave a game from the in-game menu, and next time Porpoise offers **Resume** or **Start Over**.
- **Cheats and patches** *(beta)*: each game's settings list the codes Dolphin knows for it, with one switch each.
- **Saves on a USB drive** *(beta)*: copy GameCube and Wii saves to a drive and back.

## Sound and system *(beta)*

- **Sound** (Settings → Audio):
  - **Accurate audio** (Dolphin's exact sound chip).
  - **Wii Remote speaker**, on the TV or from each player's DualSense.
  - **Audio buffer** (Low, Normal or Safe).
  - **Audio stretching**, so a game that slows down doesn't crackle.
  - **Microphone:** the DualSense's mic is the GameCube Microphone (Mario Party 6 and 7, hold R3 to talk) and the Wii Speak.
- **The consoles' own settings** (Settings → System):
  - Wii widescreen, PAL games at 60 Hz, and the sensor bar above or below the TV.
  - **GameCube boot animation**, from your own GameCube's BIOS.
  - **Start Wii discs in the Wii Menu**, from your own Wii Menu.

Porpoise includes no Nintendo files. The [README](https://github.com/elripalda/Porpoise#channels-wiiware-and-homebrew) shows where to put your own.

## Everything else

- **Sixteen languages**, with four new ones: **简体中文, 繁體中文, 한국어 and Türkçe**. All menus are translated again.
- **Accessibility:** text size, color filters for color blindness, high contrast, reduced motion and larger button hints.
- **Check my setup:** a checklist of what Porpoise can see (/data, USB drives, games, covers) and how to fix anything missing.
- **Choose a version:** Settings → About lists every release, so you can move to any version, newer or older. **Beta updates** offers test versions.
- **Sturdier:** damaged or unusual game files can no longer close Porpoise while it reads your library.
- **New icon and backdrop** on the PS5 home screen, and **no more Dolphin messages** over your game.

## Help and community

Help, bug reports and news are now on the **[RIPALDA Discord](https://discord.gg/GgDE5Vynyu)**. In Porpoise, **Settings → About → Report a bug** saves your logs and shows a QR code that opens the Discord on your phone. Ruben's other projects are at **[ripalda.dev](https://ripalda.dev)**.

## Install or update

**From 1.1, 1.5 or 1.5.1:** open **Settings → About → Updates** and install 2.0 from there.

**By hand or new install:**
1. Download **`Porpoise-2.0.zip`** below and unzip it. You get a folder named **`PPSA99764`**.
2. If you're updating, delete `/data/homebrew/PPSA99764/` on the PS5 first. Your games, saves, covers and settings live in `/data/porpoise/` and are kept.
3. Copy the **`PPSA99764`** folder into **`/data/homebrew/`** over FTP or with PS5 Upload.
4. For a new install, register it like your other homebrew (for example with ShadowMountPlus).

## Legal

Porpoise contains **no games and no BIOS or firmware files**. The only keys it carries are the two Wii disc keys (standard and Korean) that Dolphin itself includes, which it uses to read a disc's or channel's own banner. Play only games you own, from backups you made yourself. Porpoise is not affiliated with Nintendo or Sony. Emulation is provided by [Dolphin](https://dolphin-emu.org) (GPL-2.0-or-later). Porpoise is GPL-3.0-or-later, and every component's license and source revision is listed in `licenses/README.txt` inside the zip.

`Porpoise-2.0.zip` SHA-256: `7047ad2946bf6eb418f805a2f5ae463b6ed7125dc8e7e911cfe7932e7bb5e9ae`
