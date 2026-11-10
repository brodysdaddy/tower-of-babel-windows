# Tower of Babel (Windows prototype)

A two-button stacking game written in portable C++. It's prototyped on Windows first and meant to run later on an **M5StickS3** handheld (ESP32-S3, 135×240 screen, two buttons), alongside my [Bible Trivia Stick](https://github.com/brodysdaddy/bible-trivia-stick) game.

A brick layer slides back and forth above the tower, and you press to drop it. Whatever hangs past the layer below is sliced off and tumbles away, so the tower gets narrower. Land a layer (almost) exactly on top for a **PERFECT**, and string perfects together to grow the tower back. Miss completely and the tower is finished.

## Features

- **Sky that changes with height:** sunset at the base, then dusk, then a starry night with a crescent moon. The sun sinks below the horizon as you climb.
- **Parallax layers:** drifting clouds, distant ziggurat silhouettes and desert sand all scroll at different speeds.
- **Bricks that change material every 10 layers:** mud brick, terracotta, glazed blue, turquoise and gold. Mortar joints line up across layers, so a cut slices through bricks naturally.
- **Juice:** tumbling cut-off pieces, dust puffs, gold sparkles and a white flash on perfects, a camera that eases upward, and a little screen shake.
- **Rising combo chime:** each perfect in a row plays one semitone higher.
- **High score**, a pause screen, and a game-over summary.

## Architecture

```
src/
  core/tower_game.*      Rules only: sliding, cutting, perfects, speed-up
  gfx/framebuffer.*      135x240 RGB565 pixel buffer, shapes, 5x7 font
  app/tower_app.*        Screens, visuals, effects, sounds (as data)
  app/platform.h         The 3 functions a platform must provide
  windows/main_win.cpp   Window, keyboard, 30 fps loop, high-score file
  windows/sound_win.cpp  Non-blocking Beep() on a background thread
```

Everything except `src/windows` is portable C++ with no heap allocation and no platform code:

- **The game draws into its own 135×240 buffer**, the exact size of the stick's screen in portrait, using the stick's native 16-bit color format (RGB565). Windows just scales that buffer up 3× in a window. The stick will push the same buffer straight to its display. What you see on Windows is pixel-for-pixel what the stick will show.
- **The keyboard stands in for the two buttons**, so the controls already match the hardware.
- **Sound is non-blocking.** Sounds are lists of (frequency, duration) notes. Windows plays them on a background thread, and the stick will use its speaker's built-in tone generator. The game never freezes while a sound plays.
- **It runs at 30 fps**, the target frame rate for the stick.

## Controls

| Key | Stick button | Action |
|---|---|---|
| Space / Enter / Up | Front | Start, drop a layer, play again |
| P / Tab / Down | Side | Pause / resume, back to the title |
| M | — | Sound on/off |
| Esc | — | Quit |

The game also pauses automatically when the window loses focus.

## Building

Requires Windows and either **Visual Studio** (with "Desktop development with C++") or **MinGW/MSYS2 g++**.

```bat
build.bat
tower.exe
```

`build.bat` finds Visual Studio automatically, so it works from a normal Command Prompt; a Developer Command Prompt isn't needed. It links only standard Windows libraries (`user32`, `gdi32`, `winmm`), so there's nothing else to install.

## Tuning

The gameplay knobs are at the top of `src/core/tower_game.h`:

- starting width
- perfect tolerance
- growth on combos
- starting speed, speed-up per layer, and max speed

Sky colors, brick materials and sounds are data tables at the top of `src/app/tower_app.cpp`.

## Next steps

- **Port to the M5StickS3:** add a stick front end (buttons, display, speaker, flash for the high score) next to the trivia game.
- **Game selector:** a menu on the stick to choose Trivia or Tower of Babel. Each choice would have its own background and style, with an animated transition between them.
- **More polish:** bricks for more tiers, wind gusts at high altitude, and a "zoom out" view of the whole tower at game over.
