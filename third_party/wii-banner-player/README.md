# Wii Banner Player (reference)

Porpoise's `src/porpoise_banner.cpp` plays a Wii disc's banner (`opening.bnr`:
its brlyt layouts, brlan animations, TPL textures and BNS sound) following the
Wii Banner Player Project's player, https://github.com/jordan-woyak/wii-banner-player
(zlib licence, LICENSE.txt here). It is an altered version, written again for
Porpoise: a software renderer with its own TEV, the GX alpha test and blend
modes, texture-pattern (flipbook) keys and konst colours, and no OpenGL, SFML
or Boost. No file of the original is copied into Porpoise.
