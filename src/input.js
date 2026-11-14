// Keyboard input mapped to game actions, with edge-triggered presses.

const MAP = {
  ArrowLeft: 'left',
  KeyA: 'left',
  ArrowRight: 'right',
  KeyD: 'right',
  ArrowUp: 'up',
  KeyW: 'up',
  Space: 'up',
  ArrowDown: 'down',
  KeyS: 'down',
  KeyE: 'interact',
  Enter: 'interact',
  Escape: 'pause',
  KeyP: 'pause',
  KeyM: 'mute',
  KeyR: 'restart',
  ShiftLeft: 'dash',
  ShiftRight: 'dash',
  KeyJ: 'dash',
  KeyX: 'shard',
  KeyC: 'laser',
};

const BLOCK = new Set(['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Space']);

export class Input {
  constructor(target = window) {
    this.downSet = new Set();
    this.pressedSet = new Set();
    this.anyPress = false;
    this.anyPressAt = 0;

    target.addEventListener('keydown', (e) => {
      if (BLOCK.has(e.code)) e.preventDefault();
      const action = MAP[e.code];
      this.anyPress = true;
      this.anyPressAt = performance.now();
      if (!action) return;
      if (!e.repeat) this.pressedSet.add(action);
      this.downSet.add(action);
    });

    target.addEventListener('keyup', (e) => {
      const action = MAP[e.code];
      if (action) this.downSet.delete(action);
    });

    window.addEventListener('blur', () => {
      this.downSet.clear();
    });

    // Bridge for the on-screen touch controls shipped inside the Android
    // WebView build (assets/www/touch.js). It waits for this object before
    // drawing its buttons, so it has to exist for every Input instance.
    window.__gameInput = {
      set: (action, down) => this.setAction(action, down),
      tapAny: () => this.tapAny(),
    };
  }

  // Press or release an action from a pointer/button source. A press is
  // edge-triggered like a key press, so held buttons keep reporting down().
  setAction(action, down) {
    if (down) {
      if (!this.downSet.has(action)) this.pressedSet.add(action);
      this.downSet.add(action);
      this.anyPress = true;
      this.anyPressAt = performance.now();
    } else {
      this.downSet.delete(action);
    }
  }

  // A tap on the canvas: counts as "any key" for title and ending screens.
  tapAny() {
    this.anyPress = true;
    this.anyPressAt = performance.now();
  }

  down(action) {
    return this.downSet.has(action);
  }

  pressed(action) {
    return this.pressedSet.has(action);
  }

  consume(action) {
    const had = this.pressedSet.has(action);
    this.pressedSet.delete(action);
    return had;
  }

  consumeAny() {
    const had = this.anyPress;
    this.anyPress = false;
    return had;
  }

  endFrame() {
    this.pressedSet.clear();
    this.anyPress = false;
  }
}
