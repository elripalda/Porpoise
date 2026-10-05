<p align="center"><img src="https://github.com/elripalda/Porpoise-Dolphin-Emulator-for-PS5/raw/main/docs/images/banner.png" alt="Porpoise" width="100%"></p>

**Porpoise 2.0 beta 3.** A test version of 2.0: faster, smoother, steadier games, a newer Dolphin and graphics driver, and a set of new features. Features marked *beta* are still being tuned. If you'd rather stay on a finished version, keep 1.5.1.

## Faster games

- **Much faster in 3D-heavy games.** Dolphin rounds every single-precision multiply-add exactly as the GameCube does, and on the PS5 that one correction was eating a third of the emulated CPU's time in WWE Day of Reckoning 2. It's now off, as it was in Dolphin until 2025. If a game ever needs it, **Settings → System → Exact multiply-add** turns it back on, for every game or just one.
- **The emulator gets cores of its own:** the emulated CPU and its graphics each keep to a core of the PS5 that nothing else busy shares. (Settings → System → *Emulator on its own cores*.)
- **Starts about five seconds sooner:** the menus' lettering is made once and kept, instead of at every start.

## Smoother and steadier

- **V-Sync on the TV's own refresh.** Every frame now waits on the TV's vblank instead of Porpoise's own timer, and a late frame doesn't wait for the next one, so a game just short of 60 doesn't drop to 30. (Settings → Video → V-Sync.)
- **No more waiting on the sound.** 1.5.1 held nearly every frame about 6 ms for the speakers. That's gone.
- **Newer Dolphin core**, with the fix that saves new shader pipelines in the background instead of in the middle of a frame.
- **Newer PS5 graphics driver** (Mihawk's PS5_Vulkan / PS5 Mesa). Porpoise now sends the TV a 1080p picture instead of drawing every frame at 4K. Want it sharper on a 4K TV? **Settings → Video → Output resolution** offers 1440p and 4K.
- **Smoother menus:** covers load in the background and fade in, and Memory Cards opens without a hitch.
- **Threaded GPU recording** *(beta, off)*: Settings → Graphics. Worth trying on a demanding game.

## New

- **Quick resume** *(beta)*: leave a game from the in-game menu and it picks up right there next time. Settings → Graphics.
- **Cheats and patches** *(beta)*: a game's settings list the codes and patches Dolphin knows for it, one switch each.
- **Check my setup:** shows what Porpoise can see on your console (/data, USB drives, games, covers) and what to do about anything missing. It opens the first time 2.0 starts, and any time from Settings → Games.
- **Saves on a USB drive** *(beta)*: Options in Memory Cards copies a GameCube or Wii save to the drive's `Porpoise Saves` folder; Settings → Games → *Saves from a USB drive* copies them back.
- **Report a bug:** Settings → About saves Porpoise's logs to `/data/porpoise/reports` (and your USB drive) and shows a QR code to open an issue. **Performance report** records where a slow game spends its time.
- **Choose a version:** Settings → About lists every release, betas included, so you can move to any version, newer or older. Turn on **Beta updates** to be offered test versions.
- **Wii multiplayer:** in Wii games, every other controller is another player's own Wii Remote, with its own pointer and motion.
- **Every menu translated again** into all eleven languages, including everything added since the Wii update.

## Install or update

1.5.1's updater only offers finished versions, so this beta goes on by hand once. From then on, **Settings → About → Choose a version** can move you to any release, and back to 1.5.1.
1. Download **`Porpoise-2.0-beta.3.zip`** below and unzip it. You get a folder named **`PPSA99764`**.
2. If you're updating, delete `/data/homebrew/PPSA99764/` on the PS5 first. Your games, saves, covers and settings live in `/data/porpoise/` and are kept.
3. Copy the **`PPSA99764`** folder into **`/data/homebrew/`** over FTP or with PS5 Upload.
4. For a new install, register it like your other homebrew (for example with ShadowMountPlus).

Something wrong? Use **Settings → About → Report a bug** and [open an issue](https://github.com/elripalda/Porpoise-Dolphin-Emulator-for-PS5/issues) with the report's files, your firmware and which HEN you run.

## Legal

Porpoise contains **no games and no BIOS or firmware files**. The only key it carries is the Wii disc key Dolphin itself includes, used to read a Wii disc's own banner. Play only games you own, from backups you made yourself. Porpoise is not affiliated with Nintendo or Sony. Emulation is provided by [Dolphin](https://dolphin-emu.org) (GPL-2.0-or-later). Porpoise is GPL-3.0-or-later; every component's licence and source revision is listed in `licenses/README.txt` inside the zip.

`Porpoise-2.0-beta.3.zip` SHA-256: `(filled in at release)`
