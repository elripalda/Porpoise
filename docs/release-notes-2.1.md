<p align="center"><img src="https://github.com/elripalda/Porpoise-Dolphin-Emulator-for-PS5/raw/Main/docs/images/banner.png" alt="Porpoise" width="100%"></p>

**Porpoise 2.1** brings **RetroAchievements** with unlocks that pop up like PS5 trophies, your own cheats folder, custom buttons for every Wii controller, Porpoise's folder on extended storage or a USB drive, an Online switch for WiiLink, and fixes for what you reported after 2.0.

## RetroAchievements (softcore)

- **Sign in from your library:** press **L1 + Square** (or go to **Settings → Games → RetroAchievements**) and sign in with your [RetroAchievements](https://retroachievements.org) account. The panel has its own on-screen keyboard. Porpoise keeps a sign-in token, never your password.
- **Unlocks pop up like PS5 trophies:** the achievement's badge, its title and the trophy sound. Completing a game plays the platinum sound, and each game starts with a popup showing how many you've unlocked.
- The popups go through your jailbreak's **ELF loader** (port 9021); without one they're plain notifications.
- Built on Dolphin's own RetroAchievements support, so every disc format Porpoise plays is recognized and save states keep your progress. Softcore only: hardcore mode isn't available.

## Cheats and buttons

- **Your own cheats folder:** drop a game's codes in `/data/porpoise/cheats/<game ID>.ini` (Dolphin's Action Replay or Gecko format). They turn on by themselves when the game starts, and each one gets its own switch in the game's **Cheats**. Wiimmfi patch codes go here too.
- **Customize buttons for Wii controllers:** Remote + Nunchuk, Remote, Sideways and Classic each get their own layout (**Settings → Controls → Customize buttons**, L1 / R1 to pick the controller).

## Video and system

- **Output resolution** is now the first row of **Video**, and **Match the PS5** is the default: Porpoise keeps the console's own output, so your TV doesn't switch modes when Porpoise opens.
- **Online (beta)** in **Settings → System**: WiiConnect24 channels through WiiLink.
- **Move Porpoise's folder** to extended storage or a USB drive (**Settings → Games**). Settings, saves, save states, covers and texture packs all move together, with a progress bar.

## Fixes

- The **Custom textures** row no longer disappears from Details after you play.
- Games with a texture pack now say so when they start: **Custom textures: N found**.
- Porpoise asks to leave the app sandbox before anything else starts, which should stop it closing right after Lapy frees it.
- The sandbox message has a **Don't show again** button, and a switch in **Settings → Games** brings it back.

## Coming in 2.5: Netplay

Playing GameCube and Wii games **together online** is what we're building next. Follow along on the **[RIPALDA Discord](https://discord.gg/GgDE5Vynyu)**.

## Install or update

**From 2.0 (or 1.x):** open **Settings → About → Updates** and install 2.1 from there.

**By hand or new install:**
1. Download **`Porpoise-2.1.zip`** below and unzip it. You get a folder named **`PPSA99764`**.
2. If you're updating, delete `/data/homebrew/PPSA99764/` on the PS5 first. Your games, saves, covers and settings live in `/data/porpoise/` and are kept.
3. Copy the **`PPSA99764`** folder into **`/data/homebrew/`** over FTP or with PS5 Upload.
4. For a new install, register it like your other homebrew (for example with ShadowMountPlus).

## Legal

Porpoise contains **no games and no BIOS or firmware files**. Play only games you own, from backups you made yourself. Porpoise is not affiliated with Nintendo, Sony or RetroAchievements. Emulation is provided by [Dolphin](https://dolphin-emu.org) (GPL-2.0-or-later); achievements by RetroAchievements' [rcheevos](https://github.com/RetroAchievements/rcheevos) (MIT). Porpoise is GPL-3.0-or-later, and every component's license and source revision is listed in `licenses/README.txt` inside the zip.
