/*
 * The Last Journey — Pocket Edition
 *
 * A small, self-contained C port of the game for microcontrollers with a
 * colour TFT (ESP32 and friends). It keeps the shape of the story: run the
 * three roads, gather the memories, and reach the end. Rendering goes to a
 * 320x180 RGB565 framebuffer that is pushed to the display once per frame.
 *
 * No dynamic allocation, no floating point display code, no libc drawing.
 */

#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define W GAME_W
#define H GAME_H
#define MAX_SPIDERS 10
#define MAX_SHARDS 10
#define MAX_MEM 4
#define GND_STEP 2
#define GND_MAX 1600

static uint16_t fb[W * H];

/* ---- palette ----------------------------------------------------------- */
static uint16_t to565(uint32_t c) {
  return (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
}

typedef struct {
  float x0, x1;
} Gap;

typedef struct {
  const char *name;
  float length;
  int gap_count;
  Gap gaps[4];
  uint32_t sky_top, sky_bot, hill_far, hill_near, dirt, grass, stone;
  /* The last road ends on a bridge over the sea; the span cuts the water in
   * two and the keeper rides either half of it. 0/0 = no bridge. */
  float bridge_x0, bridge_x1;
} StageCfg;

static const StageCfg STAGES[3] = {
  {
    "THE ENCHANTED FOREST", 2100.0f, 2,
    { { 640, 686 }, { 1080, 1126 } },
    0x101A33, 0x51708F, 0x18301F, 0x24452C, 0x2A1E14, 0x4C7A3A, 0x1B120C,
    0.0f, 0.0f
  },
  {
    "THE DROWNED RUINS", 2400.0f, 3,
    { { 520, 566 }, { 900, 946 }, { 1320, 1366 } },
    0x1C1226, 0x7A4A5A, 0x261E33, 0x392C46, 0x2E2836, 0x67607A, 0x1A1620,
    0.0f, 0.0f
  },
  {
    "THE BURNING CITY", 2700.0f, 4,
    { { 430, 476 }, { 760, 806 }, { 1150, 1196 }, { 1560, 1606 } },
    0x160B10, 0x8A3A26, 0x24161A, 0x3B2020, 0x2A1A18, 0x8A4A2A, 0x140C0A,
    2050.0f, 2270.0f
  },
};

/* ---- level data -------------------------------------------------------- */
static int16_t gnd[GND_MAX];
static int8_t hill_far[GND_MAX];
static int8_t hill_near[GND_MAX];
static int gnd_n;
static int stage_i;
static float stage_len;
/* the bridge over the last road's sea */
#define SEA_W 44.0f
static float bridge_x0, bridge_x1, arena_y, sea_y;
static int has_bridge;

static uint16_t sky_row[H];
static uint16_t col_hill_far, col_hill_near, col_dirt, col_grass, col_stone;

/* ---- entities ---------------------------------------------------------- */
typedef struct {
  float x, y, vx, vy;
  int on_ground, facing, hp, invuln;
  float shoot_cd, anim;
  float stamina;   /* her breath: running spends it */
  int winded;
} Player;

typedef struct {
  float x, home;
  int dir, alive;
} Spider;

typedef struct {
  float x;
  int y, taken;
} Memory;

typedef struct {
  float x, y, vx;
  int live;
  float life;
} Shard;

static Player p;
static Spider spiders[MAX_SPIDERS];
static int spider_n;
static Memory mems[MAX_MEM];
static int mem_n, mem_taken;
static Shard shards[MAX_SHARDS];

/* ---- the two keepers -------------------------------------------------- */
/* Stage one ends with the tiger of the ruins, the last road with the water
 * dragon. 0 = no boss on the road. */
static const int BOSS_KIND[3] = { 0, 1, 2 };

typedef struct {
  int kind;              /* 1 tiger, 2 dragon */
  float x, home, base_y;
  int hp, max_hp, alive, active, facing;
  float intro, cd, claw_t, pounce_t, pounce_x;
  float hop_t, hop_from, hop_to, lift, steam_t, steam_burst;
  int hops;
  float flash;
} Boss;

static Boss boss;

static float cam;
static int state; /* 0 play, 1 clear, 2 dead, 3 end, 4 shop */
static float state_t;
static float banner;
static int finished;

/* ---- the soul peddler -------------------------------------------------- */
/* Souls are the purse: they come off the bots she puts down, they add up
 * across the run, and the stall at the first of every road spends them. */
#define SHOP_ROWS 3
static int souls;
static int purse;      /* souls this road will pay in all: 35-80 */
static int purse_start;/* the purse as it was filled, to spot the lone hundred */
static int purse_left; /* payouts still waiting on the road */
static int max_hp = 3;
static int shop_sel;
static int auto_kill;
static int bots_killed;
static float shop_t;
static float zap_t, zap_x, zap_cd;

static const char *SHOP_NAME[2] = { "AUTOMATIC GUARDIAN", "REFILL HER HEALTH" };
static const int SHOP_COST[2] = { 150, 10 };
static const char *SHOP_DESC[2] = {
  "TWO SHADOWS KILL NEARBY BOTS. NEVER A BOSS.",
  "HER LIFE FILLS TO THE BRIM AGAIN.",
};

/* A road pays 35 to 80 souls in all, split across the spiders standing on it:
 * each kill takes its share of the purse. One road in two hundred is a lone
 * hundred instead, paid in one go at its first kill, and spends the purse. */
static int roll_souls(void) {
  int share, jitter, amount;
  if (purse <= 0) return 0;
  if (purse_left <= 0) purse_left = 1;
  if (purse == purse_start && (rand() % 200) == 0) {
    purse = 0;
    purse_left = 0;
    return 100;
  }
  if (purse_left <= 1) {
    amount = purse; /* the last kill takes whatever is left, to the soul */
  } else {
    share = purse / purse_left;
    jitter = share / 5;
    amount = share + (jitter > 0 ? (rand() % (2 * jitter + 1)) - jitter : 0);
    if (amount < 1) amount = 1;
    if (amount > purse) amount = purse;
  }
  purse -= amount;
  purse_left--;
  return amount;
}

/* ---- helpers ----------------------------------------------------------- */
static void mem_str(char *b, int n);
static void draw_boss(void);

static float clampf(float v, float a, float b) {
  return v < a ? a : (v > b ? b : v);
}

static int gi(float x) {
  int i = (int)(x * (1.0f / GND_STEP));
  if (i < 0) i = 0;
  if (i >= gnd_n) i = gnd_n - 1;
  return i;
}

static float ground_y(float x) {
  return (float)gnd[gi(x)];
}

static void px(int x, int y, uint16_t c) {
  if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) fb[y * W + x] = c;
}

