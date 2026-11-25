// The soul peddler: the stall that waits where every road begins and sells
// power for souls. Catalogue and window only — the game owns the currency and
// the effects, so a new item needs one entry here plus a case in applyUpgrade().

import { W, H } from './util.js';
import { drawText, wrapText } from './font.js';

export const SHOP_ITEMS = [
  {
    id: 'shield',
    name: 'A WARD OF HER OWN',
    cost: 50,
    // Once owned, the ward can be bought back every time it has been spent,
    // the way her health can be refilled.
    max: 999,
    color: '#8fd8ff',
    desc: 'A SHIELD BAR SITS ABOVE HER LIFE AND TAKES THE FIRST BLOW OF EVERY ROAD. WHEN IT IS SPENT, THIS STALL FILLS IT AGAIN: THE ROAD NEVER WILL.',
    bought: 'THE AIR CLOSES AROUND HER SHOULDERS.',
  },
  {
    id: 'fire',
    name: 'FIRE IN HER X',
    cost: 50,
    max: 1,
    color: '#ff7a3c',
    desc: 'HER X SHARDS BITE 15 PERCENT HARDER AND SET THE FOE ALIGHT: 2 PERCENT OF ITS LIFE EVERY SECOND.',
    bought: 'THE SHARDS LEAVE HER HAND ALREADY BURNING.',
  },
  {
    id: 'ice',
    name: 'ICE IN HER X',
    cost: 50,
    max: 1,
    color: '#9fd8ff',
    desc: 'HER X SHARDS BITE 14 PERCENT HARDER, AND THE COLD SLOWS WHATEVER THEY TOUCH.',
    bought: 'EVERY SHARD LEAVES A RIME WHERE IT LANDS.',
  },
  {
    id: 'elec',
    name: 'ELECTRIC IN HER X',
    cost: 50,
    max: 1,
    color: '#c9a6ff',
    desc: 'HER X SHARDS BITE 18 PERCENT HARDER, AND THE CHARGE JUMPS TO ANOTHER FOE CLOSE BY.',
    bought: 'THE AIR SNAPS AROUND HER FINGERS.',
  },
  {
    id: 'warden',
    name: 'AUTOMATIC GUARDIAN',
    cost: 150,
    max: 1,
    color: '#a9c8ff',
    desc: 'THE TWO GUARDIANS STRIKE ANY BOT THAT COMES NEAR HER — DOGS, SPIDERS, CRABS. NEVER A BOSS.',
    bought: 'THE SHADOWS DO NOT WAIT TO BE ASKED. ANYTHING THAT CRAWLS TOO CLOSE IS ALREADY GONE.',
  },
  {
    id: 'heal',
    name: 'REFILL HER HEALTH',
    cost: 10,
    max: 999,
    color: '#ff8a9a',
    desc: 'HER LIFE BAR FILLS TO THE BRIM AGAIN. THE ROAD DOES NOT MEND HER — THIS DOES.',
    bought: 'THE PEDDLER KNOCKS ON THE COUNTER: ONE MORE BREATH, ALL OF IT.',
  },
];

export function drawShopOverlay(g, game) {
  const s = game.shop;
  if (!s) return;

  const px = 44;
  const py = 16;
  const pw = W - 88;
  const ph = 238;

  g.save();
  g.fillStyle = 'rgba(3,3,8,0.82)';
  g.fillRect(0, 0, W, H);

  g.fillStyle = 'rgba(20,15,28,0.96)';
  g.fillRect(px, py, pw, ph);
  g.fillStyle = 'rgba(190,170,220,0.32)';
  g.fillRect(px, py, pw, 1);
  g.fillRect(px, py + ph - 1, pw, 1);
  g.fillRect(px, py, 1, ph);
  g.fillRect(px + pw - 1, py, 1, ph);

  drawText(g, 'THE SOUL PEDDLER', W / 2, py + 8, {
    align: 'center', color: '#efe6f4', scale: 1, spacing: 2, shadow: '#000000',
  });
  drawText(g, `SOULS  ${game.souls}`, W / 2, py + 20, {
    align: 'center', color: '#ffd9a0', scale: 1, spacing: 1,
  });

  const rowH = 20;
  const top = py + 36;
  const rows = SHOP_ITEMS.length + 1;

  for (let i = 0; i < rows; i++) {
    const y = top + i * rowH;
    const item = SHOP_ITEMS[i];
    const sel = i === s.index;

    if (sel) {
      g.fillStyle = 'rgba(150,120,190,0.22)';
      g.fillRect(px + 8, y - 6, pw - 16, rowH - 4);
      // the pixel font has no arrow: draw the marker
      g.fillStyle = '#ffd9a0';
      g.beginPath();
      g.moveTo(px + 15, y - 1);
      g.lineTo(px + 15, y + 4);
      g.lineTo(px + 20, y + 1.5);
      g.closePath();
      g.fill();
    }

    if (!item) {
      drawText(g, 'LEAVE THE STALL', px + 26, y, {
        color: sel ? '#ffffff' : '#b9b2c8', scale: 1, spacing: 1,
      });
      drawText(g, 'WALK ON', px + pw - 14, y, { align: 'right', color: '#8a8296', scale: 1 });
      continue;
    }

    const owned = game.upgradeCount(item.id);
    const soldOut = owned >= item.max;
    const whole = !soldOut && item.id === 'shield' && game.wardIsWhole();
    const afford = game.souls >= item.cost;
    const nameCol = soldOut || whole ? '#6b6478' : afford ? item.color : '#8a8296';

    drawText(g, item.name, px + 26, y, { color: nameCol, scale: 1, spacing: 1 });

    drawText(g, soldOut ? 'OWNED' : whole ? 'WHOLE' : `${item.cost} SOULS`, px + pw - 14, y, {
      align: 'right',
      color: soldOut || whole ? '#6b6478' : afford ? '#ffd9a0' : '#8a8296',
      scale: 1,
    });
  }

  const chosen = SHOP_ITEMS[s.index];
  const desc = chosen ? chosen.desc : 'SHE TAKES HER SOULS WITH HER.';
  let dy = top + rows * rowH + 4;
  for (const line of wrapText(desc, pw - 30, 1, 1)) {
    drawText(g, line, W / 2, dy, { align: 'center', color: '#c9c1d6', scale: 1, spacing: 1 });
    dy += 11;
  }

  // The elements are meant to be worn together.
  drawText(g, 'TWO MIX: HOT MIST / PLASMA / STORM. ALL THREE VAPORIZE.', W / 2, py + ph - 34, {
    align: 'center', color: '#9fd8d0', scale: 1, spacing: 1,
  });

  if (s.notice) {
    drawText(g, s.notice.text, W / 2, py + ph - 22, {
      align: 'center', color: s.notice.color, scale: 1, spacing: 1, shadow: '#000000',
    });
  }

  drawText(g, 'A / D  CHOOSE     E  BUY     ESC  CLOSE', W / 2, py + ph - 10, {
    align: 'center', color: '#8d869f', scale: 1, spacing: 1,
  });
  g.restore();
}
