# Changelog

## 1.5 — 2026-10-04

### Revolution look (Settings → Interface → Look)
- A bright home screen of game tiles, twelve to a page: the first tile goes
  back to the game played last, page arrows and L2 / R2 turn the page, and a
  floor along the bottom keeps a segmented clock, the date, and round buttons
  for Settings and the memory cards. Porpoise's mark and the tabs sit on top.
- **Point with the controller:** a hand pointer (Porpoise's own drawing) that
  leans as you twist the controller, a tap of rumble and a tick as it lands
  on something. The D-pad works as well; the touch pad turns pointing on and
  off; R3 centres it.
- **Wii discs' own tiles and banners:** Porpoise reads each Wii disc's banner
  (.iso, .rvz, .wia, .wbfs, .ciso, .gcz) and plays it — animated tiles on the
  home screen, the full banner and its jingle when a tile opens. GameCube games
  keep their covers.
- **Opened tile:** Cross on a tile opens it, with **Start** and **Wii
  controls** (or Game settings) below, save states, save data and favourite;
  Triangle turns between the banner and the game's facts.
- Every other screen goes white with it: Memory Cards, Settings, the Wii Remote
  setup, the guide, dialogs and the in-game menu. A cover flow layout is there
  too (Interface → Home screen).
- Switching to it shows the Wii games; GameCube games are one press away in
  Sort & filter. Each look remembers its own choice.

### Memory Cards
- **Wii saves:** L2 / R2 switches between the GameCube cards and the Wii saves
  Dolphin keeps, each with its own banner, icon and name. Square backs one up,
  Triangle deletes it.

### Also
- Sort & filter (All / GameCube / Wii), "Get covers and info now", covers for
  games added while a download runs, custom texture packs (the core now sees
  the console's memory), per-game "Before this game" for the Wii Remote setup,
  developer options kept out of the way, the updater's "Finishing up".

## 1.1 — 2026-10-04 (build 15)

### Recommended settings (beta)
- Every game's settings open with **Recommended**: Dolphin's own fixes for the
  game and Porpoise's tested picks, each with an **on/off switch** — green is
  on — and a tag saying where it comes from. Picks are updated daily.
- Picks can change Dolphin's own per-game settings too. The first one is for
  **WWE Day of Reckoning 2**: EFB copies stay on the GPU and textures decode
  there, which should lift it well above the ~6 fps it ran at (still being
  tested).
- A game's Dolphin settings now **stay put when you change graphics in the
  in-game menu** (the Dolphin core used to drop them until the next launch).
- If you keep your own Dolphin settings file for a game, Recommended says so
  and leaves it alone.

### In-game menu
- Four tabs, switched with L1 / R1: **Game**, **Video**, **Graphics** and
  **Controls**, with nearly every setting there, applied at once and saved for
  that game.
- **Save states:** three slots per game, each with a picture. *Save state…* and
  *Load state…* open a slot picker: saving over a used slot needs a second
  press, loading shows empty slots dimmed. A spinner shows while a state is
  written, and *Fast save states* (on by default) leaves Dolphin's texture
  cache out of them, so saving takes a moment instead of many seconds.
  Pictures fit the game's real shape — 4:3 or 16:9 — without the black bars.
  Start a game from a slot in its Details page.
- **Setups:** save a game's Video and Graphics settings as one of four setups
  and use it on any other game, from the menu or from Settings.
- **Fast forward:** 2x or 4x, shown in a corner while it's on.
- The Controls tab draws the DualSense with a line out to every button and the
  GameCube button it plays, and opens Customize buttons over the paused game.

### Controls
- **PlayStation is the default layout** (Cross is A, Circle is B). The
  **GameCube** layout now matches Dolphin's: Circle is A, Cross is B (it was
  Cross A, Square B). Saved settings keep the layout they had.
- **Four layouts of your own** (*My layout 1–4*) beside GameCube and
  PlayStation. Any game can use any of them. 1.0's custom buttons become
  *My layout 1* (a game's own custom buttons move to a free layout).
- New controller art and **button icons everywhere**, from Zacksly's
  *PS5 Button Icons and Controls* (CC BY 3.0).
- The DualSense **light bar** shows each player's colour.

### Picture
- **Nine screen filters:** Smooth, Sharp, Sharpen, CRT, Arcade CRT, VHS,
  **Soft VHS** (soft, faded colour and glow, no glitches), **8-bit** and
  **Pocket** (a green handheld screen), with a strength setting.
- **Eight borders** for 4:3 games: **Porpoise** (the logo and name), Porpoise
  glass, Midnight, **Frost**, **Carbon**, and three cabinets — Arcade, **Retro**
  and **Synthwave** — plus your own PNGs in `/data/porpoise/borders/`. Borders
  now draw at full resolution.

### Library
- **Favourites** (Options): a gold star on the box, and a *Favourites first*
  sort.
- **Play time** for every game, and a *Most played* sort.

### Languages
- **Twelve languages**, each with its flag: English, Español (España),
  **Español (Latinoamérica)**, Français, **Deutsch**, **Italiano**,
  **Nederlands**, **Polski**, Português (Portugal), **Português (Brasil)**,
  **Русский** and **日本語**. *System* follows the PS5's language.
- Game descriptions in Spanish, French, Portuguese, Italian, German and Dutch
  where GameTDB has them.
- Translations are first drafts: corrections go in
  `/data/porpoise/lang/<code>.txt` (see README) and need no new build.

### Updates
- **Settings > About > Updates:** *Check now* looks on GitHub, and when a newer
  Porpoise is out the same row **installs it**: the download is checked against
  GitHub's SHA-256 and every file against the release's manifest before
  anything is replaced, then Porpoise closes so the new one opens next time.
  Games, saves and settings are never touched.
- An update notice in the library when a new Porpoise is out.

### Fixes and speed
- **Can't see /data, empty folder browser, games in /data/games not found:**
  when the console starts Porpoise inside the app sandbox, Porpoise now asks the
  HEN (etaHEN / OnionHEN) to free it, as PS5SX2 does, and says so if it can't.
  Settings, memory cards and states a sandboxed 1.0 kept in the app's own
  folder come along to `/data/porpoise` the first time.
- **A clearer folder browser:** it opens on the whole console at `/data`,
  lists the game files in each folder (and how many each folder holds), keeps
  drives and shortcuts on Triangle (only drives with something on them), shows
  folders the old one missed, and copies a game from a USB drive to the
  console with one press.
- Shaders compile on four background threads instead of one: less stutter the
  first time a game shows something new.
- "Last played 1 weeks ago" is gone.

### Also
- A **QR code** in About that opens the bug tracker on your phone.
- About credits **Mihawk (mihawk-99)**, who brought the Dolphin core to the PS5.
- Second discs of two-disc games keep their own history and states.

## 1.0 — 2026-10-03

The first public release of Porpoise.

### Controllers
- Up to four players, one per signed-in PS5 user. Controllers can join in the
  middle of a game.
- Three button layouts: GameCube (the default), PlayStation (Cross A, Circle B)
  and Custom.
- Customize buttons: a picture of the DualSense with the GameCube button on each
  control. Pick a button, press the one you want, and the two swap.

### Library
- A cover-flow shelf of thick glass box-art tiles in a perspective grid room,
  with gloss, chromatic edges and a slow moving reflection.
- Automatic game search in the usual folders and on USB and extended drives.
  You can also add folders with a built-in folder browser, searched four
  levels deep.
- Supports `.iso`, `.gcm`, `.rvz`, `.ciso`, `.gcz`, `.wbfs` and `.wia`.
- Sort by title or by recently played.

### Box art and details
- Box art from GameTDB: 5:7 front covers, back covers and disc label art, which
  appear as they download.
- Game details from GameTDB: description, developer, publisher, release date,
  genre, players and rating.
- Square flies the box into Details, Triangle shows the back of the box, the
  right stick turns it, and the disc spins alongside.
- L2 / R2 swipe to the previous or next game inside Details.

### Memory cards
- Slot A and Slot B with save icons, banners, block counts and dates.
- Copy and delete saves, each with a confirmation.
- *Save data* on a game's Details page jumps to that game's save.

### Settings
- A rail of sections: Video, Graphics, Audio, Controls, System, Games,
  Interface and About.
- Many Dolphin options: internal resolution (1080p default), widescreen hack,
  aspect ratio, anisotropic filtering, anti-aliasing, texture filtering, output
  resampling, ubershaders, texture cache accuracy, per-pixel lighting, fog,
  overscan, custom textures, CPU clock, dual core, fast disc loading, cheats,
  console language and progressive scan.
- Per-game settings, with changed values highlighted and a reset to default.
- Reset all settings.

### In-game menu
- Options + touch pad pauses the game and slides in a menu with Resume, quick
  video and volume settings, Quit to library and Close Porpoise.

### Music, sound and languages
- Original menu music and sound effects by @elripalda, with separate switches
  and volumes.
- English, Spanish, French and Portuguese. Porpoise follows the PS5's language,
  with an override in Settings and optional translation fix files. More
  languages are coming in a future update.

### Under the hood
- Dolphin's JIT on the PS5's CPU, with Vulkan through Mesa's RADV.
- Frames locked to the TV's vblank when the display allows it, for even
  motion; clean 48 kHz sound; never above 60 fps.
- Asynchronous ubershaders by default, so games don't stall on new shaders.
- Textures are freed only after the GPU is done with them, and large images are
  scaled down to save memory.