static void rect(int x, int y, int w, int h, uint16_t c) {
  int yy, xx;
  if (y < 0) { h += y; y = 0; }
  if (y + h > H) h = H - y;
  if (x < 0) { w += x; x = 0; }
  if (x + w > W) w = W - x;
  for (yy = y; yy < y + h; yy++) {
    uint16_t *row = &fb[yy * W];
    for (xx = x; xx < x + w; xx++) row[xx] = c;
  }
}

static void disc(int cx, int cy, int r, uint16_t c) {
  int y, x;
  for (y = -r; y <= r; y++) {
    int span = (int)sqrtf((float)(r * r - y * y));
    for (x = -span; x <= span; x++) px(cx + x, cy + y, c);
  }
}

/* ---- 3x5 font ---------------------------------------------------------- */
static const char *FONT[40] = {
  "111101111101101", /* A */
  "110101110101110", /* B */
  "111100100100111", /* C */
  "110101101101110", /* D */
  "111100111100111", /* E */
  "111100111100100", /* F */
  "111100101101111", /* G */
  "101101111101101", /* H */
  "111010010010111", /* I */
  "001001001101111", /* J */
  "101101110101101", /* K */
  "100100100100111", /* L */
  "101111111101101", /* M */
  "101111111111101", /* N */
  "111101101101111", /* O */
  "111101111100100", /* P */
  "111101101111001", /* Q */
  "111101110101101", /* R */
  "111100111001111", /* S */
  "111010010010010", /* T */
  "101101101101111", /* U */
  "101101101101010", /* V */
  "101101111111101", /* W */
  "101101010101101", /* X */
  "101101010010010", /* Y */
  "111001010100111", /* Z */
  "111101101101111", /* 0 */
  "010110010010111", /* 1 */
  "111001111100111", /* 2 */
  "111001111001111", /* 3 */
  "101101111001001", /* 4 */
  "111100111001111", /* 5 */
  "111100111101111", /* 6 */
  "111001001001001", /* 7 */
  "111101111101111", /* 8 */
  "111101111001111", /* 9 */
  "001001010100100", /* / */
  "000010000010000", /* : */
  "000000000000010", /* . */
  "010010010000010", /* ! */
};

static int glyph(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= '0' && c <= '9') return 26 + (c - '0');
  if (c == '/') return 36;
  if (c == ':') return 37;
  if (c == '.') return 38;
  if (c == '!') return 39;
  return -1;
}

static int text_w(const char *s, int scale) {
  int n = (int)strlen(s);
  return n > 0 ? n * 4 * scale - scale : 0;
}

static void text(const char *s, int x, int y, int scale, uint16_t col) {
  int i;
  for (i = 0; s[i]; i++) {
    int g = glyph(s[i]);
    if (g >= 0) {
      int r, c;
      const char *bits = FONT[g];
      for (r = 0; r < 5; r++) {
        for (c = 0; c < 3; c++) {
          if (bits[r * 3 + c] == '1') {
            rect(x + c * scale, y + r * scale, scale, scale, col);
          }
        }
      }
    }
    x += 4 * scale;
  }
}

static void text_center(const char *s, int y, int scale, uint16_t col) {
  text(s, (W - text_w(s, scale)) / 2, y, scale, col);
}

