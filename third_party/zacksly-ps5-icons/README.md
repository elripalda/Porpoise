# PS5 Button Icons and Controls (Zacksly)

**PS5 Button Icons and Controls** by **Zacksly** (https://zacksly.itch.io,
https://twitter.com/_Zacksly), licensed under
[CC BY 3.0](https://creativecommons.org/licenses/by/3.0/). See `LICENSE.txt`.

These are the original SVGs Porpoise uses, unchanged. Porpoise doesn't ship
them as they are: `tools/make-controller-art.py` draws the app's PNGs from
them, and in doing so **modifies** them as follows:

- `assets/ui/buttons.png` puts the outline and solid button icons in one atlas
  at a small size.
- `assets/ui/dualsense.png` lays the outline controller over a faint copy of
  the solid one, makes its lines a little thicker, and removes the
  PlayStation logo.
- `assets/ui/controller-lines.png` is the "Controls" schematic without its
  button badges, D-pad and face-button icons, or the PlayStation logo. Porpoise
  draws its own labels at the ends of the lines.

Only the files Porpoise uses are kept here; the full pack is on Zacksly's
itch.io page.
