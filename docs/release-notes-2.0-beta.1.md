<p align="center"><img src="https://github.com/elripalda/Porpoise-Dolphin-Emulator-for-PS5/raw/main/docs/images/banner.png" alt="Porpoise" width="100%"></p>

**Porpoise 2.0 Beta 1** — the first public beta of 2.0, and the biggest performance update yet. Games run faster and smoother, on a newer Dolphin and a newer PS5 graphics driver. Features marked *beta* are still being tuned; if you'd rather stay on a finished version, keep 1.5.1.

## Performance highlights

- **Up to twice as fast in 3D-heavy games.** WWE Day of Reckoning 2 went from about 24 fps to 50+ in testing. Dolphin rounds every single-precision multiply-add exactly as the GameCube does, and on the PS5 that one correction was eating a third of the emulated CPU's time. It's now off, as it was in Dolphin until 2025; **Settings → System → Exact multiply-add** turns it back on for any game that needs it.
- **The emulator gets cores of its own.** The emulated CPU and its graphics each keep to a PS5 core that nothing else busy shares. (Settings → System → *Emulator on its own cores*.)
- **V-Sync on the TV's own refresh.** Frames now wait on the TV's vblank instead of Porpoise's own timer, and a late frame doesn't wait for the next one, so a game just short of 60 doesn't drop to 30.
- **No more waiting on the sound.** 1.5.1 held nearly every frame about 6 ms for the speakers. That's gone.
- **1080p output by default.** The new graphics driver lets Porpoise send the TV a 1080p picture instead of drawing every frame at 4K. Want it sharper on a 4K TV? **Settings → Video → Output resolution** offers 1440p and 4K.
- **Newer Dolphin core**, which compiles new shader pipelines in the background instead of in the middle of a frame: fewer stutters the first time a game draws something new.
- **Starts about five seconds sooner**, and the menus stay smooth: covers load in the background and fade in, and Memory Cards opens without a hitch.

## Smoother and cleaner

- **Newer PS5 graphics driver:** Mihawk's PS5_Vulkan / PS5 Mesa.
- **Threaded GPU recording** *(beta, off)*: Settings → Graphics. Worth trying on a demanding game.
- **No more freeze when you quit to the library:** the game fades to black while Porpoise saves your spot, and the menus fade back in once everything has loaded.
- **A cleaner start:** the dolphin fades in on black, then the menus fade in with the music.
- **No more Dolphin messages over your game:** the yellow "Saved to memory card"-style notes are gone.

## New

- **Quick resume** *(beta, off)*: turn it on in Settings → Graphics, and leaving a game from the in-game menu keeps your spot. Next time you start that game, Porpoise asks **Resume** or **Start Over**, with a picture of where you were.
- **Cheats and patches** *(beta)*: a game's settings list the codes and patches Dolphin knows for it, one switch each.
- **Check my setup:** a checklist of what Porpoise can see on your console (/data, USB drives, games, covers) and what to do about anything missing. It opens the first time 2.0 starts, and any time from Settings → Games.
- **Saves on a USB drive** *(beta)*: Options in Memory Cards copies a GameCube or Wii save to the drive's `Porpoise Saves` folder; Settings → Games → *Saves from a USB drive* copies them back.
- **Report a bug:** Settings → About saves Porpoise's logs to `/data/porpoise/reports` (and your USB drive) and shows a QR code to open an issue. **Performance report** records where a slow game spends its time.
- **Choose a version:** Settings → About lists every release, betas included, so you can move to any version, newer or older. Turn on **Beta updates** to be offered test versions.
- **Wii multiplayer:** in Wii games, every other controller is another player's own Wii Remote, with its own pointer and motion.
- **Four new languages: 简体中文, 繁體中文, 한국어 and Türkçe** — sixteen in all. Porpoise follows your PS5's language, or pick one in Settings → Interface → Language.
- **Every menu translated again** into every language, including everything added since the Wii update.
- Beta features wear a **BETA** badge in Settings, and Porpoise has a **new icon and backdrop** on the home screen.

## Install or update

1.5.1's updater only offers finished versions, so this beta goes on by hand once. From then on, **Settings → About → Choose a version** can move you to any release, and back to 1.5.1.
1. Download **`Porpoise-2.0-beta.1.zip`** below and unzip it. You get a folder named **`PPSA99764`**.
2. If you're updating, delete `/data/homebrew/PPSA99764/` on the PS5 first. Your games, saves, covers and settings live in `/data/porpoise/` and are kept.
3. Copy the **`PPSA99764`** folder into **`/data/homebrew/`** over FTP or with PS5 Upload.
4. For a new install, register it like your other homebrew (for example with ShadowMountPlus).

Something wrong? Use **Settings → About → Report a bug** and [open an issue](https://github.com/elripalda/Porpoise-Dolphin-Emulator-for-PS5/issues) with the report's files, your firmware and which HEN you run.

## Legal

Porpoise contains **no games and no BIOS or firmware files**. The only key it carries is the Wii disc key Dolphin itself includes, used to read a Wii disc's own banner. Play only games you own, from backups you made yourself. Porpoise is not affiliated with Nintendo or Sony. Emulation is provided by [Dolphin](https://dolphin-emu.org) (GPL-2.0-or-later). Porpoise is GPL-3.0-or-later; every component's licence and source revision is listed in `licenses/README.txt` inside the zip.

`Porpoise-2.0-beta.1.zip` SHA-256: `264ba20cc4aad1de1a30e337cb7b747b5ae82b81488f653215c74af416a8df86`