/* ---- world reset ------------------------------------------------------- */
static void reset_stage(int idx) {
  const StageCfg *cfg;
  int i;
  stage_i = idx;
  cfg = &STAGES[idx];
  stage_len = cfg->length;
  gnd_n = (int)(stage_len / GND_STEP) + 1;
  if (gnd_n > GND_MAX) gnd_n = GND_MAX;

  for (i = 0; i < gnd_n; i++) {
    float wx = i * (float)GND_STEP;
    float y = 138.0f + sinf(wx * 0.010f + idx * 1.7f) * 9.0f
                        + sinf(wx * 0.027f + idx) * 3.0f;
    int g;
    for (g = 0; g < cfg->gap_count; g++) {
      if (wx >= cfg->gaps[g].x0 && wx <= cfg->gaps[g].x1) y = 999.0f;
    }
    gnd[i] = (int16_t)y;
    hill_far[i] = (int8_t)(14.0f + sinf(wx * 0.008f + idx) * 7.0f
                                + sinf(wx * 0.019f) * 3.0f);
    hill_near[i] = (int8_t)(10.0f + sinf(wx * 0.013f + idx * 2.0f) * 6.0f);
  }

  bridge_x0 = cfg->bridge_x0;
  bridge_x1 = cfg->bridge_x1;
  has_bridge = bridge_x1 > bridge_x0 + 1.0f;
  arena_y = 0.0f;
  sea_y = 0.0f;
  if (has_bridge) {
    int bx;
    /* The span is level and the sea comes up just under its planks, so the
     * bridge cuts the water in two. Both lips are shallow enough to wade. */
    arena_y = ground_y((bridge_x0 + bridge_x1) * 0.5f);
    sea_y = arena_y + 2.0f;
    for (bx = (int)bridge_x0; bx <= (int)bridge_x1; bx++)
      gnd[gi((float)bx)] = (int16_t)arena_y;
    for (bx = (int)(bridge_x0 - SEA_W); bx < (int)bridge_x0; bx++)
      if (bx >= 0 && bx < gnd_n) gnd[gi((float)bx)] = (int16_t)(arena_y + 8.0f);
    for (bx = (int)bridge_x1 + 1; bx <= (int)(bridge_x1 + SEA_W); bx++)
      if (bx >= 0 && bx < gnd_n) gnd[gi((float)bx)] = (int16_t)(arena_y + 8.0f);
  }

  col_hill_far = to565(cfg->hill_far);
  col_hill_near = to565(cfg->hill_near);
  col_dirt = to565(cfg->dirt);
  col_grass = to565(cfg->grass);
  col_stone = to565(cfg->stone);

  {
    uint16_t top = to565(cfg->sky_top);
    uint16_t bot = to565(cfg->sky_bot);
    int rt = (top >> 11) & 0x1F, gt = (top >> 5) & 0x3F, bt = top & 0x1F;
    int rb = (bot >> 11) & 0x1F, gb = (bot >> 5) & 0x3F, bb = bot & 0x1F;
    for (i = 0; i < H; i++) {
      int r = rt + (rb - rt) * i / H;
      int g = gt + (gb - gt) * i / H;
      int b = bt + (bb - bt) * i / H;
      sky_row[i] = (uint16_t)((r << 11) | (g << 5) | b);
    }
  }

  p.x = 26.0f;
  p.y = ground_y(p.x);
  p.vx = 0;
  p.vy = 0;
  p.on_ground = 1;
  p.facing = 1;
  p.invuln = 0;
  p.shoot_cd = 0;
  p.anim = 0;
  p.stamina = 100.0f;
  p.winded = 0;
  zap_t = 0.0f;
  zap_cd = 0.0f;

  mem_n = MAX_MEM;
  mem_taken = 0;
  {
    static const float frac[4] = { 0.22f, 0.44f, 0.66f, 0.86f };
    for (i = 0; i < MAX_MEM; i++) {
      float mx = stage_len * frac[i];
      if (ground_y(mx) > 300.0f) mx += 70.0f;
      mems[i].x = mx;
      mems[i].y = (int)ground_y(mx) - 12;
      mems[i].taken = 0;
    }
  }

  spider_n = 5 + idx;
  if (spider_n > MAX_SPIDERS) spider_n = MAX_SPIDERS;
  /* the purse for this road, filled before she takes a step */
  purse = 35 + rand() % 46;
  purse_start = purse;
  purse_left = spider_n + (BOSS_KIND[idx] ? 1 : 0);
  {
    static const float sfrac[10] = {
      0.22f, 0.34f, 0.46f, 0.56f, 0.66f, 0.74f, 0.80f, 0.84f, 0.88f, 0.92f
    };
    for (i = 0; i < spider_n; i++) {
      float sx = stage_len * sfrac[i];
      if (ground_y(sx) > 300.0f) sx += 50.0f;
      spiders[i].x = sx;
      spiders[i].home = sx;
      spiders[i].dir = (i & 1) ? -1 : 1;
      spiders[i].alive = 1;
    }
  }
  for (i = 0; i < MAX_SHARDS; i++) shards[i].live = 0;

  boss.kind = BOSS_KIND[idx];
  boss.max_hp = boss.kind ? 9 : 0;
  boss.hp = boss.max_hp;
  boss.alive = boss.kind ? 1 : 0;
  boss.active = 0;
  boss.x = stage_len * 0.80f;
  boss.home = boss.x;
  /* the dragon starts in the sea on the near side of its bridge */
  if (boss.kind == 2 && has_bridge) boss.x = boss.home - 132.0f;
  boss.base_y = 0.0f;
  boss.facing = -1;
  boss.intro = 1.2f;
  boss.cd = 1.5f;
  boss.claw_t = 0.0f;
  boss.pounce_t = 0.0f;
  boss.pounce_x = boss.x;
  boss.hop_t = 0.0f;
  boss.hop_from = boss.x;
  boss.hop_to = boss.x;
  boss.lift = 0.0f;
  boss.steam_t = 0.0f;
  boss.steam_burst = 0.0f;
  boss.hops = 0;
  boss.flash = 0.0f;

  cam = 0;
  /* The peddler is set up where the road begins: the window opens first. */
  state = 4;
  state_t = 0;
  shop_sel = 0;
  shop_t = 0.0f;
  banner = 2.6f;
}

void game_init(void) {
  finished = 0;
  souls = 0;
  max_hp = 3;
  auto_kill = 0;
  bots_killed = 0;
  shop_sel = 0;
  memset(fb, 0, sizeof(fb));
  reset_stage(0);
  p.hp = max_hp;
}

/* ---- rules ------------------------------------------------------------- */
static void hurt(int dmg) {
#ifdef GAME_INVINCIBLE
  (void)dmg;
  return;
#endif
  if (p.invuln > 0 || state != 0) return;
  p.hp -= dmg;
  p.invuln = 1.2f;
  p.vy = -70.0f;
  p.vx = -p.facing * 40.0f;
  if (p.hp <= 0) {
    state = 2;
    state_t = 1.4f;
  }
}

