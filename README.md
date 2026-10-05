<p align="center">
  <img src="docs/images/banner.png" alt="Porpoise - Dolphin Emulator for PS5 " width="100%">
</p>

<p align="center">
  <a href="https://github.com/elripalda/Porpoise/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/elripalda/Porpoise?label=release&color=3b8cff"></a>
  <img alt="Platform: PS5 homebrew" src="https://img.shields.io/badge/platform-PS5%20homebrew-1f5fd6">
  <img alt="Powered by Dolphin" src="https://img.shields.io/badge/emulation-Dolphin-5cd3ff">
  <a href="LICENSE"><img alt="License: GPL-3.0-or-later" src="https://img.shields.io/badge/license-GPL--3.0--or--later-a9a6ff"></a>
</p>

<p align="center">
  <b>Porpoise</b> is a GameCube and Wii emulator for jailbroken PS5, built on
  <a href="https://dolphin-emu.org">Dolphin</a>, with its own launcher for the TV and the DualSense.
</p>

<p align="center">
  <a href="https://github.com/elripalda/Porpoise/releases/latest"><b>Download Porpoise 1.5.1</b></a> ·
  <a href="#install">Install</a> ·
  <a href="#controls">Controls</a> ·
  <a href="BUILDING.md">Build from source</a> ·
  <a href="CREDITS.md">Credits</a>
</p>

---

## What it is

Porpoise is a PS5 homebrew app (title ID `PPSA99764`) that runs the Dolphin
emulator's libretro core inside its own launcher. It has a game library with
box art, a memory-card manager, per-game settings and an in-game menu, all
driven with a DualSense. Dolphin's x86-64 JIT runs on the console's CPU, and
graphics go through Vulkan on Mesa's RADV driver.

It plays your own GameCube and Wii disc backups. It doesn't include any games,
BIOS or firmware files.

## Features

