// On-screen touch controls for Android (and any touch device).
// Builds a translucent button overlay that drives the game through the
// window.__gameInput bridge installed by src/input.js.
//
// build-apk.sh copies this file over assets/www/touch.js in the base APK, so
// changes here reach the installed game without a new base APK.

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

  // ---- build DOM -----------------------------------------------------------
  const root = document.createElement('div');
  root.id = 'tj-controls';

  // movement pad (left)
  const pad = document.createElement('div');
  pad.className = 'tj-pad';
  const mk = (cls, html) => {
    const b = document.createElement('div');
    b.className = 'tj-btn ' + cls;
    b.innerHTML = html;
    return b;
  };
  const left = mk('tj-left', '&#9664;');
  const right = mk('tj-right', '&#9654;');
  bindHold(left, 'left');
  bindHold(right, 'right');
  pad.appendChild(left);
  pad.appendChild(right);

  // action cluster (right)
  const acts = document.createElement('div');
  acts.className = 'tj-acts';
  const jump = mk('tj-round tj-jump', '&#9650;');      // jump / glide (hold)
  const use = mk('tj-round tj-use', 'E');               // interact
  const dash = mk('tj-round tj-pw tj-dash', '&#187;');  // dash, or hold to run
  const shard = mk('tj-round tj-pw tj-shard', 'X'); // white shards
  const laser = mk('tj-round tj-pw tj-laser', 'C');  // beam (hold)
  bindHold(jump, 'up');
  bindTap(use, 'interact');
  // hold to run from the first step, tap to dash once the power is awake:
  // both use the same held action, so this has to be a hold button.
  bindHold(dash, 'dash');
  bindTap(shard, 'shard');
  bindHold(laser, 'laser');
  acts.appendChild(jump);
  acts.appendChild(use);
  acts.appendChild(dash);
  acts.appendChild(shard);
  acts.appendChild(laser);

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
  // Running is hers from the first step: the dash button is always on screen
  // because holding it walks her into a sprint whether or not Ember Dash has
  // been awakened yet. X (shards) and C (beam) are only ever used inside the
  // two big battles (guardian knights = game.bossBattle, wilds = game.combat),
  // so those two appear exactly then.
  const game = () => window.__game;
  function refresh() {
    const g = game();
    const inBattle = !!(g && (g.bossBattle || g.combat));
    dash.classList.remove('tj-hidden');
    shard.classList.toggle('tj-hidden', !inBattle);
    laser.classList.toggle('tj-hidden', !inBattle);
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