static void shoot(void) {
  /* While a keeper stands her aim is already on it: the shards leave her in
   * both directions, so whichever way she runs the boss is still in reach. */
  int dirs = (boss.alive && boss.active) ? 2 : 1;
  int d, i;
  for (d = 0; d < dirs; d++) {
    float dir = d == 0 ? (float)p.facing : -(float)p.facing;
    for (i = 0; i < MAX_SHARDS; i++) {
      if (!shards[i].live) {
        shards[i].x = p.x + dir * 7.0f;
        shards[i].y = p.y - 11.0f;
        shards[i].vx = dir * 250.0f;
        shards[i].life = 1.6f;
        shards[i].live = 1;
        break;
      }
    }
  }
  p.shoot_cd = 0.28f;
}

/* ---- the keepers' rules ------------------------------------------------ */
static void update_boss(float dt) {
  float ad;
  if (!boss.alive) return;
  if (!boss.active) {
    if (p.x > boss.x - 150.0f) boss.active = 1;
    else return;
  }
  if (boss.flash > 0.0f) boss.flash -= dt;
  if (boss.steam_burst > 0.0f) boss.steam_burst -= dt;
  boss.base_y = (boss.kind == 2 && has_bridge) ? sea_y : ground_y(boss.x);
  if (boss.intro > 0.0f) { boss.intro -= dt; return; }
  boss.facing = (p.x < boss.x) ? -1 : 1;
  ad = fabsf(p.x - boss.x);

  if (boss.kind == 1) {
    /* the tiger: closes the gap on two legs, swipes, and leaps at her mark */
    if (boss.claw_t > 0.0f) {
      boss.claw_t -= dt;
      if (boss.claw_t <= 0.0f && ad < 40.0f) hurt(1);
      return;
    }
    if (boss.pounce_t > 0.0f) {
      boss.pounce_t -= dt;
      boss.x += (boss.pounce_x - boss.x) * clampf(dt * 6.0f, 0.0f, 1.0f);
      boss.lift = sinf((1.0f - boss.pounce_t / 0.5f) * 3.14159f) * 14.0f;
      if (boss.pounce_t <= 0.0f) {
        boss.lift = 0.0f;
        if (fabsf(p.x - boss.x) < 24.0f) hurt(1);
        boss.cd = 1.0f;
      }
      return;
    }
    boss.lift = 0.0f;
    if (ad > 44.0f) boss.x += (p.x > boss.x ? 1.0f : -1.0f) * (ad > 120.0f ? 72.0f : 34.0f) * dt;
    boss.cd -= dt;
    if (boss.cd <= 0.0f) {
      if (ad < 62.0f) boss.claw_t = 0.4f;
      else { boss.pounce_t = 0.5f; boss.pounce_x = p.x; }
      boss.cd = 1.5f;
    }
  } else {
    /* the water dragon: leaps from the sea on one side of the bridge into the
     * sea on the other, and every second landing breathes */
    if (boss.steam_t > 0.0f) {
      boss.steam_t -= dt;
      if (boss.steam_t <= 0.0f) {
        boss.steam_burst = 0.5f;
        /* it runs the whole span: neither end of the bridge is safe */
        if (has_bridge) {
          if (p.x > bridge_x0 - 20.0f && p.x < bridge_x1 + 20.0f) hurt(1);
        } else if (ad < 170.0f) {
          hurt(1);
        }
      }
      return;
    }
    if (boss.hop_t > 0.0f) {
      float k;
      boss.hop_t -= dt;
      k = 1.0f - boss.hop_t / 0.9f;
      boss.x = boss.hop_from + (boss.hop_to - boss.hop_from) * k;
      boss.lift = sinf(k * 3.14159f) * 34.0f;
      if (boss.hop_t <= 0.0f) {
        boss.lift = 0.0f;
        boss.hops++;
        if (boss.hops % 2 == 0) boss.steam_t = 0.7f;
        boss.cd = 0.8f;
      }
      return;
    }
    boss.lift = 0.0f;
    boss.cd -= dt;
    if (boss.cd <= 0.0f) {
      boss.hop_from = boss.x;
      boss.hop_to = (boss.x < boss.home) ? boss.home + 132.0f : boss.home - 132.0f;
      boss.hop_to = clampf(boss.hop_to, 24.0f, stage_len - 24.0f);
      boss.hop_t = 0.9f;
      boss.cd = 0.8f;
    }
  }
}

static void reset_stage(int idx);

static void leave_shop(void) {
  /* The stall is the first thing on every road: leaving it is stepping on. */
  state = 0;
  state_t = 0;
  banner = 1.4f;
}

static void update_shop(int left_pressed, int right_pressed, int jump_pressed, int action_pressed) {
  if (left_pressed) shop_sel = (shop_sel + SHOP_ROWS - 1) % SHOP_ROWS;
  if (right_pressed) shop_sel = (shop_sel + 1) % SHOP_ROWS;
  if (jump_pressed) { leave_shop(); return; }
  if (!action_pressed) return;

  if (shop_sel >= 2) { leave_shop(); return; }
  if (shop_sel == 0) {
    if (!auto_kill && souls >= SHOP_COST[0]) {
      souls -= SHOP_COST[0];
      auto_kill = 1;
      bots_killed = 0;
    }
  } else if (shop_sel == 1) {
    if (p.hp < max_hp && souls >= SHOP_COST[1]) {
      souls -= SHOP_COST[1];
      p.hp = max_hp;
    }
  }
}

