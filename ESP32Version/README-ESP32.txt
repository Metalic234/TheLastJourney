THE LAST JOURNEY — Pocket Edition (ESP32 / TFT)
===============================================

A small C version of the game for microcontrollers with a colour TFT.
It runs on an ESP32 with no operating system, no browser and no network:
the engine draws into a 320x180 RGB565 framebuffer and pushes it to the
display about 30 times a second.

WHAT IT IS (AND IS NOT)
-----------------------
This is the pocket edition, not a byte-for-byte port of the desktop game.
It keeps the shape of the story — three roads, memory shards to collect,
spiders on the path, a death that restarts the stage, and an ending — but
it runs with simplified art, no bosses, no voyage and no sound. The full
game stays on Android, Windows and the web.

CONTROLS
--------
  LEFT / RIGHT   walk
  JUMP           jump (hold does nothing here; it is a plain jump)
  SHARD          throw a shard. Spiders die in one hit; touching one costs
                 a heart.
  Collect all four memories on a road, then reach its end to continue.
  Falling into a gap costs the whole run of that road.

THE MEMORY PEDDLER
------------------
At the end of every road a stall opens before the next one: the peddler
sells power and takes memories, so what she carries into the next road is
her choice. The pocket edition has no bosses, so the stall stands in for
the boss gate of the bigger versions.

  LEFT / RIGHT   choose a ware
  C              buy the chosen ware (memories are spent, not borrowed)
  X              leave the stall and take the next road

  EXTRA HEART    2 memories   one more heart, up to five, and she is mended
  SWIFT BOOTS    1 memory     she runs faster on every road after this
  LONG SHARDS    1 memory     her shards carry further before they fall
  LEAVE THE STALL             walk on

Memories are a purse: each road's four pickups add to it, and only the
stall spends it.

HARDWARE
--------
  - ESP32 dev board (classic ESP32 or ESP32-S3 both work; PSRAM helps)
  - ILI9341 320x240 SPI TFT (ST7796 480x320 also works with a different
    rotation and offset)
  - four momentary buttons to GND

Wiring used by the sketch:

  TFT (via TFT_eSPI User_Setup.h)     Buttons
  ------------------------------      -----------------
  SCK   -> GPIO 18                    LEFT   -> GPIO 32
  MOSI  -> GPIO 23                    RIGHT  -> GPIO 33
  CS    -> GPIO 15                    JUMP   -> GPIO 25
  DC    -> GPIO 2                     SHARD  -> GPIO 26
  RST   -> GPIO 4                     (other leg of each button -> GND)
  BL    -> 3V3 (or a transistor)

Change the pins in TheLastJourney.ino to match your board.

SOFTWARE
--------
  - Arduino IDE with the "ESP32 by Espressif" board package
  - TFT_eSPI library (Bodmer), configured in its User_Setup.h for your
    panel, or with the User_Setup_Select.h approach

Open ESP32Version/TheLastJourney/TheLastJourney.ino, pick your board and
upload. The sketch folder already contains game.c and game.h, which the
IDE compiles along with it.

MEMORY AND SPEED
----------------
The framebuffer is 320 x 180 x 2 bytes = 115,200 bytes of static RAM.
On a classic ESP32 that is a large but workable slice; keep Wi-Fi and
Bluetooth off in your sketch, and prefer a board with PSRAM if you add
anything else. If RAM is tight, shrink GAME_W/GAME_H in game.h and scale
the image when pushing it, or switch the panel offset.

At 40 MHz SPI the display update is roughly 25 ms, so the game is capped
at 30 fps in the sketch. Lower FRAME_TIME to taste.

TRY IT WITHOUT HARDWARE
-----------------------
The engine is plain C99 with no Arduino dependencies. A host harness runs
it on a desktop, plays a scripted run and writes PPM frames:

  cd ESP32Version
  ./build-host.sh            # needs cc; writes ./frames/frame_*.ppm

That is how this port was tested: it compiles warning-free with
-Wall -Wextra, the scripted runner clears all three roads and reaches the
ending (frame dumps show the title cards, the pits, the spiders, the
memories, the burning city and THE END).

FILES
-----
  TheLastJourney/game.c        engine, levels, physics, drawing
  TheLastJourney/game.h        public API and framebuffer size
  TheLastJourney/TheLastJourney.ino   ESP32 + TFT_eSPI wrapper
  host/main_host.c             desktop harness and frame dumper
  build-host.sh                builds and runs the harness
