# Changelog

## 2.1.1 — 2026-10-06

- **Achievements lists:** an Achievements tab in the in-game menu, and a
  game's list from its Details (Square), with badges.
- **Widescreen done per game:** games with a widescreen code (Warped Polygon's
  collection, or Dolphin's own) play in true 16:9 by default; the rest stay
  4:3 unless you choose the emulated hack, which now warns what it does.
- Achievement popups drawn by Porpoise when the console's own can't show.
- Optimizations and fixes for reported bugs.

## 2.1 — 2026-10-05

RetroAchievements with PS5-style trophy popups, your own cheats folder,
custom buttons for every Wii controller, Porpoise's folder on extended
storage or a USB drive, an Online switch for WiiLink, and fixes for what
players reported after 2.0.

### RetroAchievements (softcore)
- **Sign in from the library:** **L1 + Square** (or Settings → Games →
  RetroAchievements) opens the account panel, with an on-screen keyboard.
  Porpoise keeps a sign-in token in `retroachievements.ini`, never your
  password. Sign out from the same panel.
- **Unlocks pop up like PS5 trophies:** the achievement's badge, its title and
  the trophy sound, in the PS5's own notifications (the Trophies channel, so
  your trophy notification settings apply). Completing a game plays the
  platinum sound. When a game starts, a popup says how many you've unlocked.
- The popups go through your jailbreak's ELF loader (port 9021), as PS5SX2's
  do; without one they're plain notifications.
- Dolphin's own RetroAchievements support, built into the PS5 core: every disc
  format Porpoise plays is recognized, and save states keep your progress.
  Softcore only: hardcore mode isn't available.
- Leaving a game waits a few seconds for an unlock still being sent.

