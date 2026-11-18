THE LAST JOURNEY — Windows Edition (v1.9)
=========================================

A standalone Windows app: one .exe that opens the game in its own native
window. No browser, no address bar, no internet connection, no install.

WHAT'S NEW IN v1.9
------------------
- THE POWERS NO LONGER COST MEMORIES. The stones are gone; Blue and Brown
  leave her double jump and dash, and the snow giant leaves her glide.
  Every memory she finds can go to the peddler now.
- NEW WARE — AUTOMATIC BOT KILLER (2 memories): the two guardians kill any
  bot that comes near her by themselves. Never a boss.
- Same game as the web and Android 1.9 builds.

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
  Shift or J            dash (appears once unlocked)
  X                     white shards (once unlocked)
  C                     light beam (hold; once unlocked)
  P or Escape           pause
  M                     mute / unmute
  R                     restart (while paused)

Click the window once if the keyboard seems unresponsive — that also
satisfies Windows' rule that audio may only start after an interaction.

What's included: the same game as Android v1.9 — all levels and biomes,
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
