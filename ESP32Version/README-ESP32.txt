THE LAST JOURNEY — Pocket Edition (ESP32 / TFT)
===============================================

A small C version of the game for microcontrollers with a colour TFT.
It runs on an ESP32 with no operating system, no browser and no network:
the engine draws into a 320x180 RGB565 framebuffer and pushes it to the
display about 30 times a second.

WHAT IT IS (AND IS NOT)
-----------------------
This is the pocket edition, not a byte-for-byte port of the desktop game.
It keeps the shape of the story — four roads, memory shards to collect,
spiders on the path, a keeper at the end of the last three roads (three of
them hold the last), a death that restarts the stage, and an ending — but it
runs with simplified art,
no voyage and no sound. The full game stays on Android, Windows and the
web.

CONTROLS
--------
  LEFT / RIGHT   walk
  JUMP           jump (hold does nothing here; it is a plain jump)
  SHARD          throw a shard. Spiders die in one hit; touching one costs
                 a heart. She throws down an empty road too: the shard flies
                 the way she faces and burns out by itself.
                 HOLD SHARD while walking and she runs: the sprint
                 spends her breath (the thin bar under the hearts) and it
                 comes back while she walks. While a keeper is standing she
                 throws in BOTH directions, so the aim is on it either way.
  Collect all four memories on a road, then reach its end to continue.
  Falling into a gap costs the whole run of that road.

  LEFT + RIGHT       open the menu. It holds RESUME, RESTART THIS ROAD and
                     BEGIN AGAIN, which is what a title screen would do:
                     a road restarts from its first step with her breath
                     whole and everything she has bought still hers, and
                     the stall waiting at the start of it. There is no
                     sound on this board, so there is no SOUND row.
                     In the menu LEFT / RIGHT choose, C takes the row and
                     JUMP steps back out.

THE KEEPERS
-----------
The last three roads end in a fight. The keeper holds the end of the road
and cannot be walked past.

  THE TIGER OF THE RUINS   ends the drowned ruins. It stalks her on two
                           legs, swipes when close, and leaps at her.
  THE WATER DRAGON         ends the burning city. The last road ends in
                           nothing but cliff and one plank bridge across a
                           single dark cut: no water, no bays, no wells at
                           either end of the span. The dragon keeps to the
                           planks, stands at one end of the deck, leaps the
                           whole span, and comes down on the boards at the
                           far end. It sees her from the approach, long
                           before she reaches its chest.
                           Every third landing a Steam Blast runs the span
                           and takes a heart — unless she is in the air:
                           it rolls low along the planks, so a jump clears
                           it.
  THE VERDANT WILDS        ends in three keepers, one after another, all
                           in the same glade: the Forest Fairy, then the
                           Love Goddess, then the Ocean Rage God. Killing
                           one does not open the road; the next steps out
                           of the green as the one before it falls, and
                           their weather is wiped with them.

  THE FOREST FAIRY         first of the three. She
                           hovers out of reach and lets the wood fight for
                           her:
                             POISONED ARROWS   six of them come out of the
                                               canopy over wherever she
                                               stands. Standing still is
                                               what gets her hit: keep
                                               moving and they fall behind.
                             A FIREBALL        rolls along the ground after
                                               her and takes a heart unless
                                               she is in the air — jump it,
                                               the way the dragon's blast
                                               is jumped.
                             SHADOW CLONES     two dark copies of her stand
                                               beside her and each looses
                                               an arrow every second and a
                                               half. Only the bright one
                                               takes a shard.
                           Her bar is longer than the first two keepers':
                           twelve hits against their nine.

  THE LOVE GODDESS         second of the three. She hovers just off the
                           ground, crowned and pink, and throws:
                             SHOOTING LOVE     three hearts in a fan, aimed
                                               at wherever she stands. Move,
                                               and the fan misses behind her.
                             A POISONED KISS   one kiss, thrown underhand,
                                               so it arcs and comes down on
                                               her mark. Its vapour is green.
                           Fourteen hits.

  THE OCEAN RAGE GOD       last of the three, and the longest bar of the
                           game at sixteen hits. He stands on the road with
                           a coral crown and a trident, and calls:
                             THE GREAT WAVE    one wall of water runs the
                                               glade low. It rolls along the
                                               ground: a jump clears it.
                             THE TORNADO       a spinning column stands where
                                               she was a moment ago and takes
                                               a heart every second she stays
                                               in it. Running out of it is the
                                               answer; it cannot be jumped.
                             THE RAIN OF FISHES six fish fall out of the
                                               sky across the whole glade.
                                               Keep moving and they land
                                               behind her.

