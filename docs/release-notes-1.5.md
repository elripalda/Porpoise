<p align="center"><img src="https://github.com/elripalda/Porpoise/raw/main/docs/images/banner.png" alt="Porpoise" width="100%"></p>

**Porpoise 1.5.** A brand-new theme called **Revolution**, Wii games that play their own animated banners, the DualSense as a Wii Remote, and Wii saves in Memory Cards.

## What's new

### Revolution: a new theme
Turn it on in **Settings → Interface → Theme**.
- A bright, clean home screen of game tiles, twelve to a page, with a segmented clock and the date along the bottom and round buttons for Settings and Memory Cards. Arrows at the sides (or L2 / R2) turn the page.
- **Point with the controller:** a hand pointer that follows the DualSense. It opens over empty space, points over anything you can pick, and leans as you twist the controller, with a tick and a tap of rumble as it lands. The D-pad works too, the touch pad turns pointing on and off, and R3 re-centres it.
- **Wii games play their own banners.** Porpoise reads each Wii disc's own tile and banner (`.iso`, `.rvz`, `.wia`, `.wbfs`, `.ciso`, `.gcz`) and plays it animated on the home screen. GameCube games show their covers.
- **Opening a game:** Cross grows its tile to fill the screen. The full banner plays with its jingle, with **Wii Controls** (or **Game Settings**) and **Start** below, plus save states, save data and favourite. Triangle switches to the game's info, and Circle shrinks it back into its tile. Square on a tile starts the game straight away.
- The whole app goes white with it: Memory Cards, Settings, the Wii Remote setup, dialogs and the in-game menu.
- Prefer the cover flow? *Interface → Home screen* puts it in the Revolution theme too.
- Switching to Revolution shows your Wii games and Wii saves. Switching back to Porpoise shows every game and the GameCube cards.

### The DualSense as a Wii Remote (beta)
- Every Wii game gets a Wii controller: **Remote + Nunchuk** (the default), **Remote**, **Remote sideways**, **Classic Controller**, or **two controllers** (alpha: a second DualSense is the Nunchuk).
- **Point at the screen** with the gyro (hold R1 a moment to centre it), the touch pad or the right stick. Tilt, swing and point like a real Remote, and flick to shake. The pointer is smoothed against hand shake without lagging behind.
- **Wii Remote setup:** pick the controller, centre it, then point at two corners of your TV, so the pointer lands where you point from where you sit. It can come up before each Wii game (Triangle skips it), with Simple and Advanced modes and four presets.
- **Grip:** Auto works out how you're holding the DualSense (flat in both hands, or on end in either hand), or pick one yourself.
- **How to hold it:** a page in Settings → Wii Remote that draws the DualSense as each Wii controller, with every button labelled.
- In a Wii game, the in-game menu can recalibrate the pointer, open the setup or switch presets.

### Memory Cards
- **Wii saves:** L2 / R2 switches between the GameCube cards and your Wii saves, each with its own banner, icon and name. **Square** backs a save up and **Triangle** deletes it.

### Faster and lighter
- Banners and covers stop all background work the moment a game starts, so the game gets the console to itself.
- Animated tiles are streamed straight to the GPU a frame at a time, and covers and banners that are off screen give their memory back.

### Also new
- **Sort & filter:** show All, GameCube or Wii games, and *Get covers and info now* to fetch missing art.
- **Custom texture packs** now load.
- Covers arrive for games added while a download is running.
- English buttons and prompts now use Title Case.
- The updater shows *Finishing up* with a spinner while it installs.

## Install or update

**From 1.1:** open **Settings → About → Updates** and install 1.5 from there.

**By hand or new install:**
1. Download **`Porpoise-1.5.zip`** below and unzip it. You get a folder named **`PPSA99764`**.
2. If you're updating, delete `/data/homebrew/PPSA99764/` on the PS5 first. Your games, saves, covers and settings live in `/data/porpoise/` and are kept.
3. Copy the **`PPSA99764`** folder into **`/data/homebrew/`** over FTP or with PS5 Upload.
4. For a new install, register it like your other homebrew (for example with ShadowMountPlus).

You need a jailbroken PS5 with **etaHEN** and **kstuff**. More in the [README](https://github.com/elripalda/Porpoise#readme), the [changelog](https://github.com/elripalda/Porpoise/blob/main/CHANGELOG.md) and the [credits](https://github.com/elripalda/Porpoise/blob/main/CREDITS.md).

## Thanks

**[Mihawk (mihawk-99)](https://github.com/mihawk-99)**, whose PS5 RetroArch, PS5_Vulkan and PS5_Mesa ports Porpoise is built on. Wii banners are read and played following the **[Wii Banner Player Project](https://github.com/jordan-woyak/wii-banner-player)** (zlib), and `.rvz` discs with the **[Zstandard](https://github.com/facebook/zstd)** educational decoder. Controller art and button icons: *PS5 Button Icons and Controls* by **[Zacksly](https://zacksly.itch.io)** ([@_Zacksly](https://twitter.com/_Zacksly)), CC BY 3.0, adapted for Porpoise.

## Legal

Porpoise contains **no games and no BIOS or firmware files**. The only key it carries is the Wii disc key Dolphin itself includes, used to read a Wii disc's own banner. Play only games you own, from backups you made yourself. Porpoise is not affiliated with Nintendo or Sony. Emulation is provided by [Dolphin](https://dolphin-emu.org) (GPL-2.0-or-later). Porpoise is GPL-3.0-or-later; every component's licence and source revision is listed in `licenses/README.txt` inside the zip.

`Porpoise-1.5.zip` SHA-256: `e5768829410213382c89fe31720c42e18a9c5d480ca2dd1cae52af475e8b1f08`