### The library
- Games are shown as a row of box-art tiles. Left and right move between them.
- Porpoise looks for games in the usual folders and on USB and extended drives
  (see [Adding games](#adding-games)). You can also add your own folders; each
  is searched four levels deep.
- Formats: `.iso`, `.gcm`, `.rvz`, `.ciso`, `.gcz`, `.wbfs` and `.wia`.
- Sort by title, recently played, most played or favourites first. Each game
  shows its play time and when you last played it.

### Revolution theme
- **Settings → Interface → Theme** switches between **Porpoise** (the cover
  flow) and **Revolution**: a bright home screen of game tiles, twelve to a
  page, with a segmented clock and the date along the bottom and round buttons
  for Settings and the memory cards. Arrows at the sides (or L2 / R2) turn the
  page. *Home screen* can switch it to a cover flow instead.
- **Point with the controller:** a hand pointer that follows the DualSense,
  opens over empty space, points over anything you can choose and leans as you
  twist. The D-pad works too, the touch pad turns pointing on and off, and R3
  centres it.
- **Wii games play their own tile and banner**, animated, read from the disc
  (any of the formats above), with the banner's jingle when a tile opens.
  GameCube games show their covers.
- **Cross** on a tile grows it to fill the screen and opens it, with **Wii
  Controls** (or **Game Settings**) and **Start**, plus save states, save data
  and favourite. **Square** on a tile plays it straight away.
- Memory Cards, Settings, dialogs and the in-game menu all go white with it.
  Switching to Revolution shows your Wii games and Wii saves; switching back
  shows every game and the GameCube cards.

### Wii Remote on the DualSense (beta)
- Each Wii game gets a Wii controller: **Remote + Nunchuk** (the default),
  **Remote**, **Remote sideways**, **Classic Controller**, or **two
  controllers** (alpha: a second DualSense is the Nunchuk).
- The Remote's pointer comes from the gyro (hold R1 a moment to centre it),
  the touch pad or the right stick. The DualSense's motion is the Remote's,
  and a quick flick shakes it.
- **Wii Remote setup:** pick the controller, centre, then point at two corners
  of your screen, so the pointer matches your TV and how far away you sit. It
  can come up before each Wii game (Triangle skips it), has Simple and Advanced
  modes and keeps up to four presets.
- **Grip:** Auto reads how you're holding the DualSense, or choose one.
  **How to hold it** (Settings → Wii Remote) draws the DualSense as each Wii
  controller with every button labelled.

### Box art and game info
- Front covers, back covers and disc art download from
  [GameTDB](https://www.gametdb.com) while the console is online.
- Game details from GameTDB: description, developer, publisher, release date,
  genre, number of players and rating.
- To use your own art, put a PNG named after the game's disc ID in the covers
  folder.
- Covers and info can each be turned off under **Settings → Games**.

### The details page
- **Square** opens a game's details. **Triangle** shows the back of the box,
  the **right stick** turns it, and **L2 / R2** move to the previous or next
  game.
- From here: **Play**, **Save states**, **Game settings** and **Save data**.
- **Save states:** three slots per game, each with a picture. You can start a
  game from any of them.
- **Options** marks a game as a favourite, here or in the library.

### Memory cards
- Slot A and Slot B side by side, with each save's icon, banner, title, block
  count and date, and each slot's free blocks.
- **Square** copies a save to the other slot and **Triangle** deletes it. Both
  ask first.
- **Save data** on a game's details page goes straight to that game's save.
- **Wii saves:** L2 / R2 switches to the Wii saves Dolphin keeps, each with its
  banner, icon and name. **Square** backs one up and **Triangle** deletes it.

### Controllers and players
- Up to four players. Every controller signed in to a PS5 user is a player,
  and controllers can join in the middle of a game.
- **Button layouts:** *PlayStation* (the default: Cross is A, Circle is B),
  *GameCube* (Circle is A, Cross is B, as in Dolphin) and four layouts of your
  own (*My layout 1–4*).
- **Customize buttons** shows the DualSense with the GameCube button each
  control plays. Pick a GameCube button, press the DualSense button you want,
  and the two swap. It also works from the in-game menu.
- Any game can use any layout. Analog L and R follow the triggers.
- The light bar shows each player's colour.

### Settings
Settings are split into sections:

| Section | What's inside |
|---|---|
| **Video** | Internal resolution (1080p by default; 4x and above marked *experimental*), widescreen hack, aspect ratio, anisotropic filtering, texture filtering, anti-aliasing (MSAA/SSAA), output resampling, **screen filter** and its strength, **border**, FPS counter |
| **Graphics** | Shader compilation (asynchronous ubershaders by default), texture cache accuracy, per-pixel lighting, disable fog, crop overscan, custom texture packs, skip duplicate frames |
| **Audio** | Game volume and mute; **menu music** and **menu sounds**, each with its own switch and volume |
| **Controls** | Button layout (PlayStation, GameCube or one of your four), Customize buttons, vibration, connected controllers |
| **Wii Remote** | Wii Remote setup, presets, setup before each Wii game, How to hold it, Wii controller, pointer, pointer speed, grip, motion, flick to shake |
| **System** | Emulated CPU clock (50–300%), dual core, fast disc loading, cheats, console language, progressive scan |
| **Games** | Find games automatically, add or remove game folders, search again, download covers, download game info |
| **Interface** | Menu language, **Theme** (Porpoise or Revolution), Revolution's home screen and pointer, reduced motion, larger text, **reset all settings** |
| **About** | Version, **Updates** (check GitHub and install a new Porpoise), a QR code for reporting a bug, and credits |

- **Per-game settings.** Any game can override the Video, Graphics, Audio, Controls
  and System settings. Changed values show in blue, and *This game → Reset to
  default* clears them.
- **Recommended settings (beta).** Supported games come with recommended
  settings you can switch on. The list is small for now and will grow as more
  games are tested. A game's settings open with **Recommended**: **Dolphin's own
  fixes** for it, from Dolphin's per-game database (on by themselves), and
  **Porpoise's picks**, settings tested on PS5. Each has an on/off switch —
  green is on — and a tag saying where it comes from; changes apply the next
  time the game starts. Picks can set Porpoise's own options and Dolphin's
  per-game settings alike (the first, for *WWE Day of Reckoning 2*, keeps EFB
  copies and texture decoding on the GPU). They come from
  [`data/recommended.ini`](data/recommended.ini) in this repository and update
  by themselves once a day; suggestions for more games are welcome.
- **Setups.** Save a game's Video and Graphics settings as one of four setups
  (in-game menu → Graphics → *Save as a setup*) and use it on any other game,
  from the in-game menu or the game's settings.

### Screen filters and borders
- **Screen filters:** *Smooth*, *Sharp*, *Sharpen*, *CRT* (scanlines and an
  aperture grille), *Arcade CRT* (a curved tube with rounded corners), *VHS*
  (tracking wobble, colour bleed and tape noise), *Soft VHS* (soft, faded
  colour and a gentle glow, no glitches), *8-bit* (a small palette, big pixels
  and dithering) and *Pocket* (a four-green handheld screen), each with a
  strength.
- **Borders** fill the bars beside a 4:3 picture when widescreen is off:
  *Porpoise* (the logo and name), *Porpoise glass*, *Midnight*, *Frost*,
  *Carbon*, and three cabinets with a curved opening — *Arcade cabinet*,
  *Retro cabinet* (wood and 70s stripes) and *Synthwave cabinet* (a striped
  sun and a neon grid).
  **Make your own:** a 1920×1080 PNG, transparent where the picture shows (the
  4:3 picture fills x 240–1680), in `/data/porpoise/borders/`. It appears in
  the list by its file name.
- **Reset all settings** puts Porpoise back the way it ships. Your games,
  folders and saves are kept.

### In-game menu
Press **Options + touch pad** together while playing. The game pauses and the
menu opens, with four tabs (**L1 / R1**):

- **Game:** Resume, **Save state…** and **Load state…** (three slots, with
  pictures: pick a slot with left and right; saving over a used slot asks for a
  second press, and a spinner shows while the state is written), **Fast
  forward** (2x or 4x), Volume, Quit to library and Close Porpoise.
- **Video:** resolution, widescreen, aspect ratio, anti-aliasing, anisotropic
  filtering, screen filter and strength, border, FPS counter.
- **Graphics:** shader compilation, texture cache, texture filtering, output
  resampling, per-pixel lighting, fog, overscan, duplicate frames, and **Save
  as a setup / Use a setup**.
- **Controls:** button layout, **Customize buttons** over the paused game,
  vibration, and the DualSense drawn with a line to every button saying which
  GameCube button it is.

Changes apply immediately and are saved for that game.

When you resume, the game ignores your buttons until you let go, so the press
that closed the menu never reaches the game.

### Music and sound
- Original menu music and sound effects by Ruben. The music fades out when a
  game starts and comes back in the library.
- Music and effects each have their own switch and volume.

### Languages
- **Sixteen languages**, each with its flag in the language list (Chinese
  with its script's character instead): English,
  Español (España), **Español (Latinoamérica)**, Français, **Deutsch**,
  Italiano, **Nederlands**, **Polski**, Português (Portugal), **Português
  (Brasil)**, **Türkçe**, **Русский**, **日本語**, **한국어**, **简体中文** and
  **繁體中文**. Porpoise follows your PS5's system
  language, and **Settings → Interface → Language** overrides it.
- Game descriptions from GameTDB come in Spanish, French, Portuguese, Italian,
  German and Dutch too, where GameTDB has them (English otherwise).
- The translations are first drafts, and fixes are welcome. To fix a line
  without waiting for an update, create `/data/porpoise/lang/<code>.txt` —
  `es`, `es-419`, `fr`, `pt`, `pt-BR`, `it`, `de`, `nl`, `pl`, `ru`, `tr`,
  `ja`, `ko`, `zh-Hans` or `zh-Hant` —
  with lines such as `Quit to library = Volver a la biblioteca`. In the source,
  translations live in [`i18n/`](i18n).

### Performance
- When the TV's refresh rate allows it, each frame is shown on its own vblank;
  otherwise Porpoise keeps its own clock. Games run at their normal speed,
  with 48 kHz sound.
- Dolphin compiles shaders on four background threads with asynchronous
  ubershaders, so games stutter less the first time they draw something new.
  The shader cache is kept between runs.
- Dolphin's x86-64 JIT with fastmem, and Vulkan through RADV.
- Some games are slow on PS5 because of Dolphin's own per-game fixes; see
  **Recommended** in a game's settings and the speed line in `core.log`.

## Requirements

- A PS5 able to run homebrew, with **etaHEN** and **kstuff** loaded and
  **ShadowMountPlus** (or your usual method) to register homebrew titles.
- A way to copy files to the console: FTP, or a tool such as PS5 Upload.
- **Your own games**, as backups you made from discs you own. Porpoise
  includes no games and no system files of any kind.
- Optional: an internet connection on the console, for box art and game info.

## Install

1. Download **`Porpoise-1.5.1.zip`** from the
   [latest release](https://github.com/elripalda/Porpoise/releases/latest) and
   unzip it on your computer. You get a folder named **`PPSA99764`**.
2. Connect to your PS5 with FTP (for example etaHEN's FTP server and a client
   like FileZilla) or with PS5 Upload.
3. Copy the whole **`PPSA99764`** folder into **`/data/homebrew/`**, so you end
   up with `/data/homebrew/PPSA99764/`.
4. Register it the same way as your other homebrew (for example with
   ShadowMountPlus). The Porpoise tile appears on the home screen.
5. Copy your game backups into **`/data/porpoise/games/`** (Porpoise creates it
   on first launch), into any folder listed under [Adding games](#adding-games),
   or onto a USB drive, then open Porpoise.

Box art and game details download automatically while the console is online.

### Updating
**From 1.1 on, Porpoise updates itself:** **Settings → About → Updates**.
*Check now* looks on GitHub (Porpoise also looks once a day while it's online
for covers), and when a newer Porpoise is out the same row installs it. The
download is checked against GitHub's SHA-256 and every file against the
release's manifest before anything is replaced; if anything goes wrong,
nothing is changed. Porpoise then closes; open it again from the home screen.
Your games, saves, covers and settings live in `/data/porpoise/` and are never
touched.

**By hand** (and from 1.0, which can't update itself): delete
`/data/homebrew/PPSA99764/`, then upload the new folder — ShadowMountPlus keeps
serving the old copy until it is removed. If your 1.0 couldn't reach `/data`
(your games were in the app's own `porpoise/games` folder), copy the new files
over the old folder instead of deleting it: 1.1 moves your settings, memory
cards and states to `/data/porpoise` itself and still finds games there. Check
**Settings → About** to confirm the version.

## Adding games

With **Find games automatically** on (the default), Porpoise looks in:

- `/data/porpoise/games`
- `/data/games`, `/data/GameCube`, `/data/Wii`, `/data/roms`, `/data/iso`
- every USB drive (`/mnt/usb0`–`/mnt/usb7`) and extended storage
  (`/mnt/ext0`, `/mnt/ext1`)

Each of these is searched four folders deep, so games sorted into subfolders
are found too.

To use any other folder, go to **Settings → Games → Add a game folder**, browse
to it, and press **Square**. The browser shows the game files in each folder,
so you can see where they are. Porpoise searches that folder and four levels
below it. After copying new games, use **Search for games now**.

### Playing from a USB or external drive

1. **Format the drive as exFAT** on your computer. FAT32 also works but can't
   hold files over 4 GB (most Wii `.iso` files are bigger); the PS5 doesn't
   read NTFS.
2. **Copy your games onto it**, anywhere up to four folders deep, for example
   `games/Wii/Super Mario Galaxy.rvz` or `GameCube/Melee.iso`. `.rvz` files
   are much smaller than `.iso` and load just as well.
3. **Plug it into the PS5** and make sure a jailbreak daemon is running (see
   [Troubleshooting](#troubleshooting)). Porpoise has to be out of the app
   sandbox to see any drive: if it says it can't reach `/data`, it can't see
   your drive either.
4. **Open Porpoise.** The games appear in the library. If you plugged the
   drive in after opening Porpoise, use **Settings → Games → Search for games
   now**.
5. Games deeper than four folders? Add their folder with **Add a game folder**
   (Triangle in the browser jumps to your drives).

You can also copy a game from the drive to the console: in **Add a game
folder**, browse to it on the drive and press **Cross** on the game.

**PS5 extended storage** (a drive the PS5 formatted for its own games) shows
up as `/mnt/ext0`. A computer can't read that format, so copy games to it over
FTP instead (for example into `/mnt/ext0/games/`).

## Controls

### Launcher

| Button | Library | Details | Memory cards |
|---|---|---|---|
| D-pad / left stick | Browse games | Choose an action | Move between saves and slots |
| **Cross** | Play | Confirm | – |
| **Circle** | – | Back to the library | Back |
| **Square** | Open Details | – | Copy to the other slot |
| **Triangle** | Sort | Front / back of the box | Delete |
| **Options** | Favourite | Favourite | – |
| **L2 / R2** | – | Previous / next game | – |
| **Right stick** | – | Turn the box | – |
| **L1 / R1** | Switch tabs: Library, Memory Cards, Settings | | |

### In a game

| Button | Action |
|---|---|
| **Options + touch pad** | Pause and open the in-game menu |
| **Circle** (in the menu) | Resume |
| **L1 / R1** (in the menu) | Switch tabs: Game, Video, Graphics, Controls |

With the default **PlayStation** layout:

| GameCube | DualSense |
|---|---|
| A / B / X / Y | Cross / Circle / Square / Triangle |
| Z | R1 |
| L / R (analog) | L2 / R2 |
| Start | Options |
| Control stick / C-stick | Left stick / right stick |
| D-pad | D-pad |

The **GameCube** layout puts A on Circle and B on Cross. To set any button
yourself, go to **Settings → Controls → Customize buttons** (or the in-game
menu's Controls tab), and give a game its layout in its own settings.

**More players:** turn on another controller and choose a PS5 user for it. It
becomes the next player, up to four, and can join in the middle of a game.

## Where things are kept

| Path | What |
|---|---|
| `/data/homebrew/PPSA99764/` | The app. Replace it to update. |
| `/data/porpoise/games/` | A good place for your game files |
| `/data/porpoise/covers/` | Box art, back covers and disc art (`<disc ID>.png`); replace any with your own |
| `/data/porpoise/saves/` | Dolphin's data, including the memory cards (`User/GC/<region>/Card A`, `Card B`) |
| `/data/porpoise/game-settings/` | Each game's own settings |
| `/data/porpoise/states/` | Save states, three per game, with their pictures |
| `/data/porpoise/borders/` | Your own borders (1920×1080 PNGs) |
| `/data/porpoise/info.tsv` | Downloaded game details (`info-es.tsv` and so on for other languages) |
| `/data/porpoise/recommended.ini` | Porpoise's recommended settings, as last downloaded |
| `/data/porpoise/setups/` | Your four setups (Video and Graphics settings to use on any game) |
| `/data/porpoise/banners/` | Wii discs' banners, read once from each disc |
| `/data/porpoise/saves/User/Wii/backups/` | Wii saves you backed up from Memory Cards |
| `/data/porpoise/latest-release.json` | What GitHub last said about the newest release |
| `/data/porpoise/lang/` | Your own translation fixes (optional) |

## Troubleshooting

- **Adding a game folder:** *Settings → Games → Add a game folder* opens on
  the whole console (`/`), on `data`. Each folder shows how many games are in
  it, and the games themselves are listed under the folders, so you can see
  where they are. **Square** uses the folder you're in (add as many as you
  like), **Triangle** jumps to your drives and Porpoise's own games folder,
  and **Cross on a game** on a USB drive copies it to the console.
- **Still seeing the old version after updating by hand?** Delete
  `/data/homebrew/PPSA99764/` completely before uploading the new folder.
- **"Porpoise can't reach /data", only a few folders in the folder browser, or
  games in `/data/games` not showing?** The console started Porpoise inside the
  app sandbox, and a jailbreak daemon has to free it. Make sure one is running
  **before** you open Porpoise:
  - **etaHEN:** turn on **Legacy Command Server** in etaHEN's Toolbox
    settings. etaHEN's own app-jailbreak list is built in and can't be edited
    to add Porpoise, so this is the setting that lets etaHEN free it.
  - **OnionHEN:** add `PPSA99764` to `exact_title_ids` under `[app_jailbreak]`
    in `/data/OnionHEN/config.ini` (comma-separated), then restart the console
    or reload OnionHEN.
  - **A standalone daemon** such as **Lapy** (loaded from your Homebrew
    Launcher or `autoload.txt`). Porpoise speaks both the classic Lapy request
    and the newer owned-root one.

  Then open Porpoise. It now keeps asking the daemon for a few seconds as it
  starts, so a daemon that loads a moment late or needs a second try is handled
  on its own — but if a launch still lands in the sandbox, just open Porpoise
  once more. This commonly explains "it worked the first time but not after":
  the daemon wasn't up, or running, on the later launch. Until it's freed,
  Porpoise keeps its things in the app's own folder
  (`/data/homebrew/PPSA99764/porpoise/`).
- **A game runs slowly?** Open its settings and look at **Recommended**: some
  of Dolphin's own fixes for a game cost speed on PS5, and Porpoise's picks
  turn them off where that's been tested. The speed line in `core.log` (below)
  shows how fast the game really runs.
- **No covers?** The console needs to be online when Porpoise opens, and
  **Settings → Games → Download covers** must be on. A few discs have no art on
  GameTDB. You can add your own.
- **Something went wrong?** These two files help when you report a problem:
  - `/data/homebrew/PPSA99764/trace.txt`
  - `/data/homebrew/PPSA99764/porpoise/core.log` (records the game's real speed
    every 10 seconds)

Please [open an issue](https://github.com/elripalda/Porpoise/issues) with your
console firmware, the game's disc ID and those logs.

## Known limitations

- Wii support is experimental. The Wii Remote on the DualSense is in beta:
  pointing works well, motion-heavy games are still being tuned, and two
  controllers as Remote and Nunchuk is alpha.
- Save states are tied to the Porpoise and Dolphin version that made them; a
  future update may not load older ones. Your memory card saves always carry
  over.
- Netplay and achievements aren't part of Porpoise.
- Memory-card copy and delete work on saves Dolphin keeps as files (the normal
  `Card A` and `Card B` folders).

## Build from source

Porpoise builds on Linux or WSL with clang 18, Mesa's RADV driver for PS5 and
the PS5 payload SDK. See **[BUILDING.md](BUILDING.md)**.

## Credits

Porpoise was created by **Ruben ([@elripalda](https://www.elripalda.com))**:
the design, the launcher, the logo, the menu music and the sound effects.

It stands on the work of many people. The full list, with licences, is in
**[CREDITS.md](CREDITS.md)**:

- **[Dolphin](https://dolphin-emu.org)** by the Dolphin Emulator Project, the
  emulator itself, and the **[libretro Dolphin core](https://github.com/libretro/dolphin)**.
- **[libretro / RetroArch](https://www.libretro.com)**, whose API Porpoise uses to host Dolphin.
- **[Mihawk (mihawk-99)](https://github.com/mihawk-99)**, who brought the
  Dolphin core to the PS5. His **[PS5 RetroArch](https://github.com/mihawk-99/PS5_RetroArch)**,
  **[PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan)** and
  **[PS5_Mesa](https://github.com/mihawk-99/PS5_Mesa)** are the PS5 Dolphin port,
  the platform layer and Vulkan on PS5 through **[Mesa](https://mesa3d.org)'s
  RADV** that Porpoise is built on. Porpoise wouldn't exist without them.
- **[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk)** by John Törnblom
  and contributors, and **BlackBearReloaded**'s native title pipeline.
- **[GameTDB](https://www.gametdb.com)** and its contributors, for the box art
  and game details.
- **[Zacksly](https://zacksly.itch.io)** ([@_Zacksly](https://twitter.com/_Zacksly)),
  for *PS5 Button Icons and Controls* (CC BY 3.0): the DualSense drawings and the
  button icons, adapted for Porpoise.
- The **[Wii Banner Player Project](https://github.com/jordan-woyak/wii-banner-player)**
  (zlib), for how a Wii disc's banner is read and played, and the
  **[Zstandard](https://github.com/facebook/zstd)** educational decoder, for
  reading `.rvz` discs.
- **[Nunito](https://github.com/googlefonts/nunito)** and Noto Sans
  **[JP](https://github.com/google/fonts/tree/main/ofl/notosansjp)**,
  **[SC](https://github.com/google/fonts/tree/main/ofl/notosanssc)**,
  **[TC](https://github.com/google/fonts/tree/main/ofl/notosanstc)** and
  **[KR](https://github.com/google/fonts/tree/main/ofl/notosanskr)**,
  **[JetBrains Mono](https://github.com/JetBrains/JetBrainsMono)**,
  **[VT323](https://github.com/google/fonts/tree/main/ofl/vt323)**,
  **[Doto](https://github.com/oliverlalan/Doto)**,
  **[Exo 2](https://github.com/googlefonts/Exo-2.0)** and
  **[Lora](https://github.com/cyrealtype/Lora-Cyrillic)** (as Porpoise Serif) (SIL OFL), and **[stb](https://github.com/nothings/stb)** by Sean Barrett.
- The PS5 scene: **etaHEN**, **kstuff** and **ShadowMountPlus**, and the front
  ends **PS5SX2** and **ProsperoEden** for the inspiration. Porpoise asks the
  HEN to leave the app sandbox the way PS5SX2 does.

## Legal

> [!IMPORTANT]
> **Porpoise does not condone piracy.** It contains no games and no console
> BIOS, IPL or firmware files, and none will ever be provided or linked to. The
> only key it carries is the Wii disc key that Dolphin itself includes, used to
> read a Wii disc's own banner. Play only games you own, as backups you made
> from your own discs.

Porpoise is an independent, free and open-source project. It is **not
affiliated with, endorsed by or sponsored by Nintendo or Sony Interactive
Entertainment**, and it contains no code, artwork, sounds or other material from
either. *Nintendo*, *GameCube* and *Wii* are trademarks of Nintendo.
*PlayStation* and *PS5* are trademarks of Sony Interactive Entertainment Inc.
These names are used only to describe compatibility.

Emulation is provided by Dolphin (GPL-2.0-or-later). Box art and game details
are downloaded on your console from GameTDB and are not distributed with
Porpoise.

## License

Porpoise is free software, licensed under the
**[GNU General Public License v3.0 or later](LICENSE)**. Parts carried over
from other projects keep their own notices. Every component that ships in the
app, with its licence and exact source revision, is listed in
`licenses/README.txt` inside the release and in [CREDITS.md](CREDITS.md).

<p align="center"><sub>Made with care by <a href="https://www.elripalda.com">@elripalda</a>.</sub></p>
