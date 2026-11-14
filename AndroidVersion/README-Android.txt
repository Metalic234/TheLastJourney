THE LAST JOURNEY — Android Edition (v1.4)
==========================================

WHAT'S NEW IN v1.4
------------------
SIGNED WITH A PERMANENT RELEASE KEY
- The APK used to be signed with a throwaway debug key (v1.0–v1.3). It is
  now signed with a real release key: 4096-bit RSA, validity to 2054.
- Every future build signed with this same key installs straight over the
  current app. No uninstall each time, and your saved progress stays.
- The keystore lives in signing/ (see SIGNING below). Back it up.

UPDATED TO THE CURRENT ANDROID
- Now targets Android 17 (API level 37), the current Android release, and
  compiles against SDK 37. Minimum is still Android 5.0 (API 21).
- Game bundle rebuilt from the current source; gameplay is unchanged.

IMPORTANT — ONE-TIME UNINSTALL
------------------------------
v1.0–v1.3 were signed with a debug key, and Android only installs an update
when the signature matches. Installing v1.4 over v1.3 therefore says "App
not installed". Uninstall the old copy once, install v1.4, and from then on
every update installs in place.

WHAT'S NEW IN v1.3
------------------
SOUND OVERHAUL
- A mastering compressor now sits over everything: no more harsh crackle
  when several sounds stack at once.
- The old buzzy "blast" is rebuilt as a warm layered whoosh; noise
  swells, footsteps and the metal riff were de-fizzed with proper
  filtering. Everything is cleaner and softer on the ears.
BATTLE BEHAVIOR (as requested)
- BLUE BOSS: physical blade strikes + long-range volleys of blue shards
  that arc across the street. The old ground-mark "storm" is gone.
- BROWN BOSS: fights physically now — he plants his feet and swings a
  LONG BROWN MACE (studded head, long handle, drawn in his hand), with
  a readable windup. The old sky-slam is gone.
- DOGS: physical bites only (unchanged — they were already melee).
- SCORPION: physical claw swipes and tail stings only (unchanged).

WHAT'S NEW IN v1.2
------------------
- FIXED: the X (white shards) and C (guardian beam) buttons were missing.
  They now appear automatically when a battle starts and disappear after
  it — matching the original design where X and C are only used inside
  the two big battles (the guardian knights, and the dogs + scorpion).
- DASH still appears once you awaken Ember Dash on your journey.

WHAT'S NEW IN v1.1
------------------
- FIXED: black screen on first launch (the game engine now ships as a
  single bundled script that every Android WebView can execute).
- The FULL game is included — all levels and biomes (forest, ruins,
  city, cliffs...), the dash / leap / glide / shard / beam powers,
  boss encounters, the whale voyage, and the ending. Nothing was cut.

INSTALL (sideload, ~2 minutes)
------------------------------
0. Coming from v1.3 or older: uninstall it first (one time only, see
   above). Coming from v1.4 or later: just install, it updates in place.
1. Copy TheLastJourney.apk onto your Android phone or tablet
   (USB, Bluetooth, cloud drive, or download it directly).
2. Tap the APK file. If asked, allow "Install from unknown sources"
   for the app you used to open it (Files / Chrome).
3. Tap Install. That's it — "The Last Journey" appears in your app
   drawer with its own icon.

REQUIREMENTS
------------
- Android 5.0 (Lollipop) or newer (ARM & x86, phones and tablets).
- Targets Android 17 (API 37), so it runs cleanly on current phones.
- No internet permission, no ads, fully offline, ~95 KB.
- Android 17 ignores orientation locks on large screens, so on tablets
  and in desktop mode the window may be any shape; the game scales to fit.

BUILDING A NEW APK (scripted)
-----------------------------
build-apk.sh does the whole release in one command:

  cd AndroidVersion
  ./build-apk.sh                 # keep the version name, versionCode + 1
  ./build-apk.sh -n 1.5 -c 7     # set the version explicitly

It bundles src/ with esbuild, rebuilds the APK with apktool, aligns it,
signs it with the release keystore, checks the certificate and writes
AndroidVersion/TheLastJourney.apk. Use -o <file> to write the result
somewhere else, or -t <file> to build from a different base APK.

It needs JDK 17+, Android build-tools with apksigner and zipalign (37.x),
apktool, and Node with esbuild. If they are not on PATH, point at them:

  APKTOOL_JAR=/path/to/apktool.jar \
  ANDROID_HOME=/path/to/android-sdk \
  ESBUILD=/path/to/esbuild \
  ./build-apk.sh

Files:
  signing/thelastjourney-release.jks   release keystore (alias: lastjourney)
  signing/keystore.properties          its passwords, read by the script

Keep both and never regenerate the keystore: a new key means a new
signature, and everyone would have to uninstall again. Keep this repository
private, and keep a backup of the .jks somewhere safe. The script refuses
to finish if the signing certificate is not the expected one:
  32:7F:31:2B:E0:DE:A7:CB:80:50:99:17:31:24:42:BD:26:DE:BF:AE:99:13:02:32:73:C0:DC:72:EB:EE:6E:F6

This build: versionName 1.4, versionCode 5, package com.zai.lastjourney.

HOW TO PLAY ON TOUCH
--------------------
The game auto-scales to your screen in landscape. On-screen controls:

  Bottom-left:   <  >   walk left / right
  Bottom-right:  ^      jump (hold in mid-air to GLIDE once learned)
                 E      interact / talk / collect / continue
                 >>     dash         (appears once unlocked)
                 *      white shards (appears once unlocked)
                 ~      light beam   (hold; appears once unlocked)
  Top-right:     ||     pause        (RESTART button appears while paused)
                 note   mute / unmute

- Title screen: tap anywhere to begin.
- Text notes: tap E to skip typing, tap E again to close.
- Hardware BACK button = pause. Press BACK twice quickly to exit.

TIP: a keyboard also works if you plug one in (arrows/WASD, E, J, X, C,
P, M, R) — the desktop layout is untouched.

TROUBLESHOOTING
---------------
- "App not installed" coming from v1.3 or older: that is the signature
  change — uninstall the old copy once, then install again. From v1.4 on,
  updates install over the app without uninstalling.
- Still a black screen? Make sure you uninstalled the old copy first, then
  install again. If "Could not start the game engine..." appears, update
  "Android System WebView" in the Play Store and relaunch.
- No sound on first launch: tap once anywhere; Android requires one
  interaction before audio can start.
- If controls feel small on a tablet, they are always in the same
  corners — muscle memory kicks in fast.

Enjoy the journey.