static void update_play(float dt, GameInput in, int jump_pressed, int action_pressed) {
  int dir = (in.right ? 1 : 0) - (in.left ? 1 : 0);
  float gy;
  int i;
  /* Holding the shard button while she moves makes her run. The sprint
   * spends her breath; a shard does not. */
  int running = 0;

  if (dir != 0) p.facing = dir;

  running = in.action && dir != 0 && p.on_ground && p.stamina > 0.0f && !p.winded;
  if (running) {
    p.stamina -= 34.0f * dt;
    if (p.stamina <= 0.0f) { p.stamina = 0.0f; p.winded = 1; }
  } else {
    p.stamina += 22.0f * dt;
    if (p.stamina > 100.0f) p.stamina = 100.0f;
    if (p.winded && p.stamina >= 35.0f) p.winded = 0;
  }
  p.vx += (dir * (running ? 118.0f : 74.0f) - p.vx) * clampf(dt * 9.0f, 0.0f, 1.0f);
  if (jump_pressed && p.on_ground) {
    p.vy = -184.0f;
    p.on_ground = 0;
  }
  p.vy += 430.0f * dt;
  p.x = clampf(p.x + p.vx * dt, 8.0f, stage_len - 8.0f);
  p.y += p.vy * dt;

  gy = ground_y(p.x);
  if (gy < 300.0f && p.y >= gy) {
    p.y = gy;
    p.vy = 0;
    p.on_ground = 1;
  } else {
    p.on_ground = 0;
  }
  if (p.y > H + 24) {
    p.hp = 0;
    state = 2;
    state_t = 1.4f;
    return;
  }
  if (p.invuln > 0) p.invuln = fmaxf(0.0f, p.invuln - dt);
  if (p.shoot_cd > 0) p.shoot_cd -= dt;
  if (action_pressed && p.shoot_cd <= 0) shoot();
  p.anim += fabsf(p.vx) * dt * 0.35f;

  for (i = 0; i < MAX_SHARDS; i++) {
    Shard *s = &shards[i];
    if (!s->live) continue;
    s->x += s->vx * dt;
    s->life -= dt;
    if (s->life <= 0) s->live = 0;
  }

  if (zap_t > 0.0f) zap_t -= dt;
  if (zap_cd > 0.0f) zap_cd -= dt;
  if (auto_kill && zap_cd <= 0.0f) {
    int best = -1;
    float bd = 60.0f;
    int k;
    for (k = 0; k < spider_n; k++) {
      float d;
      if (!spiders[k].alive) continue;
      d = fabsf(spiders[k].x - p.x);
      if (d < bd) { bd = d; best = k; }
    }
    if (best >= 0) {
      zap_x = spiders[best].x;
      zap_t = 0.22f;
      zap_cd = 0.55f;
      spiders[best].alive = 0;
      bots_killed++;
      souls += roll_souls();
    }
  }

  for (i = 0; i < spider_n; i++) {
    Spider *sp = &spiders[i];
    if (!sp->alive) continue;
    sp->x += sp->dir * 20.0f * dt;
    if (sp->x > sp->home + 30.0f) sp->dir = -1;
    if (sp->x < sp->home - 30.0f) sp->dir = 1;

    if (p.invuln <= 0) {
      float sp_gy = ground_y(sp->x);
      if (fabsf(p.x - sp->x) < 9.0f && p.y > sp_gy - 14.0f && p.y < sp_gy + 2.0f) {
        hurt(1);
      }
    }
    {
      int k;
      for (k = 0; k < MAX_SHARDS; k++) {
        if (!shards[k].live) continue;
        if (fabsf(shards[k].x - sp->x) < 9.0f
            && shards[k].y > ground_y(sp->x) - 16.0f
            && shards[k].y < ground_y(sp->x) + 2.0f) {
          sp->alive = 0;
          shards[k].live = 0;
          souls += roll_souls();
          break;
        }
      }
    }
  }

  update_boss(dt);
  if (boss.alive && boss.active) {
    /* The keeper holds the end of the road: she cannot walk past it. */
    if (p.x > boss.home + 70.0f) p.x = boss.home + 70.0f;
    {
      float bhit_y = boss.base_y - 20.0f - boss.lift;
      for (i = 0; i < MAX_SHARDS; i++) {
        if (!shards[i].live) continue;
        if (fabsf(shards[i].x - boss.x) < 24.0f && fabsf(shards[i].y - bhit_y) < 30.0f) {
          shards[i].live = 0;
          boss.hp--;
          boss.flash = 0.12f;
          if (boss.hp <= 0) {
            boss.hp = 0;
            boss.alive = 0;
            souls += roll_souls();
          }
        }
      }
    }
  }

  for (i = 0; i < mem_n; i++) {
    if (mems[i].taken) continue;
    if (fabsf(p.x - mems[i].x) < 9.0f
        && p.y > mems[i].y - 2 && p.y < mems[i].y + 18) {
      mems[i].taken = 1;
      mem_taken++;
    }
  }

  cam = clampf(p.x - 110.0f, 0.0f, stage_len - W);

  if (p.x > stage_len - 34.0f && mem_taken >= mem_n && !boss.alive) {
    state = 1;
    state_t = 2.0f;
  }
}

