// On-screen touch controls for Android (and any touch device).
// Builds a translucent button overlay that drives the game through the
// window.__gameInput bridge installed by src/input.js.
//
// The hands are split down the middle: the left thumb walks, runs and jumps,
// the right thumb fights. Every button is a rounded, lit key with a one-word
// caption, so nothing on the glass has to be guessed at.
//
// build-apk.sh copies this file (and touch.css) over assets/www in the base
// APK, so changes here reach the installed game without a new base APK.

(function () {
  'use strict';

  function init() {
    if (!window.__gameInput) {
      // input.js runs inside a deferred module; wait until the bridge exists.
      setTimeout(init, 50);
      return;
    }
    if (window.__touchButtons) return;

    const held = new Set();

    function releaseAll() {
      for (const a of Array.from(held)) {
        window.__gameInput.set(a, false);
        held.delete(a);
      }
      document.querySelectorAll('.tj-btn.active').forEach((b) => b.classList.remove('active'));
    }
    window.__touchButtons = { releaseAll };

    function bindHold(el, action) {
      const press = (e) => {
        e.preventDefault();
        if (held.has(action)) return;
        held.add(action);
        window.__gameInput.set(action, true);
        el.classList.add('active');
        if (el.setPointerCapture && e.pointerId !== undefined) {
          try { el.setPointerCapture(e.pointerId); } catch (err) { /* ignore */ }
        }
      };
      const release = (e) => {
        if (e) e.preventDefault();
        if (!held.has(action)) return;
        held.delete(action);
        window.__gameInput.set(action, false);
        el.classList.remove('active');
      };
      el.addEventListener('pointerdown', press);
      el.addEventListener('pointerup', release);
      el.addEventListener('pointercancel', release);
      el.addEventListener('lostpointercapture', release);
      el.addEventListener('contextmenu', (e) => e.preventDefault());
    }

    function bindTap(el, action) {
      el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        el.classList.add('active');
        window.__gameInput.set(action, true);
      });
      const end = (e) => {
        if (e) e.preventDefault();
        el.classList.remove('active');
        window.__gameInput.set(action, false);
      };
      el.addEventListener('pointerup', end);
      el.addEventListener('pointercancel', end);
      el.addEventListener('pointerleave', end);
      el.addEventListener('contextmenu', (e) => e.preventDefault());
    }

    // ---- build DOM ---------------------------------------------------------
    const root = document.createElement('div');
    root.id = 'tj-controls';

    // A glyph, and - where the glyph alone is a guess - one caption under it.
    const mk = (cls, glyph, caption) => {
      const b = document.createElement('div');
      b.className = 'tj-btn ' + cls;
      const g = document.createElement('span');
      g.className = 'tj-glyph';
      g.innerHTML = glyph;
      b.appendChild(g);
      if (caption) {
        const c = document.createElement('span');
        c.className = 'tj-cap';
        c.textContent = caption;
        b.appendChild(c);
      }
      return b;
    };

    // LEFT THUMB: walking, running, jumping.
    // A wide JUMP bar sits over the two directions; RUN (hold to sprint, tap
    // to dash once she has it) is beside them, under the thumb that leaves
    // the arrows.
    const pad = document.createElement('div');
    pad.className = 'tj-pad';
    const jump = mk('tj-jump', '&#9650;', 'JUMP');
    const left = mk('tj-walk tj-left', '&#9664;');
    const right = mk('tj-walk tj-right', '&#9654;');
    const dash = mk('tj-run tj-dash', '&#187;', 'RUN');
    bindHold(jump, 'up');
    bindHold(left, 'left');
    bindHold(right, 'right');
    // Hold to run from the first step, tap to dash once the power is awake:
    // both use the same held action, so this has to be a hold button.
    bindHold(dash, 'dash');
    pad.appendChild(jump);
    pad.appendChild(left);
    pad.appendChild(right);
    pad.appendChild(dash);

    // RIGHT THUMB: fighting, with E in the corner of the same hand.
    // X throws the volley and C holds the knights' beam; the two of them stack
    // under the E button, nearest the thumb.
    const acts = document.createElement('div');
    acts.className = 'tj-acts';
    const fight = document.createElement('div');
    fight.className = 'tj-fight';
    const shard = mk('tj-fight-key tj-shard', 'X', 'SHARD');
    const laser = mk('tj-fight-key tj-laser', 'C', 'BEAM');
    const use = mk('tj-use', 'E', 'USE');
    bindTap(shard, 'shard');
    bindHold(laser, 'laser');
    bindTap(use, 'interact');
    fight.appendChild(laser);
    fight.appendChild(shard);
    // E sits inboard of the fight column, so X stays under the resting thumb.
    acts.appendChild(use);
    acts.appendChild(fight);

    // system buttons (top right)
    const sys = document.createElement('div');
    sys.className = 'tj-sys';
    const pause = mk('tj-sysbtn', '&#10073;&#10073;');
    const mute = mk('tj-sysbtn', '&#9835;');
    const restart = mk('tj-sysbtn tj-restart', '&#8635;');
    bindTap(pause, 'pause');
    bindTap(mute, 'mute');
    bindTap(restart, 'restart');
    restart.classList.add('tj-hidden');
    sys.appendChild(restart);
    sys.appendChild(mute);
    sys.appendChild(pause);

    root.appendChild(pad);
    root.appendChild(acts);
    root.appendChild(sys);
    document.body.appendChild(root);

    // tapping anywhere outside buttons advances title / ending screens
    window.addEventListener('pointerdown', (e) => {
      if (e.target === root || root.contains(e.target)) return;
      if (window.__gameInput) window.__gameInput.tapAny();
    }, { passive: true });

    // ---- power-aware visibility ---------------------------------------------
    // Running is hers from the first step: the RUN button is always on screen
    // because holding it walks her into a sprint whether or not Ember Dash has
    // been awakened yet. X (shards) and C (beam) are only ever used inside the
    // two big battles (guardian knights = game.bossBattle, wilds = game.combat),
    // so the whole fight stack appears exactly then and E sits alone in the
    // corner on every quiet road.
    const game = () => window.__game;
    function refresh() {
      const g = game();
      const inBattle = !!(g && (g.bossBattle || g.combat));
      fight.classList.toggle('tj-hidden', !inBattle);
      const stopped = !g || g.mode === 'title' || g.mode === 'ending';
      const showRestart = !!(g && !stopped && (g.paused || g.mode === 'ending'));
      restart.classList.toggle('tj-hidden', !showRestart);
      root.classList.toggle('tj-title', stopped);
    }
    setInterval(refresh, 120);
    refresh();
  }
  init();
})();
