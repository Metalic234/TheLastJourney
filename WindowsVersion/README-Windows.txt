THE LAST JOURNEY — Windows Edition (v2.2.1)
===========================================

WHAT'S NEW IN v2.2.1
--------------------
- THE LAST BRIDGE NOW CROSSES A CUT. The sea waits at either end of the
  span and the middle is open air, so the planks read as a bridge again
  instead of a jetty laid across flat water. The dragon still rides the
  water at both ends and never stands on the deck.
- THE WARD BAR IS HERS ALONE. Her shield bar no longer appears until the
  peddler has sold her the ward; before that there is only her life bar.
- THE SHOP'S PERCENT TEXT READS CORRECTLY, and the peddler's window fits
  every line.

WHAT'S NEW IN v2.2.0
--------------------
- THE DRAGON'S BRIDGE IS ONE UNBROKEN DECK. The last span runs the whole
  cut from cliff to cliff, so there is no hole in the planks to fall
  through mid-fight. The sea is still in two parts, one at each end.
- THE STEAM BLAST CAN BE JUMPED. Every third landing now, not every
  second, 9 instead of 13, and it rolls low: jump and it goes under her.
  A warning line marks the moment it gathers.
- A WARD FOR HER, 50 souls: a shield bar above her life bar, taking the
  first blow of every road and coming back at each stage.
- HER LIFE BAR RUNS PURPLE, BLUE, GREEN, YELLOW, ORANGE, RED — and black
  and empty when she is gone.
- FIRE, ICE AND ELECTRIC IN HER X, 50 souls each: +15 percent and burning,
  +14 percent and slowing, +18 percent and a charge that jumps. Any two
  mix; all three vaporize with shock and mist.
- Same release key / save-free install as v2.1.1: version 2.2.1.

WHAT'S NEW IN v2.1.1
--------------------
- THE WATER DRAGON LEAPS THE SEA, NOT THE BRIDGE. The last span stands over
  open water now: the sea runs under the bridge and out past both of its
  ends, so the bridge cuts the water in two. The dragon rides the water on
  one side, leaps the whole span, and lands with a splash in the water on
  the other — never on the planks. The Steam Blast still comes after every
  second landing, and now it runs the full span.
- Same release key / save-free install as v2.1.0: version 2.1.1.

WHAT'S NEW IN v2.1.0
--------------------
- LONGER ROADS, MORE OF EVERYTHING. The stages with bots grew by a quarter
  to a half and carry two or three more: more spiders in the forest, more
  black dogs in the deserts, more crabs on the cliffs.
- TWO NEW BOSSES. A GIANT HUMANOID TIGER holds the end of the ruined city
  — it stalks her, swipes, and leaps at her feet — and THE WATER DRAGON
  waits past the last cliffs on a new bridge, vaulting from side to side
  and breathing a Steam Blast after every second landing that costs her an
  eighth of her health. The dragon's bar is the scorpion's.
- BOSS AIM LOCK. While a boss stands, X shards and the knights' C beam
  turn on it from every direction: run and face anywhere.
- FOREST SPIDERS CAN BE CUT with X and C like any other bot, and the
  guardians no longer strike them when they bite her.
- Same release key / save-free install as v2.0.1: version 2.1.0.

WHAT'S NEW IN v2.0.1
--------------------
- SOULS ARE PAID OUT PER ROAD, NOT PER KILL: every stage pays 35 to 80
  souls in all, split across the memories and bots on it. One pickup in
  two hundred is still a lone hundred.
- THE OPENING HINT LINE NOW TEACHES RUNNING: HOLD SHIFT TO RUN, RUNNING
  SPENDS HER BREATH. Running is hers from the first step; only the dash
  waits for Blue and Brown.

WHAT'S NEW IN v2.0
------------------
- MEMORIES ARE SOULS NOW: every bot she puts down and every memory she
  picks up pays souls, 35 to 80 per road.
- STAMINA AND RUNNING: hold Shift to sprint at nearly twice her walk. It
  spends her breath, drawn under the souls counter; the dash is free.
- THE SHOP IS ON EVERY ROAD, at the start of all eight stages.
- HER HEALTH CARRIES BETWEEN STAGES. Only the peddler's refill (10 souls)
  fills the bar again; a retry after death starts with a whole bar.
- THE WARES: AUTOMATIC GUARDIAN (150 souls, once) and REFILL HER HEALTH
  (10 souls, repeatable).
- Same game as the web and Android 2.0 builds.

A standalone Windows app: one .exe that opens the game in its own native
window. No browser, no address bar, no internet connection, no install.

WHAT'S NEW IN v1.9
------------------
- THE POWERS NO LONGER COST MEMORIES. The stones are gone; Blue and Brown
  leave her double jump and dash, and the snow giant leaves her glide.
  Every memory she finds can go to the peddler now.
- NEW WARE — AUTOMATIC BOT KILLER (2 memories): the two guardians kill any
  bot that comes near her by themselves. Never a boss.
- Same game as the web and Android 1.9 builds (superseded by v2.0).

WHAT'S NEW IN v1.8
------------------
- THE MEMORY PEDDLER: after every road that ends in a boss, a stall waits
  on the way out. Walk up to it and the window opens: memories in, power
  out. A/D chooses, E buys, Esc closes.
  Wares: A SPARE HEART, SHARD EDGE, KNIGHTS' FOCUS, QUICK BOOTS.
- Same game as the web and Android builds.

PLAY
----
1. Copy TheLastJourney.exe anywhere you like (Desktop, USB stick, a games
   folder). It is a single file; nothing else is needed.
2. Double-click it. Windows SmartScreen will warn about an unknown app
   because the file is not code-signed: click "More info" -> "Run anyway".
3. The game opens in a 960x540 window and scales when you resize it.

Requirements: Windows 10 or 11, 64-bit, with the Microsoft Edge WebView2
runtime. Windows 10 (1803 and newer) and Windows 11 include it already.
If the app does not start, install "Microsoft Edge WebView2 Runtime" from
Microsoft's site and try again.

CONTROLS
--------
  Left/Right or A/D     walk
  Up / W / Space        jump (hold in mid-air to glide once learned)
  Down / S              duck
  E or Enter            interact / talk / collect / continue
  Shift or J            hold to run (from the first step); tap to dash
                        once Blue and Brown have awakened it
  X                     white shards (once unlocked)
  C                     light beam (hold; once unlocked)
  P or Escape           pause
  M                     mute / unmute
  R                     restart (while paused)

Click the window once if the keyboard seems unresponsive — that also
satisfies Windows' rule that audio may only start after an interaction.

What's included: the same game as Android v2.0 — all levels and biomes,
the powers, both boss battles, the voyage and the ending. The light beam
ignites like a lightsaber, the shards ring like thrown shuriken, and the
score turns to heavy metal once the two bosses are down.

BUILDING IT YOURSELF
--------------------
  cd WindowsVersion
  ./build-exe.sh

The script bundles src/ with esbuild, fetches the Neutralino framework
binaries on its first run, then produces TheLastJourney.exe with the whole
game embedded. It needs Node (npx) and internet access the first time; no
Android SDK or Java is involved.

Files:
  resources/               game page, styles, script and window icon
  neutralino.config.json   window title, size, version, build settings
  build-exe.sh             rebuilds the executable
  TheLastJourney.exe       the built app (~2.8 MB, game embedded)

NOTES
-----
- The executable is unsigned, so SmartScreen and some antivirus tools may
  warn. A code-signing certificate removes that warning; there is none here.
- The window is powered by Edge WebView2, the same rendering engine Windows
  uses for its own apps. It is not a browser window and never touches the
  network for the game.