/* ---- drawing ----------------------------------------------------------- */
static void draw_world(void) {
  int x, i;

  for (i = 0; i < H; i++) {
    uint16_t *row = &fb[i * W];
    for (x = 0; x < W; x++) row[x] = sky_row[i];
  }

  disc(W - 64, 42, 13, to565(0xF2D8A0));

  for (x = 0; x < W; x++) {
    int idx = gi(cam * 0.35f + x);
    int top = 120 - hill_far[idx];
    rect(x, top, 1, 150 - top, col_hill_far);
  }
  for (x = 0; x < W; x++) {
    int idx = gi(cam * 0.6f + x);
    int top = 134 - hill_near[idx];
    rect(x, top, 1, 162 - top, col_hill_near);
  }

  for (x = 0; x < W; x++) {
    float wx = cam + x;
    int g;
    if (has_bridge && wx >= bridge_x0 - SEA_W - 1.0f && wx <= bridge_x1 + SEA_W + 1.0f) continue;
    g = gnd[gi(wx)];
    if (g >= H || g >= 300) continue;
    rect(x, g, 1, H - g, col_dirt);
    px(x, g, col_grass);
    if ((((int)wx / 2) % 17) == 0) px(x, g + 5, col_stone);
  }

  if (has_bridge) {
    /* the sea either side of the bridge, and the plank span between them */
    int bx0 = (int)(bridge_x0 - cam);
    int bx1 = (int)(bridge_x1 - cam);
    int wy = (int)sea_y;
    for (x = bx0 - (int)SEA_W; x < bx1 + (int)SEA_W; x++) {
      if (x < 0 || x >= W) continue;
      rect(x, wy, 1, H - wy, to565(0x10343C));
      px(x, wy, ((x >> 1) & 1) ? to565(0x8FC9BF) : to565(0x2E6A68));
      if ((x % 9) == 0) px(x, wy + 4, to565(0x1D5A60));
    }
    for (x = bx0; x < bx1; x++) {
      if (x < 0 || x >= W) continue;
      rect(x, wy - 4, 1, 4, ((x >> 1) & 1) ? to565(0x6A4A2A) : to565(0x8A6238));
      if ((x % 11) == 0) {
        rect(x, wy - 8, 1, 5, to565(0x3A2A1A));
        px(x, wy - 9, to565(0x9AB07A));
      }
    }
  }

  for (i = 0; i < mem_n; i++) {
    int mx, my;
    if (mems[i].taken) continue;
    mx = (int)(mems[i].x - cam);
    my = mems[i].y + (int)(sinf(mems[i].x * 0.1f) * 2.0f);
    if (mx < -8 || mx > W + 8) continue;
    px(mx, my, to565(0xFFFFFF));
    px(mx - 1, my, to565(0x9FE8FF));
    px(mx + 1, my, to565(0x9FE8FF));
    px(mx, my - 1, to565(0x9FE8FF));
    px(mx, my + 1, to565(0x9FE8FF));
  }

  for (i = 0; i < spider_n; i++) {
    int sx, sy;
    if (!spiders[i].alive) continue;
    sx = (int)(spiders[i].x - cam);
    if (sx < -16 || sx > W + 16) continue;
    sy = (int)ground_y(spiders[i].x);
    rect(sx - 6, sy - 9, 12, 6, to565(0x140E16));
    rect(sx - 4, sy - 11, 8, 3, to565(0x1E1520));
    px(sx - 3, sy - 9, to565(0xD04040));
    px(sx + 3, sy - 9, to565(0xD04040));
    px(sx - 7, sy - 8, to565(0x140E16));
    px(sx + 7, sy - 8, to565(0x140E16));
    px(sx - 6, sy - 5, to565(0x140E16));
    px(sx + 6, sy - 5, to565(0x140E16));
  }

  draw_boss();

  for (i = 0; i < MAX_SHARDS; i++) {
    int sx, sy;
    if (!shards[i].live) continue;
    sx = (int)(shards[i].x - cam);
    sy = (int)shards[i].y;
    rect(sx, sy, 3, 1, to565(0xFFFFFF));
    px(sx - 2, sy, to565(0x9FE8FF));
  }

  if (p.invuln <= 0 || sinf(p.invuln * 28.0f) > 0) {
    int px0 = (int)(p.x - cam);
    int py0 = (int)p.y;
    int step = (int)p.anim & 1;
    rect(px0 - 3, py0 - 13, 6, 7, to565(0xD8D2C4));
    rect(px0 - 2, py0 - 18, 5, 5, to565(0xE8C0A0));
    rect(px0 - 3, py0 - 19, 7, 3, to565(0x3A2A2A));
    rect(px0 - 3 + p.facing, py0 - 13, 3, 2, to565(0xB03A3A));
    if (p.on_ground) {
      rect(px0 - 2, py0 - 6 + step, 2, 6 - step, to565(0x2A2438));
      rect(px0 + 1, py0 - 6 + (1 - step), 2, 6 - (1 - step), to565(0x2A2438));
    } else {
      rect(px0 - 2, py0 - 6, 2, 4, to565(0x2A2438));
      rect(px0 + 1, py0 - 5, 2, 4, to565(0x2A2438));
    }
  }

  /* the guardians' strike: two cold lines out of nowhere */
  if (zap_t > 0.0f) {
    int x0 = (int)(p.x - cam);
    int y0 = (int)p.y - 18;
    int x1 = (int)(zap_x - cam);
    int y1 = (int)ground_y(zap_x) - 9;
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int steps = dx > dy ? dx : dy;
    int s;
    for (s = 0; s <= steps; s++) {
      int xx = x0 + (steps ? (x1 - x0) * s / steps : 0);
      int yy = y0 + (steps ? (y1 - y0) * s / steps : 0);
      px(xx, yy, to565(0xCFE8FF));
      px(xx, yy + 1, to565(0x6F9FC8));
    }
  }
}