### Cheats and buttons
- **Your own cheats folder:** put a game's codes in `/data/porpoise/cheats/`
  as `<game ID>.ini` (Dolphin's format, Action Replay or Gecko) and they turn
  on by themselves when the game starts. Each shows in the game's **Cheats**
  with its own switch. Wiimmfi's online patch codes go here too.
- **Customize buttons for Wii controllers:** Remote + Nunchuk, Remote,
  Sideways and Classic each get their own layout (Settings → Controls →
  Customize buttons, L1 / R1 pick the controller).

### Video and system
- **Output resolution** is now the first row of Video, and **Match the PS5**
  is the default: Porpoise keeps the console's own output, so the TV doesn't
  switch modes when Porpoise opens.
- **Online (beta)** (Settings → System): WiiConnect24 channels through
  WiiLink.
- **Move Porpoise's folder** (Settings → Games) to extended storage, a second
  extended drive or a USB drive: settings, saves, save states, covers and
  texture packs move together, with a progress bar and Circle to stop.

### Fixes
- The **Custom textures** row no longer disappears from Details after you play.
- A game with a texture pack says so as it starts: **Custom textures: N found**.
- Porpoise asks the jailbreak to leave the app sandbox before anything else
  starts, which should stop it closing right after Lapy frees it.
- The sandbox message has **Don't show again**, and Settings → Games →
  *Sandbox message at start* turns it back on.

### Coming in 2.5
- **Netplay** is next: playing GameCube and Wii games together online.

## 2.0 — 2026-10-05

A big update: faster and steadier games, WiiWare, Virtual Console and
channels, homebrew apps, new sound and system settings, the DualSense's
microphone and speaker, a new look with fifteen themes and Star Cube's own
home, fonts and colors to mix, more ways to see your games and saves,
accessibility settings, four new languages and a Discord for help.

Porpoise still ships no Nintendo files: channels, the Wii Menu and the
GameCube BIOS come from your own consoles. The README says how to add each.

### Channels, WiiWare and homebrew
- **WAD files play:** WiiWare, Virtual Console and channels you made from your
  own Wii appear in the library with their names and banners, marked
  *WiiWare*, *Virtual Console* or *Channel*. A new **Channels** filter shows
  only them.
- **Homebrew apps:** an app's folder as the Homebrew Channel keeps it
  (`boot.dol` or `boot.elf` beside `meta.xml` and `icon.png`) shows with its
  name and icon.
- **WBFS and GCZ games are recognized:** their headers are read, so Wii games
  in `.wbfs` are no longer listed as GameCube, and get their covers, banners
  and Wii controls.
- **Console** (a game's settings → Controls): Auto, GameCube or Wii, for a game
  Porpoise detected wrong.
- Covers for channels and Virtual Console titles are looked up by their
  four-letter ID too.

### Sound (beta)
Settings → Audio, for every game or one:
- **Accurate audio:** Dolphin's exact sound chip (LLE), for the few games
  whose sound is missing or wrong with the fast one.
- **Wii Remote speaker:** the Remote's own sounds in the TV's sound, or from
  each player's DualSense speaker, as on a Wii.
- **Audio buffer:** Low, Normal or Safe, against crackling in demanding games.
- **Audio stretching:** a game that slows down slows its sound with it instead
  of crackling.
- **Microphone:** the DualSense's microphone as the GameCube Microphone (Mario
  Party 6 and 7; hold R3 to talk) and the Wii Speak.

### The consoles' own settings (beta)
Settings → System:
- **Wii widescreen**, **PAL games at 60 Hz** and the **sensor bar** above or
  below the TV, as the Wii's own settings.
- **GameCube boot animation** from your own GameCube's BIOS: its `IPL.bin` in
  `/data/porpoise/bios/USA`, `EUR` or `JAP`.
- **Start Wii discs in the Wii Menu**, from the Wii Menu WAD of your own Wii.
  Started once from the library, the Wii Menu stays installed.

### Fast forward buttons
- **Touch pad + R1** steps fast forward (off, 2x, 4x) and **touch pad + R2**
  fast-forwards while held, without opening the menu. Settings → Controls →
  Fast forward buttons turns them off.

### Help and community
- **The RIPALDA Discord** (discord.gg/GgDE5Vynyu) is now where to get help and
  report bugs; Settings → About has its QR code.
- **About** is tidied: Ruben's mark beside his name, @elripalda and
  ripalda.dev, and the credits in one place.

### Sturdier
- **Damaged or unusual game files can no longer close Porpoise** while it reads
  the library: the disc, WAD and banner readers check every size and value in
  a file before using it (tested against thousands of damaged files). This is
  the likely cause of Porpoise closing quietly as it started with etaHEN 2.6b's
  jailbreak; if it still happens, share `trace.txt` on the Discord.

### Faster games
- **The emulator gets cores of its own.** While a game runs, Dolphin's
  emulated CPU and its video loop each keep to a core of the PS5 that
  Porpoise's other work stays off, so neither shares a core with something
  busy. **Settings → System → Emulator on its own cores** (on) turns it off,
  for every game or one.
- **Much faster emulated CPU in 3D-heavy games.** A profile of WWE Day of
  Reckoning 2 on the console showed a third of the emulated CPU's time going
  to one correction routine: Dolphin (since 2025) rounds every
  single-precision multiply-add exactly as the GameCube does, at a cost the
  PS5 feels badly in games full of 3D math. It's now off, as Dolphin itself
  was until last year; **Settings → System → Exact multiply-add** turns it
  back on, for every game or one.
- The video thread waits more gently while it polls for work, leaving more of
  the processor to the emulated CPU.

### Smoother and steadier
- **V-Sync on the TV's own refresh.** Frames now wait on the display's vblank
  itself instead of Porpoise's own timer (which never locked to the TV in
  1.5.1). Porpoise takes the display's handle from the graphics driver as it
  opens the screen. A frame that is late doesn't wait for the next vblank, so a game
  that runs just short of 60 doesn't fall to 30. Settings → Video → V-Sync
  (on); off keeps the old timer.
- **No more waiting on the speakers.** In 1.5.1 nearly every frame waited
  about 6 ms for the sound buffer to drain; the buffer's ceiling now sits
  above the resampler's target, so it doesn't.
- **Newer Dolphin core** (libretro/dolphin after Dolphin 2609), with the fix
  that writes new shader pipelines to the cache in the background instead of
  in the middle of a frame. The core builds from source again.
