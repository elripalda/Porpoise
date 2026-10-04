# Changelog

## 1.1 — the quality-of-life update

### In-game menu
- Four tabs, switched with L1 / R1: **Game**, **Video**, **Graphics** and
  **Controls**, with nearly every setting there, applied at once and saved for
  that game.
- **Save states:** three slots per game, each with a picture. Save and load
  from the menu, or start a game from a slot in its Details page.
- **Fast forward:** 2x or 4x, shown in a corner while it's on.
- The Controls tab draws the DualSense with a line out to every button and the
  GameCube button it plays, and opens Customize buttons over the paused game.

### Controls
- **Four layouts of your own** (*My layout 1–4*) beside GameCube and
  PlayStation. Any game can use any of them. 1.0's custom buttons become
  *My layout 1* (a game's own custom buttons move to a free layout).
- New controller art and **button icons everywhere**, from Zacksly's
  *PS5 Button Icons and Controls* (CC BY 3.0).
- The DualSense **light bar** shows each player's colour.

### Picture
- **Screen filters:** Smooth, Sharp, Sharpen, CRT, Arcade CRT and VHS, with a
  strength setting.
- **Borders** for 4:3 games: Porpoise glass, Midnight and Arcade cabinet, plus
  your own PNGs in `/data/porpoise/borders/`.

### Library
- **Favourites** (Options): a gold star on the box, and a *Favourites first*
  sort.
- **Play time** for every game, and a *Most played* sort.
- **Recommended settings** in each game's settings: Dolphin's own fixes for the
  game, and Porpoise's tested picks, applied with one press and updated daily.

### Languages
- **Italiano** and **日本語** join English, Español, Français and Português,
  with a flag beside each language.
- Game descriptions in Spanish, French, Portuguese and Italian where GameTDB
  has them.

### Also
- An **update notice** when a new Porpoise is out (library and About).
- A **QR code** in About that opens the bug tracker on your phone.
- Faster save-state pictures; second discs of two-disc games keep their own
  history and states.

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
