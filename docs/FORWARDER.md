# Starting a game from a home screen forwarder

A forwarder is a separate small app with its own home screen tile (its own title ID, icon and
name). When it is opened, it launches Porpoise (`PPSA99764`) with launch arguments, and Porpoise
starts that game directly instead of opening its library.

## Arguments

| Argument | Meaning |
| --- | --- |
| `--rom <file>` or `--rom=<file>` | The game to start. An absolute path (`/mnt/usb0/games/Game.rvz`), or a path inside Porpoise's `games/` folder in its data folder (`Game.rvz`, `Wii/Game.wbfs`). A relative path may not contain `..`. |
| `--exit-after-game` | When that game ends normally (back to the library from the in-game menu), close Porpoise so the console returns to the home screen. Without it, the library opens. |

Unknown arguments are ignored. Parsing is in `src/porpoise_forward.hpp`; `src/porpoise_main.cpp`
uses it after the jailbreak daemon has freed Porpoise and its data folder has been chosen, so
`games/` resolves against the folder Porpoise is using (`/data/porpoise/games`, a drive's, or
`/app0/porpoise/games` when sandboxed).

Any file Porpoise can play works, including one outside the folders the library searches: it is
read and added to the library for this session (`Library::open_file`), with its play time,
favourite and settings kept like any other game's.

## Behaviour

- The forwarded game starts once, with the game's own settings and quick resume as usual; the
  Resume and Wii setup questions the library asks are skipped. Quitting it opens the library, or,
  with `--exit-after-game`, closes Porpoise. Quitting to the home screen closes Porpoise either way.
- If the game doesn't start, the library opens with the usual "This game didn't start" message,
  even with `--exit-after-game`.
- If the file is missing (or Porpoise is in the app sandbox, so the path can't be read), the
  library opens and shows **Forwarded game not found** with the path. When Porpoise is sandboxed,
  the sandbox message is shown instead, since it says why.
- Arguments only reach a new Porpoise process. If Porpoise is already running, the system brings
  it to the front and `main` does not run again; a forwarder should close it first.