static void draw_boss(void) {
  int bx, by, i;
  if (!boss.alive) return;
  bx = (int)(boss.x - cam);
  if (bx < -60 || bx > W + 60) return;
  by = (int)(((boss.kind == 2 && boss.base_y > 1.0f) ? boss.base_y : ground_y(boss.x)) - boss.lift);
  if (boss.kind == 1) {
    rect(bx - 9, by - 30, 18, 22, to565(0xB4651C));
    rect(bx - 7, by - 34, 14, 8, to565(0xD08634));
    rect(bx - 5, by - 30, 10, 13, to565(0xE8CFA4));
    rect(bx - 3, by - 44, 12, 11, to565(0xD08634));
    px(bx - 2, by - 42, to565(0xFFD47A));
    px(bx + 4, by - 42, to565(0xFFD47A));
    px(bx - 1, by - 40, to565(0x1A0F08));
    px(bx + 3, by - 40, to565(0x1A0F08));
    rect(bx + 5, by - 39, 5, 3, to565(0xF0DCB4));
    for (i = 0; i < 3; i++) rect(bx - 9 + i * 5, by - 28, 2, 5, to565(0x3A2210));
    rect(bx - 11, by - 26, 3, 10, to565(0xA85A18));
    rect(bx + 8, by - 26, 3, 10, to565(0xA85A18));
    rect(bx - 6, by - 10, 4, 10, to565(0x3A2414));
    rect(bx + 2, by - 10, 4, 10, to565(0x3A2414));
    if (boss.claw_t > 0.0f) rect(bx - 20, by - 30, 9, 4, to565(0xD89A4C));
  } else {
    rect(bx - 16, by - 22, 30, 12, to565(0x1D5A60));
    rect(bx - 14, by - 25, 24, 6, to565(0x8FC9BF));
    rect(bx - 24, by - 30, 8, 5, to565(0x123A40));
    rect(bx + 8, by - 31, 13, 9, to565(0x24707A));
    rect(bx + 13, by - 27, 9, 4, to565(0x7A2A2E));
    for (i = 0; i < 4; i++) px(bx + 15 + i * 2, by - 26, to565(0xD96A72));
    px(bx + 11, by - 32, to565(0xDFF7F2));
    for (i = 0; i < 3; i++) px(bx + 9 + i * 3, by - 35, to565(0xCFEAE4));
    rect(bx - 8, by - 10, 3, 10, to565(0x16424A));
    rect(bx + 4, by - 10, 3, 10, to565(0x16424A));
    if (boss.steam_t > 0.0f) disc(bx + 23, by - 24, 3, to565(0xDFF8F4));
    if (boss.steam_burst > 0.0f) {
      for (i = 0; i < 3; i++) {
        rect(bx - 60 - i * 14, by - 36 - i * 5, 120 + i * 24, 34 + i * 8, to565(0xCFEAE4));
      }
    }
  }
  if (boss.flash > 0.0f) rect(bx - 18, by - 46, 36, 46, to565(0xFFFFFF));
}

static void draw_hud(void) {
  char buf[16];
  int i;

  for (i = 0; i < max_hp && i < 5; i++) {
    uint16_t c = i < p.hp ? to565(0xE05050) : to565(0x402028);
    int hx = 6 + i * 7;
    px(hx, 6, c);
    px(hx + 1, 5, c);
    px(hx + 2, 6, c);
    px(hx + 1, 7, c);
    px(hx + 1, 8, c);
  }

  /* her breath, under the hearts */
  rect(6, 12, 44, 3, to565(0x101018));
  {
    int fill = (int)(p.stamina * 0.44f);
    uint16_t bc = p.winded ? to565(0x6B6478) : (p.stamina > 30.0f ? to565(0x8FD0FF) : to565(0xFFB066));
    rect(7, 13, fill, 1, bc);
  }

  if (boss.alive && boss.active) {
    int bw = W - 120;
    rect(60, 20, bw, 4, to565(0x30202A));
    rect(60, 20, bw * boss.hp / boss.max_hp, 4, to565(0xE05050));
    px(60, 20, to565(0xFFD9A0));
  }

  mem_str(buf, souls);
  text(buf, W - 6 - text_w(buf, 1), 5, 1, to565(0xFFD9A0));
  buf[0] = 'M'; buf[1] = 'E'; buf[2] = 'M'; buf[3] = ' ';
  buf[4] = (char)('0' + mem_taken);
  buf[5] = '/';
  buf[6] = (char)('0' + mem_n);
  buf[7] = 0;
  text(buf, W - 6 - text_w(buf, 1), 13, 1, to565(0x9FB4C4));

  if (banner > 0.0f) {
    text_center(STAGES[stage_i].name, 56, 1, to565(0xF0ECDC));
    text_center("COLLECT THE MEMORIES", 66, 1, to565(0x9FB4C4));
    if (stage_i == 0 && banner > 1.2f) {
      text_center("LEFT RIGHT MOVE   X JUMP   C SHARD", H - 22, 1, to565(0xB8C4D0));
      text_center("HOLD C TO RUN - IT SPENDS HER BREATH", H - 12, 1, to565(0x8D869F));
    }
  }

  if (state == 1) {
    text_center("STAGE CLEAR", 78, 2, to565(0xFFF2C0));
  } else if (state == 2) {
    text_center("SHE FALLS", 78, 2, to565(0xF08080));
    text_center("TRY AGAIN", 96, 1, to565(0xD0B0B0));
  } else if (state == 3) {
    text_center("THE END", 66, 3, to565(0xFFF0D8));
    text_center("THE ROAD GOES ON", 92, 1, to565(0xC8C0B0));
    text_center("PRESS X TO WALK IT AGAIN", 112, 1, to565(0x8FA0B0));
  }
}


/* ---- the peddler's window ---------------------------------------------- */
static void mem_str(char *b, int n) {
  int i = 0;
  b[i++] = 'S'; b[i++] = 'O'; b[i++] = 'U'; b[i++] = 'L'; b[i++] = ' ';
  if (n >= 100) b[i++] = (char)('0' + (n / 100) % 10);
  if (n >= 10) b[i++] = (char)('0' + (n / 10) % 10);
  b[i++] = (char)('0' + (n % 10));
  b[i] = 0;
}