Shards wear a keeper down; its health bar sits at the top of the screen.
Each of them is worth a payout from the road's soul purse, and the road
only opens once the last keeper of it is down.

THE SOUL PEDDLER
----------------
At the FIRST of every road a stall stands, and its window opens before she
takes a step: souls in, power out.

  LEFT / RIGHT   choose a ware
  C              buy the chosen ware (souls are spent, not borrowed)
  X              leave the stall and take the road

  A WARD OF HER OWN     50 souls  a shield bar above her hearts. It takes
                                  the first blow of the run; a new road does
                                  not mend it, and only the stall fills it.
                                  Once broken it is sold back at the price.
  FIRE IN HER X         50 souls  shards bite 15 percent harder and set
                                  the keeper burning: 2 percent a second.
  ICE IN HER X          50 souls  shards bite 14 percent harder and the
                                  cold slows whatever they touch.
  ELECTRIC IN HER X     50 souls  shards bite 18 percent harder and the
                                  charge jumps to another bot nearby.
  AUTOMATIC GUARDIAN   150 souls  two shadows kill any spider that comes
                                  near her, without a button. Bought once.
  REFILL HER HEALTH     10 souls  her hearts fill to the brim again.
                                  Buy it whenever she is hurt.
  LEAVE THE STALL                 walk on

Any two elements mix (fire + ice = hot mist, fire + electric = plasma,
ice + electric = storm); all three vaporize, with shock and mist at once.
Each mix has a look of its own in her X: a pale cloud with the ember still
in it, the violet bolt sheathed in flame, cold lightning with a crystal on
it, and all three at once. One element on its own looks as it always did.
Her hearts run purple at full, then blue, green, yellow, orange and red,
and go black when there is nothing left.

Souls come off the spiders she puts down (the guardian's kills pay too) and
they never vanish between roads. Each road pays 35 to 80 souls in all, split
across the spiders standing on it, so no road is a jackpot. One kill in two
hundred is a lone hundred instead, and spends that road's purse at once. Her hearts do NOT refill at the start of
a new road — only the stall mends her. A retry after a fall starts that road
with a whole heart bar.

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

The fourth road did not need a bigger board. As `size` reports it on the
host build, static state is 123,392 bytes of .bss — 115,200 of them the
framebuffer — plus 868 bytes of initialised data. The whole game, every
road and all
five keepers ask for 288 bytes more than the three-road build did (the
wilds' stage record, eight falling arrows, one fireball and two shadow
clones), and the Love Goddess and the Ocean Rage God add 416 bytes on top of
that: twelve shared bolts for hearts, kisses and fish, one wave, one storm
and the little they need to gather a spell. Their attacks were trimmed to
the board: three hearts at a time, one kiss, six fish, one wave and one
tornado, which is what a 320x180 screen can show and what 416 bytes can
hold.

TRY IT WITHOUT HARDWARE
-----------------------
The engine is plain C99 with no Arduino dependencies. A host harness runs
it on a desktop, plays a scripted run and writes PPM frames:

  cd ESP32Version
  ./build-host.sh            # needs cc; writes ./frames/frame_*.ppm

That is how this port was tested: it compiles warning-free with
-Wall -Wextra, the scripted runner clears all four roads — the tiger, the
dragon and the three keepers of the wilds among them, in order — and
reaches the ending (frame dumps show the title cards, the pits, the
spiders, the memories, the burning city, the wilds and THE END).

Each keeper was also watched on its own in the glade, with no shards
thrown: the hearts fan, the arcing kiss, the rolling wave, the tornado and
the rain of fishes all appeared on the board, and each of them took a heart
from her where it caught her.

FILES
-----
  TheLastJourney/game.c        engine, levels, physics, drawing
  TheLastJourney/game.h        public API and framebuffer size
  TheLastJourney/TheLastJourney.ino   ESP32 + TFT_eSPI wrapper
  host/main_host.c             desktop harness and frame dumper
  build-host.sh                builds and runs the harness
