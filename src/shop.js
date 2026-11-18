// The memory peddler: the stall that opens after a boss and sells power for
// memories. Catalogue and window only — the game owns the currency and the
// effects, so a new item needs one entry here plus a case in applyUpgrade().

import { W, H } from './util.js';
import { drawText, wrapText } from './font.js';

export const SHOP_ITEMS = [
  {
    id: 'warden',
    name: 'AUTOMATIC BOT KILLER',
    cost: 2,
    max: 1,
    color: '#a9c8ff',
    desc: 'THE TWO GUARDIANS STRIKE ANY BOT THAT COMES NEAR HER — DOGS, SPIDERS, CRABS. NEVER A BOSS.',
    bought: 'THE SHADOWS DO NOT WAIT TO BE ASKED. ANYTHING THAT CRAWLS TOO CLOSE IS ALREADY GONE.',
  },
  {
    id: 'heart',
    name: 'A SPARE HEART',
    cost: 2,
    max: 2,
    color: '#ff8a9a',
    desc: 'HER LIFE BAR GROWS BY A QUARTER AND FILLS TO THE NEW BRIM.',
    bought: 'IT IS HERS. THE PEDDLER KNOCKS ON THE COUNTER: ONE MORE BEAT.',
  },
  {
    id: 'shard',
    name: 'SHARD EDGE',
    cost: 1,
    max: 2,
    color: '#cfeaff',
    desc: 'EVERY WHITE SHARD SHE THROWS CUTS ONE POINT DEEPER.',
    bought: 'THE EDGES CATCH THE LIGHT NOW. GOOD.',
  },
  {
    id: 'beam',
    name: "KNIGHTS' FOCUS",
    cost: 2,
    max: 2,
    color: '#9fd0ff',
    desc: 'THE GUARDIANS POUR MORE OF THEMSELVES INTO THE BEAM.',
    bought: 'THE TWO SHADOWS LEAN IN. THE LIGHT RUNS HOTTER.',
  },
  {
    id: 'boots',
    name: 'QUICK BOOTS',
    cost: 1,
    max: 1,
    color: '#ffd166',
    desc: 'SHE RUNS FASTER FOR THE REST OF THE ROAD.',
    bought: 'THE ROAD WILL FEEL SHORTER.',
  },
];

export function drawShopOverlay(g, game) {
  const s = game.shop;
  if (!s) return;

  const px = 64;
  const py = 24;
  const pw = W - 128;
  const ph = 222;

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

  drawText(g, 'THE MEMORY PEDDLER', W / 2, py + 9, {
    align: 'center', color: '#efe6f4', scale: 1, spacing: 2, shadow: '#000000',
  });
  drawText(g, `MEMORIES  ${game.memory}`, W / 2, py + 21, {
    align: 'center', color: '#d8c9a8', scale: 1, spacing: 1,
  });

  const rowH = 19;
  const top = py + 34;
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
    const afford = game.memory >= item.cost;
    const nameCol = soldOut ? '#6b6478' : afford ? item.color : '#8a8296';

    drawText(g, item.name, px + 26, y, { color: nameCol, scale: 1, spacing: 1 });

    if (owned > 0 && !soldOut) {
      drawText(g, `x${owned}`, px + pw - 60, y, {
        align: 'right', color: '#9a92aa', scale: 1,
      });
    }
    drawText(g, soldOut ? 'OWNED' : `${item.cost} MEM`, px + pw - 14, y, {
      align: 'right', color: soldOut ? '#6b6478' : afford ? '#ffd9a0' : '#8a8296', scale: 1,
    });
  }

  const chosen = SHOP_ITEMS[s.index];
  const desc = chosen ? chosen.desc : 'SHE TAKES HER MEMORIES WITH HER.';
  let dy = top + rows * rowH + 4;
  for (const line of wrapText(desc, pw - 30, 1, 1)) {
    drawText(g, line, W / 2, dy, { align: 'center', color: '#c9c1d6', scale: 1, spacing: 1 });
    dy += 11;
  }

  if (s.notice) {
    drawText(g, s.notice.text, W / 2, py + ph - 24, {
      align: 'center', color: s.notice.color, scale: 1, spacing: 1, shadow: '#000000',
    });
  }

  drawText(g, 'A / D  CHOOSE     E  BUY     ESC  CLOSE', W / 2, py + ph - 11, {
    align: 'center', color: '#8d869f', scale: 1, spacing: 1,
  });
  g.restore();
}