- **Newer PS5 graphics driver** (Mihawk's PS5_Vulkan / PS5 Mesa dc82d01). The
  console's video output now offers 1080p, so Porpoise sends a 1080p picture
  instead of drawing every menu and game frame at 3840×2160.
  **Settings → Video → Output resolution** can choose 1440p or 4K instead
  (from the next start).
- **Memory Cards opens without a hitch:** both cards and the Wii saves are
  read in the background as the tab slides in.
- **Starts about five seconds sooner.** The menus' text is drawn from
  pictures of every letter that took about six seconds to make at each start;
  they are now kept in `/data/porpoise/cache` after the first.
- **Menus:** covers, disc art and box backs are read and decoded in the
  background and fade in, instead of stalling a frame for each cover that
  scrolls into view; memory card icons no longer wait on the GPU one by one.
- **Threaded GPU recording** (beta, Settings → Graphics, off): the driver
  records Dolphin's drawing on a thread of its own. Worth trying on a
  demanding game.
- **Quitting to the library no longer freezes:** the in-game menu and the game
  fade to black together, Porpoise saves where you were behind the dolphin,
  the library's pictures load, and the menus fade back in once they're ready.
- **No Porpoise logo flashing on the launch screen** as the game takes over
  the screen: the game's cover stays.
- **A proper start:** the console's splash stays until Porpoise is ready, then
  the dolphin rises out of black with a soft glow and stays while the library
  loads. It lifts only once the menus run smoothly: the background shows, then
  the top bar and the page fade and rise in as the music comes up.
- **No more Dolphin messages over the game:** the yellow notes Dolphin shows
  ("Saved to memory card", changed video settings and the like) are gone.

### New
- **Quick resume** (beta, Settings → Graphics, off): leaving a game from the
  in-game menu keeps where you were. Next time, Porpoise asks **Resume** or
  **Start Over**, with a picture of where you left off. Restart in the
  in-game menu starts it over too.
- **Cheats and patches** (beta): each game's settings list the codes and
  patches Dolphin knows for it, one switch each.
- **Check my setup:** shown after the first start's welcome, and any time from
  Settings → Games: a checklist of what Porpoise can see (/data, USB drives,
  games, covers) and what to do about anything missing.
- **Saves on a USB drive** (beta): Options in Memory Cards copies a GameCube
  or Wii save to the drive's `Porpoise Saves` folder; Settings → Games →
  Saves from a USB drive copies them back in.
- **Report a bug:** Settings → About saves Porpoise's logs to
  `/data/porpoise/reports` (and a USB drive), with a QR code for the Discord's
  bug reports. **Performance report** records where a slow game spends its
  time.
- **Choose a version:** Settings → About lists every release, betas included,
  to install a newer or an older one. **Beta updates** offers test versions
  as they come out.
- **Wii multiplayer:** in Wii games every other controller is another
  player's own Wii Remote, with its own pointer and motion.
- **Four new languages:** **简体中文** (Simplified Chinese), **繁體中文**
  (Traditional Chinese), **한국어** (Korean) and **Türkçe** (Turkish), sixteen
  in all. Porpoise picks them up from the PS5's own language, or Settings →
  Interface → Language. Chinese and Japanese text now wraps between
  characters, as those languages do.
- **Every menu translated again:** the texts added since the Wii work (the
  Wii Remote setup, Revolution, Wii saves and 2.0's settings) in every
  language; the Revolution clock shows the date in each language's order.
- Beta features carry a **BETA** badge in Settings.
- **New icon and backdrop** on the PS5's home screen.

### A new look
- **Liquid glass:** panels and buttons bend the room behind them at their
  edges, catch the light along their rims with a thin spectrum, and cast a
  soft shadow. Corners are rounder; the tab bar and buttons are fully round.
- **Smaller button hints** along the bottom, and only one Play: the big
  button under the chosen game is gone (Cross still plays it).
- **More motion:** highlights in Settings glide from row to row, and the
  menus come in in steps when Porpoise starts and when you come back from a
  game.
- **The in-game menu matches your theme:** its glass, colors and font.

### Themes
Fifteen themes in Settings → Interface → Theme, each with its own room:
- **Porpoise**, **Revolution** and, new:
- **Star Cube:** a home of its own. A big glass cube floats on black, turning
  slowly, small cubes drifting around it; its four edges are **Games**,
  **Calendar**, **Memory Cards** and **Settings**. The stick turns the cube to
  an edge, Cross flies in, Circle comes back out; L1 / R1 move between pages.
  - **Games:** your library on a glass page as spinning discs or covers
    (Settings → Interface → Games page); each game opens its own page with
    Play, Save states, Game settings and Save data.
  - **Calendar:** the time and date, the month with what you played each
    day, and your latest games; L2 / R2 change the month.
  - **Memory Cards:** each save a little glass cube.
  - **Settings** in violet glass with dot-matrix headings, and button hints
    that read "✕ ··· Confirm".
- **OLED:** true black, crisp text, no glow.
- **Minimal:** quiet charcoal and gray, nothing but what you need.
- **Cube:** indigo and violet on black, a glass cube turning over a grid.
- **Broadcast:** a 2000s tube TV in its frame: a curved screen, scanlines and
  pixel lettering.
- **Terminal:** green phosphor on black in a monospaced font.
- **Depth:** panes of glass drifting through deep space.
- **Aurora:** northern lights over a starry night.
- **Aero:** a bright sky, glossy bubbles and white glass.
- **Dot Matrix:** lit dots on a dark grid, dotted headings.
- **Synthwave:** a neon sunset over a racing grid.
- **Paper:** ink on warm paper in a book's typeface.
- **Crystal:** clear glass with rainbow edges, light split by a prism.

### Colors and fonts
- **Colors** (Settings → Interface → Colors) for most themes: Sapphire,
  Indigo, Spice, Emerald, Platinum, Jet, Crystal, Pearl, Ruby, Rose, Gold,
  Lime, Ocean, Sunset and Midnight Violet.
- **Font** (Settings → Interface → Font): any font in any theme: Nunito,
  M PLUS 1 (close to the GameCube's own menu lettering), Nunito + Doto,
  JetBrains Mono, JetBrains Mono + Doto, VT323, Exo 2 and Porpoise Serif.
- Choosing a theme brings its own colors and font; change either after.

### Library views
Settings → Interface → Library view, in every theme:
- **Cover flow**, as before.
- **Wheel:** the boxes stand around a turning wheel.
- **Disc flow:** your discs; the chosen one slides out of its box and spins.
- **Shelf:** rows of boxes, many at once; up and down change row.
- **Box:** one game at a time as a black plastic case with its paper insert
  on the front, spine and back. Turn it with the right stick. Covers
  downloaded from now on keep their spine.
- **List:** your games by name beside the chosen box.
- **Stack:** a deck of boxes.
- **Helix:** the boxes climbing round a column.
- **Revolution's home screen** is Tiles (the default) or Library view.

### Memory Cards views
Settings → Interface → Memory Cards view:
- **Cards**, as before.
- **Blocks:** one card at a time, drawn as a card, with its saves and what
  fills it.
- **By game:** every save on both cards and the Wii, grouped by game.
- **Cubes:** each save a little glass cube on a grid.

### Accessibility
A new section in Settings:
- **Text size:** Normal, Large or Larger.
- **Color filter** for color blindness (red-weak, green-weak, blue-weak) or
  grayscale, for the menus and, if you like, the game's picture too.
- **High contrast:** solid panels, clearer edges, brighter text.
- **Reduced motion**, **Still background** and **Larger button hints**.

### A fresh start
- **The first start** offers three looks to begin with: Porpoise, Star Cube
  or Revolution, shown live as you choose. Then the welcome and the setup
  checklist.
- **Settings → Interface → Reinitialize Porpoise** starts over as if it were
  the first time: every setting, each game's own settings too, back to how
  Porpoise ships. Your games, folders, memory cards, saves and save states
  stay.

### Languages
- The menus are now in US English (Favorite, Color, Center).
- Every new text in all sixteen languages.

## 1.5.1 — 2026-10-04

### Getting out of the sandbox
- **More reliable sandbox escape.** When Porpoise starts inside the app
  sandbox and can't see `/data` or USB drives, it now keeps asking a jailbreak
  daemon to free it for a few seconds, instead of giving up after one try. This
  targets the "worked the first time, then couldn't read /data on later
  launches" reports, where the daemon starts a moment after Porpoise or the
  first attempt loses a timing race.
- **Works with more daemons.** Porpoise now sends every request name the
  daemons in use watch for: etaHEN's and OnionHEN's, and both the classic and
  the newer owned-root **Lapy** daemon (it prepares itself the way the newer
  Lapy daemon requires, before starting any threads).
- **Clearer help.** The "can't reach /data" message now says exactly what to
  change: in etaHEN, turn on *Legacy Command Server* (etaHEN's built-in app list
  can't take Porpoise); in OnionHEN, add `PPSA99764` to `exact_title_ids` in its
  `config.ini`; or run a Lapy daemon.

### External drives
- USB drives, extended storage and the usual game folders are now searched
  **four folders deep** (two before), so games sorted into subfolders on a
  drive are found without adding the folder by hand.
- The README has step-by-step instructions for playing from a USB or external
  drive.

## 1.5 — 2026-10-04

### Revolution theme (Settings → Interface → Theme)
- A bright home screen of game tiles, twelve to a page: page arrows at the
  sides and L2 / R2 turn the page, and a floor along the bottom keeps a
  segmented clock, the date, and round buttons for Settings and the memory
  cards. Porpoise's mark and the tabs sit on top.
- **Point with the controller:** a hand pointer (drawn by Ruben) that opens
  over empty space and points over anything you can choose, leans as you twist
  the controller, with a tap of rumble and a tick as it lands on something.
  The D-pad works as well; the touch pad turns pointing on and off; R3
  centers it.
- **Wii discs' own tiles and banners:** Porpoise reads each Wii disc's banner
  (.iso, .rvz, .wia, .wbfs, .ciso, .gcz) and plays it — animated tiles on the
  home screen, the full banner and its jingle when a tile opens. GameCube games
  keep their covers.
- **Opened tile:** Cross on a tile grows it to fill the screen and opens it,
  with **Wii Controls** (or Game Settings) and **Start** below, save states,
  save data and favorite; Triangle turns between the banner and the game's
  facts. Circle shrinks it back into its tile.
- Every other screen goes white with it: Memory Cards, Settings, the Wii Remote
  setup, the guide, dialogs and the in-game menu. A cover flow layout is there
  too (Interface → Home screen).
- Switching to it shows the Wii games and the Wii saves; GameCube games are
  one press away in Sort & filter. Switching back to Porpoise shows every game
  and the GameCube cards.
- Buttons and prompts read in Title Case (in English).

### Wii Remote on the DualSense (beta)
- Wii games get a Wii controller: **Remote + Nunchuk** (the default),
  **Remote**, **Remote sideways**, **Classic Controller**, or **two
  controllers** (alpha: a second DualSense is the Nunchuk).
- **Point at the screen** with the gyro (hold R1 a moment to center), the touch
  pad or the right stick. The DualSense's motion is the Remote's (tilt, swing,
  point) and a quick flick shakes it. The pointer is smoothed against hand
  tremor without lagging on a sweep.
- **Wii Remote setup:** pick the controller, center, then point at two corners
  of your screen, so the pointer matches your TV and how far you sit. It can
  come up before each Wii game (Triangle skips it); Simple and Advanced, with
  four named presets.
- **Grip:** Auto reads how you hold the DualSense (flat in both hands, or stood
  on end in either hand), or choose one yourself.
- **How to hold it:** a page that draws the DualSense as each Wii controller,
  with what every button does. In a Wii game, the in-game menu can
  recalibrate the pointer, open the setup or switch presets.

### Memory Cards
- **Wii saves:** L2 / R2 switches between the GameCube cards and the Wii saves
  Dolphin keeps, each with its own banner, icon and name. Square backs one up,
  Triangle deletes it.

### Faster and lighter
- Banners and covers stop working in the background the moment a game starts,
  so the game has the console to itself.
- Animated tiles are drawn a frame at a time into textures streamed to the
  GPU, and covers and banners off screen give their memory back.

### Also
- **Sort & filter:** All / GameCube / Wii, and "Get covers and info now".
- Covers arrive for games added while a download is running.
- **Custom texture packs** load now (the core was told the console had no
  memory to spare for them).
- The updater shows "Finishing up" with a spinner while it installs.

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
- The DualSense **light bar** shows each player's color.

### Picture
- **Nine screen filters:** Smooth, Sharp, Sharpen, CRT, Arcade CRT, VHS,
  **Soft VHS** (soft, faded color and glow, no glitches), **8-bit** and
  **Pocket** (a green handheld screen), with a strength setting.
- **Eight borders** for 4:3 games: **Porpoise** (the logo and name), Porpoise
  glass, Midnight, **Frost**, **Carbon**, and three cabinets — Arcade, **Retro**
  and **Synthwave** — plus your own PNGs in `/data/porpoise/borders/`. Borders
  now draw at full resolution.

### Library
- **Favorites** (Options): a gold star on the box, and a *Favorites first*
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