static void cost_str(char *b, int cost) {
  int i = 0;
  if (cost >= 100) b[i++] = (char)('0' + (cost / 100) % 10);
  if (cost >= 10) b[i++] = (char)('0' + (cost / 10) % 10);
  b[i++] = (char)('0' + (cost % 10));
  b[i++] = ' ';
  b[i++] = 'S'; b[i++] = 'O'; b[i++] = 'U'; b[i++] = 'L';
  b[i] = 0;
}

static void draw_shop(void) {
  char buf[16];
  int i, y;

  /* dim the road behind the window */
  for (i = 0; i < H; i++) {
    uint16_t *row = &fb[i * W];
    int x;
    for (x = 0; x < W; x++) {
      uint16_t c = row[x];
      int r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
      row[x] = (uint16_t)(((r >> 1) << 11) | ((g >> 1) << 5) | (b >> 1));
    }
  }

  rect(38, 22, W - 76, H - 44, to565(0x161220));
  rect(38, 22, W - 76, 1, to565(0x8E7FB0));
  rect(38, H - 23, W - 76, 1, to565(0x8E7FB0));
  rect(38, 22, 1, H - 44, to565(0x8E7FB0));
  rect(W - 39, 22, 1, H - 44, to565(0x8E7FB0));

  text_center("THE SOUL PEDDLER", 30, 1, to565(0xF0E8F8));
  mem_str(buf, souls);
  text(buf, W - 44 - text_w(buf, 1), 40, 1, to565(0xD8C9A8));

  for (i = 0; i < SHOP_ROWS; i++) {
    int owned = 0;
    const char *label;
    y = 56 + i * 18;
    if (i == shop_sel) {
      rect(46, y - 3, W - 92, 13, to565(0x2A2140));
      px(48, y + 1, to565(0xFFD9A0));
      px(48, y + 2, to565(0xFFD9A0));
      px(49, y + 2, to565(0xFFD9A0));
      px(48, y + 3, to565(0xFFD9A0));
    }
    if (i == 2) {
      text("LEAVE THE STALL", 56, y, 1, shop_sel == 2 ? to565(0xFFFFFF) : to565(0xB9B2C8));
      text("WALK ON", W - 84, y, 1, to565(0x8A8296));
      continue;
    }
    label = SHOP_NAME[i];
    if (i == 0) owned = auto_kill;
    else owned = (p.hp >= max_hp) ? 2 : 0;

    if (owned == 1) {
      text(label, 56, y, 1, to565(0x6B6478));
      text("OWNED", W - 84, y, 1, to565(0x6B6478));
      continue;
    }
    if (owned == 2) {
      text(label, 56, y, 1, to565(0x6B6478));
      text("WHOLE", W - 84, y, 1, to565(0x6B6478));
      continue;
    }
    text(label, 56, y, 1, souls >= SHOP_COST[i] ? to565(0xF6E2C0) : to565(0x8A8296));
    cost_str(buf, SHOP_COST[i]);
    text(buf, W - 44 - text_w(buf, 1), y, 1,
         souls >= SHOP_COST[i] ? to565(0xFFD9A0) : to565(0x8A8296));
  }

  if (shop_sel < 2) text_center(SHOP_DESC[shop_sel], 118, 1, to565(0xC9C1D6));
  else text_center("SHE TAKES HER SOULS WITH HER.", 118, 1, to565(0xC9C1D6));

  if (shop_t > 0.25f) {
    text_center("LEFT RIGHT CHOOSE   C BUY   X GO", H - 34, 1, to565(0x8D869F));
  }
}

/* ---- public API -------------------------------------------------------- */
void game_update(float dt, GameInput in) {
  static GameInput prev;
  int jump_pressed = in.jump && !prev.jump;
  int action_pressed = in.action && !prev.action;
  int left_pressed = in.left && !prev.left;
  int right_pressed = in.right && !prev.right;
  prev = in;

  if (dt <= 0) dt = 0.016f;
  if (dt > 0.1f) dt = 0.1f;
  if (banner > 0.0f) banner -= dt;

  if (state == 1) {
    state_t -= dt;
    if (state_t <= 0) {
      /* The next road begins at the peddler's stall. */
      if (stage_i < 2) reset_stage(stage_i + 1);
      else { state = 3; finished = 1; }
    }
  } else if (state == 4) {
    shop_t += dt;
    update_shop(left_pressed, right_pressed, jump_pressed, action_pressed);
  } else if (state == 2) {
    state_t -= dt;
    if (state_t <= 0) {
      p.hp = max_hp;   /* a retry is a fresh start, not a slow death */
      reset_stage(stage_i);
    }
  } else if (state == 3) {
    if (action_pressed) { finished = 0; reset_stage(0); }
  } else {
    update_play(dt, in, jump_pressed, action_pressed);
  }

  draw_world();
  draw_hud();
  if (state == 4) draw_shop();
}

const uint16_t *game_framebuffer(void) { return fb; }
int game_stage(void) { return stage_i; }
int game_hp(void) { return p.hp; }
int game_memories(void) { return mem_taken; }
int game_total_memories(void) { return mem_n; }
int game_finished(void) { return finished; }
int game_state(void) { return state; }
int game_purse(void) { return souls; }
int game_souls(void) { return souls; }
int game_stamina(void) { return (int)p.stamina; }
int game_max_hp(void) { return max_hp; }
int game_auto_kill(void) { return auto_kill; }
int game_boss_kind(void) { return boss.kind; }
int game_boss_hp(void) { return boss.hp; }
int game_bots_killed(void) { return bots_killed; }

#ifdef GAME_TEST
void game_test_souls(int n) { souls += n; }
#endif

/* Extras for host tools and tests. */
float game_ground_at(float x) { return ground_y(x); }
float game_player_x(void) { return p.x; }
