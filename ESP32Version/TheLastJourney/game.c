/*
 * The Last Journey — Pocket Edition
 *
 * A small, self-contained C port of the game for microcontrollers with a
 * colour TFT (ESP32 and friends). It keeps the shape of the story: run the
 * four roads, gather the memories, and reach the end. Rendering goes to a
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
#define MAX_ARROWS 8
#define MAX_CLONES 2
#define MAX_BOLTS 12
#define KEEPERS 3
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

static const StageCfg STAGES[4] = {
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
  {
    /* The forest past the cliffs: a green roof, two green walls of trees and
     * a road of living bushes. It is the longest road of the four. */
    "THE VERDANT WILDS", 2900.0f, 3,
    { { 700, 748 }, { 1240, 1288 }, { 1980, 2028 } },
    0x08261A, 0x6FCB6A, 0x123A1E, 0x1E5A2C, 0x2A1C12, 0x4FE060, 0x16200E,
    0.0f, 0.0f
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
static float bridge_x0, bridge_x1, arena_y;
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
  float dmg;     /* what this shard takes off whatever it hits */
} Shard;

static Player p;
static Spider spiders[MAX_SPIDERS];
static int spider_n;
static Memory mems[MAX_MEM];
static int mem_n, mem_taken;
static Shard shards[MAX_SHARDS];

/* ---- the keepers ------------------------------------------------------ */
/* Stage one ends with the tiger of the ruins, the third road with the water
 * dragon, and the wilds with three of them one after another: the Forest
 * Fairy, the Love Goddess and the Ocean Rage God. -1 = no keeper left. */
static const int BOSS_KIND[4][KEEPERS] = {
  { 0, -1, -1 },
  { 1, -1, -1 },
  { 2, -1, -1 },
  { 3, 4, 5 },
};
static int boss_qi;    /* which of the road's keepers stands there now */
static float keeper_t; /* the note under the bar when the glade changes hands */

typedef struct {
  int kind;              /* 1 tiger, 2 dragon, 3 fairy */
  float x, home, base_y;
  /* damage is paid in fractions now: her X powers bite by percentages */
  float hp, max_hp;
  int alive, active, facing;
  float intro, cd, claw_t, pounce_t, pounce_x;
  float hop_t, hop_from, hop_to, lift, steam_t, steam_burst;
  float bob;             /* the slow swell it rides while it stands */
  float burn_t, ice_t, shock_t;
  int hops;
  float flash;
  /* the Forest Fairy's own weather */
  float rain_t;
  int rot;               /* which of her powers comes next */
  /* the love goddess and the ocean god gather before they cast */
  float cast_t;
  int spell;
} Boss;

static Boss boss;

/* ---- the Forest Fairy's weather --------------------------------------- */
/* Eight falling arrows, one rolling fireball and two shadow clones are all
 * the wilds need: 8 * 12 + 20 + 2 * 12 = 140 bytes of state. */
typedef struct {
  float x, y, vy;
  int live;
} Arrow;

typedef struct {
  float x, y, vx, life;
  int live;
} Fireball;

typedef struct {
  float x, life, fire_cd;
} Clone;

static Arrow arrows[MAX_ARROWS];
static Fireball fball;
static Clone clones[MAX_CLONES];

/* ---- the love and the tide -------------------------------------------- */
/* Twelve bolts in all: the Love Goddess' hearts and kisses and the ocean's
 * fish share one pool, because only one keeper of the three ever stands. */
typedef struct {
  float x, y, vx, vy, life;
  int kind;   /* 0 heart, 1 kiss, 2 fish */
  int live;
} Bolt;

typedef struct {
  float x, life;
  int dir, live, hit;
} Wave;

typedef struct {
  float x, life, tick;
  int live;
} Storm;

static Bolt bolts[MAX_BOLTS];
static Wave wave;
static Storm storm;

static float cam;
static int state; /* 0 play, 1 clear, 2 dead, 3 end, 4 shop, 5 menu */
static int pause_sel;
static float state_t;
static float banner;
static int finished;

/* ---- the soul peddler -------------------------------------------------- */
/* Souls are the purse: they come off the bots she puts down, they add up
 * across the run, and the stall at the first of every road spends them. */
/* Four wares for her body and her X: a ward, and fire, ice and the charge.
 * The road's own powers still sit behind them. */
#define SHOP_ROWS 7
#define SHOP_ITEMS 6
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
/* her ward: one blow taken for her, back at the start of every road */
static int shield_on, shield_hp;
/* the peddler's elements: fire burns, ice slows, the charge bites harder */
static int pw_fire, pw_ice, pw_elec;

static const char *SHOP_NAME[SHOP_ITEMS] = {
  "A WARD OF HER OWN", "FIRE IN HER X", "ICE IN HER X",
  "ELECTRIC IN HER X", "AUTOMATIC GUARDIAN", "REFILL HER HEALTH",
};
static const int SHOP_COST[SHOP_ITEMS] = { 50, 50, 50, 50, 150, 10 };
static const char *SHOP_DESC[SHOP_ITEMS] = {
  "A BAR ABOVE HER LIFE. BUY IT BACK WHEN IT BREAKS.",
  "SHARDS BITE 15% HARDER AND BURN 2% A SECOND.",
  "SHARDS BITE 14% HARDER AND SLOW WHAT THEY TOUCH.",
  "SHARDS BITE 18% HARDER AND THE CHARGE JUMPS.",
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
static void draw_weather(void);
static void clear_weather(void);
static void setup_boss(int kind);

static float clampf(float v, float a, float b) {
  return v < a ? a : (v > b ? b : v);
}

static int keeper_count(int idx) {
  int i, n = 0;
  for (i = 0; i < KEEPERS; i++) if (BOSS_KIND[idx][i] >= 0) n++;
  return n;
}

/* What her X is worth once the peddler's elements are in it. Any two feed
 * each other; all three vaporize. */
static int power_count(void) {
  return (pw_fire ? 1 : 0) + (pw_ice ? 1 : 0) + (pw_elec ? 1 : 0);
}

static float power_mult(void) {
  float m = 1.0f;
  int n = power_count();
  if (pw_fire) m += 0.15f;
  if (pw_ice) m += 0.14f;
  if (pw_elec) m += 0.18f;
  if (n == 2) m += 0.10f;
  if (n == 3) m += 0.25f;
  return m;
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
static const char *FONT[41] = {
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
  "110001010100011", /* % */
};

static int glyph(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= '0' && c <= '9') return 26 + (c - '0');
  if (c == '/') return 36;
  if (c == ':') return 37;
  if (c == '.') return 38;
  if (c == '!') return 39;
  if (c == '%') return 40;
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
/* One keeper takes the end of the road. The wilds call this again the moment
 * the one before it falls, so the glade never stands empty. */
static void setup_boss(int kind) {
  boss.kind = kind;
  /* The wilds' three are the long fight: each asks for more than the tiger's
   * nine hits, and the tide more than the green. */
  boss.max_hp = kind == 5 ? 16.0f : kind == 4 ? 14.0f
              : kind == 3 ? 12.0f : (kind ? 9.0f : 0.0f);
  boss.hp = boss.max_hp;
  boss.alive = kind ? 1 : 0;
  boss.active = 0;
  boss.x = stage_len * 0.80f;
  boss.home = boss.x;
  /* the dragon keeps to the planks, at the near end of its bridge */
  if (kind == 2 && has_bridge) boss.x = bridge_x0 + 22.0f;
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
  boss.bob = 0.0f;
  boss.flash = 0.0f;
  boss.burn_t = 0.0f;
  boss.ice_t = 0.0f;
  boss.shock_t = 0.0f;
  boss.rain_t = 0.0f;
  boss.rot = 0;
  boss.cast_t = 0.0f;
  boss.spell = 0;
}

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
  if (has_bridge) {
    int bx;
    /* The last span is only a bridge: the cliff, one cut, and a level deck
     * across it. No bays, no water at either end of the planks. */
    arena_y = ground_y((bridge_x0 + bridge_x1) * 0.5f);
    if (arena_y > 300.0f) arena_y = ground_y(bridge_x0 - 2.0f);
    for (bx = (int)bridge_x0; bx <= (int)bridge_x1; bx++)
      gnd[gi((float)bx)] = (int16_t)arena_y;
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
  purse_left = spider_n + keeper_count(idx);
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
  clear_weather();

  boss_qi = 0;
  keeper_t = 0.0f;
  setup_boss(BOSS_KIND[idx][0]);
  /* The ward is spent, not mended: a road that finds it used leaves it used. */

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
  shield_on = 0;
  shield_hp = 0;
  pw_fire = 0;
  pw_ice = 0;
  pw_elec = 0;
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
  if (shield_hp > 0) {
    shield_hp = 0;
    p.invuln = 1.2f;
    p.vy = -70.0f;
    p.vx = -p.facing * 40.0f;
    return;
  }
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
        shards[i].dmg = power_mult();
        shards[i].live = 1;
        break;
      }
    }
  }
  p.shoot_cd = 0.28f;
}

/* ---- the keepers' rules ------------------------------------------------ */
/* A keeper's death: paid out of the road's purse, like everything else. */
static void boss_die(void) {
  boss.hp = 0.0f;
  boss.alive = 0;
  clear_weather();
  souls += roll_souls();
  /* The glade changes hands: the next keeper steps out of the green. */
  {
    int qi = boss_qi + 1;
    while (qi < KEEPERS && BOSS_KIND[stage_i][qi] < 0) qi++;
    if (qi < KEEPERS && BOSS_KIND[stage_i][qi] >= 0) {
      boss_qi = qi;
      setup_boss(BOSS_KIND[stage_i][qi]);
      boss.intro = 1.6f;
      keeper_t = 2.4f;
    }
  }
}

/* ---- the fairy's weather ----------------------------------------------- */
static void clear_weather(void) {
  int i;
  for (i = 0; i < MAX_ARROWS; i++) arrows[i].live = 0;
  for (i = 0; i < MAX_CLONES; i++) clones[i].life = 0.0f;
  for (i = 0; i < MAX_BOLTS; i++) bolts[i].live = 0;
  fball.live = 0;
  wave.live = 0;
  storm.live = 0;
}

static void bolt_spawn(int kind, float x, float y, float vx, float vy, float life) {
  int i;
  for (i = 0; i < MAX_BOLTS; i++) {
    if (bolts[i].live) continue;
    bolts[i].kind = kind;
    bolts[i].x = x;
    bolts[i].y = y;
    bolts[i].vx = vx;
    bolts[i].vy = vy;
    bolts[i].life = life;
    bolts[i].live = 1;
    return;
  }
}

/* SHOOTING LOVE: three hearts fanned at her, and a poisoned kiss thrown
 * underhand so it arcs. Two powers, no more: the glade is narrow. */
static void love_cast(void) {
  float ey = ground_y(boss.x) - 26.0f;
  float hand = boss.x + (p.x > boss.x ? 10.0f : -10.0f);
  if (boss.spell == 0) {
    float base = atan2f((p.y - 12.0f) - ey, p.x - boss.x);
    int i;
    for (i = -1; i <= 1; i++) {
      float a = base + (float)i * 0.26f;
      bolt_spawn(0, hand, ey, cosf(a) * 104.0f, sinf(a) * 104.0f, 4.0f);
    }
  } else {
    float a = atan2f((p.y - 16.0f) - ey, p.x - boss.x) - 0.18f;
    bolt_spawn(1, hand, ey, cosf(a) * 128.0f, sinf(a) * 128.0f - 24.0f, 3.4f);
  }
  boss.spell = 0;
}

/* THE GREAT WAVE runs the glade low, THE TORNADO stands where she was, and
 * THE RAIN OF FISHES falls over the whole of it. */
static void ocean_cast(void) {
  int i;
  if (boss.spell == 0) {
    wave.x = boss.x + (float)boss.facing * 18.0f;
    wave.dir = boss.facing;
    wave.life = 3.4f;
    wave.hit = 0;
    wave.live = 1;
  } else if (boss.spell == 1) {
    storm.x = clampf(p.x + (p.x > boss.x ? 26.0f : -26.0f), 20.0f, stage_len - 20.0f);
    storm.life = 4.0f;
    storm.tick = 0.5f;
    storm.live = 1;
  } else {
    for (i = 0; i < 6; i++) {
      float fx = clampf(p.x - 75.0f + (float)i * 30.0f, 14.0f, stage_len - 14.0f);
      bolt_spawn(2, fx, -10.0f - (float)(i & 1) * 8.0f,
                 (float)((i & 1) ? 10 : -10), 150.0f + (float)(i % 3) * 20.0f, 4.5f);
    }
  }
  boss.spell = 0;
}

/* Poisoned arrows fall where she stands, the fireball rolls along the ground
 * and the clones loose an arrow whenever they are ready. */
static void update_weather(float dt) {
  int i;
  for (i = 0; i < MAX_ARROWS; i++) {
    Arrow *a = &arrows[i];
    if (!a->live) continue;
    a->y += a->vy * dt;
    if (p.invuln <= 0.0f && fabsf(p.x - a->x) < 7.0f
        && a->y > p.y - 20.0f && a->y < p.y + 2.0f) {
      a->live = 0;
      hurt(1);
      continue;
    }
    if (a->y > ground_y(a->x) + 2.0f || a->y > (float)H + 8.0f) a->live = 0;
  }
  if (fball.live) {
    fball.x += fball.vx * dt;
    fball.life -= dt;
    fball.y = ground_y(fball.x) - 8.0f;
    if (p.invuln <= 0.0f && fabsf(p.x - fball.x) < 11.0f && p.y > fball.y - 6.0f) {
      hurt(1);
      fball.live = 0;
    } else if (fball.life <= 0.0f || fball.x < 8.0f || fball.x > stage_len - 8.0f
               || ground_y(fball.x) > 300.0f) {
      fball.live = 0;
    }
  }
  for (i = 0; i < MAX_CLONES; i++) {
    Clone *c = &clones[i];
    if (c->life <= 0.0f) continue;
    c->life -= dt;
    c->fire_cd -= dt;
    if (c->fire_cd <= 0.0f) {
      int k;
      for (k = 0; k < MAX_ARROWS; k++) {
        if (arrows[k].live) continue;
        arrows[k].x = c->x;
        arrows[k].y = ground_y(c->x) - 34.0f;
        arrows[k].vy = 150.0f;
        arrows[k].live = 1;
        break;
      }
      c->fire_cd = 1.4f;
    }
  }

  /* the love and the tide share the bolt pool */
  for (i = 0; i < MAX_BOLTS; i++) {
    Bolt *b = &bolts[i];
    if (!b->live) continue;
    b->x += b->vx * dt;
    b->y += b->vy * dt;
    b->life -= dt;
    if (b->kind == 1) b->vy += 90.0f * dt;       /* the kiss arcs */
    else if (b->kind == 2) b->vy += 40.0f * dt;  /* the fish falls harder */
    if (p.invuln <= 0.0f && fabsf(p.x - b->x) < 7.0f
        && b->y > p.y - 20.0f && b->y < p.y + 3.0f) {
      b->live = 0;
      hurt(1);
      continue;
    }
    if (b->life <= 0.0f || b->x < 4.0f || b->x > stage_len - 4.0f
        || b->y > ground_y(b->x) + 2.0f || b->y > (float)H + 8.0f) b->live = 0;
  }
  if (wave.live) {
    float crest;
    wave.x += (float)wave.dir * 132.0f * dt;
    wave.life -= dt;
    crest = ground_y(wave.x) - 6.0f;
    if (!wave.hit && p.invuln <= 0.0f && fabsf(p.x - wave.x) < 13.0f
        && p.y > crest - 4.0f) {
      wave.hit = 1;
      hurt(1);
    }
    if (wave.life <= 0.0f || wave.x < 6.0f || wave.x > stage_len - 6.0f
        || ground_y(wave.x) > 300.0f) wave.live = 0;
  }
  if (storm.live) {
    storm.life -= dt;
    storm.tick -= dt;
    if (storm.tick <= 0.0f && p.invuln <= 0.0f
        && fabsf(p.x - storm.x) < 16.0f
        && p.y > ground_y(storm.x) - 26.0f) {
      storm.tick = 1.0f;
      hurt(1);
    }
    if (storm.life <= 0.0f) storm.live = 0;
  }
}

static void update_boss(float dt) {
  float ad, sdt;
  if (!boss.alive) return;
  if (!boss.active) {
    /* The keeper of the last bridge sees her from the approach, not from
     * inside its own chest. */
    float seen = boss.x - 150.0f;
    if (boss.kind == 2 && has_bridge) seen = bridge_x0 - 120.0f;
    if (p.x > seen) boss.active = 1;
    else return;
  }
  if (boss.flash > 0.0f) boss.flash -= dt;
  if (boss.steam_burst > 0.0f) boss.steam_burst -= dt;
  boss.bob += dt;
  /* fire keeps eating at it: two percent of everything it has, every second */
  if (boss.burn_t > 0.0f) {
    boss.burn_t -= dt;
    boss.hp -= boss.max_hp * 0.02f * dt;
    if (boss.hp <= 0.0f) { boss_die(); return; }
  }
  if (boss.ice_t > 0.0f) boss.ice_t -= dt;
  if (boss.shock_t > 0.0f) boss.shock_t -= dt;
  /* frost in its blood: everything it does comes slower */
  sdt = boss.ice_t > 0.0f ? dt * 0.55f : dt;
  boss.base_y = (boss.kind == 2 && has_bridge) ? arena_y : ground_y(boss.x);
  if (boss.intro > 0.0f) { boss.intro -= dt; return; }
  boss.facing = (p.x < boss.x) ? -1 : 1;
  ad = fabsf(p.x - boss.x);
  if (boss.kind >= 3) update_weather(dt);

  if (boss.kind == 1) {
    /* the tiger: closes the gap on two legs, swipes, and leaps at her mark */
    if (boss.claw_t > 0.0f) {
      boss.claw_t -= sdt;
      if (boss.claw_t <= 0.0f && ad < 40.0f) hurt(1);
      return;
    }
    if (boss.pounce_t > 0.0f) {
      boss.pounce_t -= sdt;
      boss.x += (boss.pounce_x - boss.x) * clampf(sdt * 6.0f, 0.0f, 1.0f);
      boss.lift = sinf((1.0f - boss.pounce_t / 0.5f) * 3.14159f) * 14.0f;
      if (boss.pounce_t <= 0.0f) {
        boss.lift = 0.0f;
        if (fabsf(p.x - boss.x) < 24.0f) hurt(1);
        boss.cd = 1.0f;
      }
      return;
    }
    boss.lift = 0.0f;
    if (ad > 44.0f) boss.x += (p.x > boss.x ? 1.0f : -1.0f) * (ad > 120.0f ? 72.0f : 34.0f) * sdt;
    boss.cd -= sdt;
    if (boss.cd <= 0.0f) {
      if (ad < 62.0f) boss.claw_t = 0.4f;
      else { boss.pounce_t = 0.5f; boss.pounce_x = p.x; }
      boss.cd = 1.5f;
    }
  } else if (boss.kind == 3) {
    /* the Forest Fairy: she hovers out of reach and lets the wood fight for
     * her — poisoned arrows out of the canopy, a fireball that rolls the
     * ground, and two shadow clones that shoot while she hides among them */
    int i;
    boss.lift = 12.0f + sinf(boss.bob * 3.0f) * 2.0f;
    if (boss.rain_t > 0.0f) boss.rain_t -= sdt;
    if (ad > 112.0f) boss.x += (p.x > boss.x ? 1.0f : -1.0f) * 24.0f * sdt;
    else if (ad < 68.0f) boss.x -= (p.x > boss.x ? 1.0f : -1.0f) * 20.0f * sdt;
    boss.x = clampf(boss.x, boss.home - 90.0f, boss.home + 90.0f);
    boss.cd -= sdt;
    if (boss.cd <= 0.0f) {
      if (boss.rot == 0) {
        /* the rain of poisoned arrows: six of them strung over her mark */
        for (i = 0; i < MAX_ARROWS; i++) {
          arrows[i].x = clampf(p.x - 30.0f + (float)i * 12.0f, 12.0f, stage_len - 12.0f);
          arrows[i].y = 14.0f - (float)(i & 1) * 6.0f;
          arrows[i].vy = 128.0f + (float)(i & 1) * 16.0f;
          arrows[i].live = 1;
        }
        boss.rain_t = 1.1f;
        boss.cd = 2.3f;
      } else if (boss.rot == 1) {
        /* the fireball: it rolls low, so a jump clears it */
        fball.x = boss.x + (p.x > boss.x ? 10.0f : -10.0f);
        fball.y = ground_y(fball.x) - 8.0f;
        fball.vx = (p.x > boss.x ? 1.0f : -1.0f) * 92.0f;
        fball.life = 4.0f;
        fball.live = 1;
        boss.cd = 2.4f;
      } else {
        /* the shadow clones: only one of the three is real */
        for (i = 0; i < MAX_CLONES; i++) {
          clones[i].x = clampf(boss.x + (i == 0 ? -34.0f : 34.0f), 14.0f, stage_len - 14.0f);
          clones[i].life = 6.5f;
          clones[i].fire_cd = 0.5f + (float)i * 0.35f;
        }
        boss.cd = 2.6f;
      }
      boss.rot = (boss.rot + 1) % 3;
    }
  } else if (boss.kind == 4) {
    /* the Love Goddess: she hovers just off the ground, fans hearts at her
     * mark and throws kisses that arc and come down poisoned */
    boss.lift = 4.0f + sinf(boss.bob * 1.6f) * 2.0f;
    if (boss.cast_t > 0.0f) {
      boss.cast_t -= sdt;
      if (boss.cast_t <= 0.0f) love_cast();
      else return;
    }
    if (ad > 150.0f) boss.x += (p.x > boss.x ? 1.0f : -1.0f) * 26.0f * sdt;
    else if (ad < 84.0f) boss.x -= (p.x > boss.x ? 1.0f : -1.0f) * 22.0f * sdt;
    boss.x = clampf(boss.x, boss.home - 90.0f, boss.home + 90.0f);
    boss.cd -= sdt;
    if (boss.cd <= 0.0f) {
      boss.spell = boss.rot & 1;  /* hearts, then the kiss, then hearts */
      boss.cast_t = 0.7f;
      boss.cd = 2.2f;
      boss.rot = (boss.rot + 1) % 2;
    }
  } else if (boss.kind == 5) {
    /* the Ocean Rage God: the great wave, the tornado and a sky of fishes */
    boss.lift = 0.0f;
    if (boss.cast_t > 0.0f) {
      boss.cast_t -= sdt;
      if (boss.cast_t <= 0.0f) ocean_cast();
      else return;
    }
    if (ad > 150.0f) boss.x += (p.x > boss.x ? 1.0f : -1.0f) * 22.0f * sdt;
    else if (ad < 78.0f) boss.x -= (p.x > boss.x ? 1.0f : -1.0f) * 16.0f * sdt;
    boss.x = clampf(boss.x, boss.home - 90.0f, boss.home + 90.0f);
    boss.cd -= sdt;
    if (boss.cd <= 0.0f) {
      boss.spell = boss.rot;
      boss.cast_t = 0.9f;
      boss.cd = 2.4f;
      boss.rot = (boss.rot + 1) % 3;
    }
  } else {
    /* the water dragon: leaps from the sea on one side of the bridge into the
     * sea on the other, and every second landing breathes */
    if (boss.steam_t > 0.0f) {
      boss.steam_t -= sdt;
      if (boss.steam_t <= 0.0f) {
        boss.steam_burst = 0.5f;
        /* It runs the whole span, so neither end of the bridge is safe. It
         * rolls low along the planks, though: a jump clears it. */
        if (p.on_ground) {
          if (has_bridge) {
            if (p.x > bridge_x0 - 20.0f && p.x < bridge_x1 + 20.0f) hurt(1);
          } else if (ad < 170.0f) {
            hurt(1);
          }
        }
      }
      return;
    }
    if (boss.hop_t > 0.0f) {
      float k;
      boss.hop_t -= sdt;
      k = 1.0f - boss.hop_t / 0.9f;
      boss.x = boss.hop_from + (boss.hop_to - boss.hop_from) * k;
      /* One long arc from end to end of the deck, so the leap clears the
       * whole span instead of skimming the boards at either end. */
      boss.lift = powf(sinf(k * 3.14159f), 0.72f) * 44.0f;
      if (boss.hop_t <= 0.0f) {
        boss.lift = 0.0f;
        boss.hops++;
        if (boss.hops % 3 == 0) boss.steam_t = 0.85f;
        boss.cd = 0.8f;
      }
      return;
    }
    boss.lift = 0.0f;
    boss.cd -= sdt;
    if (boss.cd <= 0.0f) {
      boss.hop_from = boss.x;
      if (has_bridge) {
        /* end to end of the deck, never off it */
        boss.hop_to = (boss.x < (bridge_x0 + bridge_x1) * 0.5f)
          ? bridge_x1 - 22.0f : bridge_x0 + 22.0f;
      } else {
        boss.hop_to = (boss.x < boss.home) ? boss.home + 132.0f : boss.home - 132.0f;
      }
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

  if (shop_sel >= SHOP_ITEMS) { leave_shop(); return; }
  if (shop_sel == 0) {
    /* The ward is hers for good, but a spent one is bought back here: the
     * road does not mend it, and neither does anything else. */
    if (shield_hp <= 0 && souls >= SHOP_COST[0]) {
      souls -= SHOP_COST[0];
      shield_on = 1;
      shield_hp = 1;
    }
  } else if (shop_sel == 1) {
    if (!pw_fire && souls >= SHOP_COST[1]) { souls -= SHOP_COST[1]; pw_fire = 1; }
  } else if (shop_sel == 2) {
    if (!pw_ice && souls >= SHOP_COST[2]) { souls -= SHOP_COST[2]; pw_ice = 1; }
  } else if (shop_sel == 3) {
    if (!pw_elec && souls >= SHOP_COST[3]) { souls -= SHOP_COST[3]; pw_elec = 1; }
  } else if (shop_sel == 4) {
    if (!auto_kill && souls >= SHOP_COST[4]) {
      souls -= SHOP_COST[4];
      auto_kill = 1;
      bots_killed = 0;
    }
  } else if (shop_sel == 5) {
    if (p.hp < max_hp && souls >= SHOP_COST[5]) {
      souls -= SHOP_COST[5];
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
          boss.hp -= shards[i].dmg;
          boss.flash = 0.12f;
          if (pw_fire) boss.burn_t = 4.0f;
          if (pw_ice) boss.ice_t = 3.2f;
          if (pw_elec) boss.shock_t = 1.4f;
          if (boss.hp <= 0.0f) boss_die();
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
/* The wilds wear their green above the road as well: a scalloped canopy, tall
 * trunks behind the walk, and bushes, flowers and giant carnivore plants on
 * it. Everything here is a few dozen short runs, so the frame stays cheap. */
static void draw_jungle_back(void) {
  int x, tx;
  float off = fmodf(cam * 0.6f, 96.0f);

  for (x = 0; x < W; x++) {
    int h = 12 + (int)(sinf((cam + x) * 0.05f) * 4.0f)
                + (int)(sinf((cam + x) * 0.13f + 1.7f) * 3.0f);
    rect(x, 0, 1, h, to565(0x0B3320));
    px(x, h, to565(0x1E6A34));
  }

  for (tx = -(int)off; tx < W + 96; tx += 96) {
    int gy = 128 - hill_near[gi(cam * 0.6f + tx)] / 2;
    rect(tx, 20, 4, gy - 20, to565(0x15321A));
    rect(tx - 1, 20, 6, 3, to565(0x0E2A16));
    disc(tx + 1, 18, 10, to565(0x1B5A2C));
    disc(tx - 6, 22, 6, to565(0x226B34));
    disc(tx + 8, 22, 6, to565(0x226B34));
  }
}

/* Plants and flowers stand at the road's own speed. */
static void draw_jungle_ground(void) {
  int x;
  for (x = 0; x < W; x++) {
    float wx = cam + x;
    int g = gnd[gi(wx)];
    int m;
    if (g >= H || g >= 300) continue;
    m = (int)wx % 150;
    if (m == 0) {
      /* a giant carnivore plant: stem, bulb, mouth and teeth */
      rect(x - 1, g - 20, 3, 20, to565(0x2E6A2A));
      disc(x, g - 26, 7, to565(0x7A1E4A));
      disc(x, g - 27, 4, to565(0x2A0618));
      px(x - 4, g - 28, to565(0xFFFFFF));
      px(x + 3, g - 28, to565(0xFFFFFF));
      px(x - 2, g - 24, to565(0xFFFFFF));
      px(x + 1, g - 24, to565(0xFFFFFF));
      rect(x - 5, g - 32, 4, 2, to565(0x3FC24A));
      rect(x + 2, g - 33, 4, 2, to565(0x3FC24A));
    } else if (m % 40 == 0) {
      /* a bush with two flowers in it */
      disc(x, g - 4, 5, to565(0x1E5A2C));
      disc(x + 5, g - 3, 4, to565(0x2E7A3A));
      px(x - 2, g - 8, to565(0xFFB0D8));
      px(x + 5, g - 7, to565(0xFFF0A0));
    } else if (m % 7 == 0) {
      px(x, g - 1, to565(0x9FFFB0));
    }
  }
}

static void draw_world(void) {
  int x, i;

  for (i = 0; i < H; i++) {
    uint16_t *row = &fb[i * W];
    for (x = 0; x < W; x++) row[x] = sky_row[i];
  }

  disc(W - 64, 42, 13, to565(0xF2D8A0));

  if (stage_i == 3) draw_jungle_back();

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
    if (has_bridge && wx >= bridge_x0 - 1.0f && wx <= bridge_x1 + 1.0f) continue;
    g = gnd[gi(wx)];
    if (g >= H || g >= 300) continue;
    rect(x, g, 1, H - g, col_dirt);
    px(x, g, col_grass);
    if ((((int)wx / 2) % 17) == 0) px(x, g + 5, col_stone);
  }

  if (stage_i == 3) draw_jungle_ground();

  if (has_bridge) {
    /* The span rides over one cut: dark air under the planks, and nothing
     * at either end of the bridge but the cliff it lands on. */
    int bx0 = (int)(bridge_x0 - cam);
    int bx1 = (int)(bridge_x1 - cam);
    int deck = (int)arena_y;
    for (x = bx0 - 1; x < bx1 + 1; x++) {
      if (x < 0 || x >= W) continue;
      rect(x, deck + 4, 1, H - deck - 4, to565(0x08141A));
    }
    for (x = bx0; x < bx1; x++) {
      if (x < 0 || x >= W) continue;
      rect(x, deck - 2, 1, 4, ((x >> 1) & 1) ? to565(0x6A4A2A) : to565(0x8A6238));
      if ((x % 11) == 0) {
        rect(x, deck - 8, 1, 6, to565(0x3A2A1A));
        px(x, deck - 9, to565(0x9AB07A));
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

  /* the fairy's weather, drawn over the road she stands on */
  for (i = 0; i < MAX_ARROWS; i++) {
    int ax, ay;
    if (!arrows[i].live) continue;
    ax = (int)(arrows[i].x - cam);
    ay = (int)arrows[i].y;
    if (ax < -4 || ax > W + 4) continue;
    px(ax, ay, to565(0x7DE08A));
    px(ax, ay + 1, to565(0x9FFFB0));
    px(ax - 1, ay + 2, to565(0x2E7A4A));
    px(ax - 1, ay - 1, to565(0xFFB0D8));
  }
  if (fball.live) {
    int fx = (int)(fball.x - cam), fy = (int)fball.y;
    rect(fx - 12, fy + 1, 5, 1, to565(0x8A3A10));
    disc(fx, fy, 8, to565(0xB03A12));
    disc(fx, fy, 6, to565(0xFF8A3C));
    disc(fx + 1, fy, 3, to565(0xFFD166));
    disc(fx + 2, fy, 2, to565(0xFFF6D8));
  }
  for (i = 0; i < MAX_CLONES; i++) {
    int cx, cy, wing;
    if (clones[i].life <= 0.0f) continue;
    if (clones[i].life < 1.2f && sinf(clones[i].life * 20.0f) < 0.0f) continue;
    cx = (int)(clones[i].x - cam);
    cy = (int)ground_y(clones[i].x) - 12;
    wing = (int)(sinf(clones[i].life * 12.0f) * 2.0f);
    disc(cx, cy - 8, 7, to565(0x1B4A2A));
    disc(cx - 6, cy - 12 + wing, 4, to565(0x2E7A4A));
    disc(cx + 6, cy - 12 - wing, 4, to565(0x2E7A4A));
    rect(cx - 4, cy - 8, 8, 9, to565(0x2A5A34));
    rect(cx - 3, cy - 13, 6, 5, to565(0x2E7A4A));
    px(cx - 2, cy - 11, to565(0x9FFFB0));
    px(cx + 2, cy - 11, to565(0x9FFFB0));
  }

  draw_weather();

  /* Her X wears whatever the peddler put in it: flame, a violet bolt, the
   * same white shard she started with — and a look of its own once two or
   * three elements are in it. */
  {
    int xlook, n = power_count();
    uint16_t scol = to565(0x9FE8FF);
    if (n >= 3) xlook = 7;                                   /* vapour */
    else if (n == 2) xlook = (pw_fire && pw_ice) ? 4         /* mist */
                          : (pw_fire ? 5 : 6);              /* plasma, storm */
    else if (pw_fire) xlook = 1;
    else if (pw_elec) xlook = 3;
    else if (pw_ice) xlook = 2;
    else xlook = 0;
    if (xlook == 1) scol = to565(0xFF8A3C);
    else if (xlook == 3) scol = to565(0xC9A6FF);
    else if (xlook == 2) scol = to565(0xBFE6FF);
    for (i = 0; i < MAX_SHARDS; i++) {
      int sx, sy;
      if (!shards[i].live) continue;
      sx = (int)(shards[i].x - cam);
      sy = (int)shards[i].y;
      if (xlook == 1) {
        /* a small ball of flame with its tail behind it */
        rect(sx - 3, sy, 3, 1, scol);
        px(sx, sy, to565(0xFFD166));
        px(sx + 1, sy, to565(0xFFF6D8));
      } else if (xlook == 3) {
        /* a violet bolt, jagged end to end */
        px(sx, sy - 1, scol);
        px(sx + 1, sy, to565(0xF3E8FF));
        px(sx + 2, sy - 1, scol);
      } else if (xlook == 4) {
        /* hot mist: a soft pale cloud with the ember still in it */
        px(sx - 4, sy, to565(0xCFC4C4));
        rect(sx - 3, sy - 1, 2, 1, to565(0xDCE6EC));
        px(sx, sy, to565(0xEFE6E0));
        px(sx + 1, sy - 1, to565(0xFFFFFF));
        px(sx + 1, sy + 1, to565(0xFF8A3C));
      } else if (xlook == 5) {
        /* plasma: the charge sheathed in flame */
        rect(sx - 3, sy, 2, 1, to565(0xFF8A3C));
        px(sx, sy - 1, to565(0xC9A6FF));
        px(sx + 1, sy, to565(0xFFFFFF));
        px(sx + 2, sy - 1, to565(0xC9A6FF));
      } else if (xlook == 6) {
        /* storm: cold lightning with a crystal riding on it */
        px(sx, sy - 1, to565(0x5CC0FF));
        px(sx + 1, sy, to565(0xEAFAFF));
        px(sx + 2, sy - 1, to565(0x5CC0FF));
        px(sx + 2, sy - 3, to565(0xDFF3FF));
      } else if (xlook == 7) {
        /* all three: the flame tail, the violet charge, the cold head */
        rect(sx - 4, sy, 2, 1, to565(0xFF8A3C));
        px(sx - 2, sy + 1, to565(0xDCE6EC));
        px(sx, sy - 1, to565(0xC9A6FF));
        px(sx + 1, sy, to565(0xFFFFFF));
        px(sx + 2, sy - 1, to565(0xBFE6FF));
      } else {
        rect(sx, sy, 3, 1, to565(0xFFFFFF));
        px(sx - 2, sy, scol);
      }
    }
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
  } else if (boss.kind == 3) {
    /* the Forest Fairy: a glow with wings, a green dress and a wand */
    int wing = (int)(sinf(boss.bob * 9.0f) * 2.0f);
    disc(bx, by - 14, 13, to565(0x16422A));
    disc(bx, by - 14, 10, to565(0x226B3A));
    disc(bx - 8, by - 18 + wing, 5, to565(0xBFF0C0));
    disc(bx + 8, by - 18 - wing, 5, to565(0xBFF0C0));
    rect(bx - 5, by - 12, 10, 12, to565(0x3FC24A));
    rect(bx - 6, by - 14, 12, 3, to565(0x7DE08A));
    rect(bx - 4, by - 19, 8, 6, to565(0xE8C0A0));
    rect(bx - 5, by - 20, 10, 3, to565(0x8AE060));
    px(bx - 2, by - 16, to565(0x1A2A12));
    px(bx + 2, by - 16, to565(0x1A2A12));
    px(bx, by - 22, to565(0xFFF0A0));
    px(bx - 2, by - 22, to565(0xFFE060));
    px(bx + 2, by - 22, to565(0xFFE060));
    rect(bx + 7, by - 18, 1, 9, to565(0x8A6238));
    px(bx + 7, by - 19, to565(0xFFF0A0));
    /* while the rain is strung, a green line gathers over her */
    if (boss.rain_t > 0.0f) rect(bx - 7, by - 27, 15, 2, to565(0x9FFFB0));
  } else if (boss.kind == 4) {
    /* the Love Goddess: pink, crowned, with her heart already in her hand */
    int wing = (int)(sinf(boss.bob * 5.0f) * 2.0f);
    disc(bx, by - 22, 14, to565(0x4A1230));
    disc(bx, by - 22, 11, to565(0x7A1E4A));
    disc(bx - 10, by - 26 + wing, 5, to565(0xFFB0D8));
    disc(bx + 10, by - 26 - wing, 5, to565(0xFFB0D8));
    rect(bx - 6, by - 18, 12, 18, to565(0xFF7AB0));
    rect(bx - 8, by - 6, 16, 3, to565(0xFFB0D8));
    rect(bx - 5, by - 26, 10, 9, to565(0xE8C0A0));
    rect(bx - 8, by - 28, 16, 4, to565(0x8A2A50));
    rect(bx - 8, by - 25, 3, 12, to565(0x8A2A50));
    rect(bx + 6, by - 25, 3, 12, to565(0x8A2A50));
    px(bx - 2, by - 22, to565(0x1A1020));
    px(bx + 2, by - 22, to565(0x1A1020));
    px(bx - 3, by - 20, to565(0xFF5A96));
    px(bx + 3, by - 20, to565(0xFF5A96));
    for (i = -5; i <= 5; i += 5) rect(bx + i, by - 31, 3, 3, to565(0xFFD166));
    px(bx, by - 33, to565(0xFF5A96));
    /* the heart she is holding out */
    disc(bx + 9, by - 18, 2, to565(0xFF5A96));
    disc(bx + 13, by - 18, 2, to565(0xFF5A96));
    rect(bx + 8, by - 17, 7, 2, to565(0xFF5A96));
    px(bx + 10, by - 14, to565(0xFFB0D8));
    /* while she gathers, the heart in her hand swells */
    if (boss.cast_t > 0.0f) disc(bx + 12, by - 18, 5, to565(0xFFB0D8));
  } else if (boss.kind == 5) {
    /* the Ocean Rage God: a tall figure of the tide, coral on his brow and
     * a trident in his hand */
    int k;
    disc(bx, by - 34, 15, to565(0x0E3A48));
    disc(bx, by - 34, 12, to565(0x1D6A7A));
    rect(bx - 10, by - 30, 20, 30, to565(0x1D5A60));
    rect(bx - 13, by - 24, 26, 6, to565(0x2A8AA0));
    rect(bx - 7, by - 30, 14, 10, to565(0x8FC9BF));
    rect(bx - 6, by - 40, 12, 11, to565(0x9FD8C8));
    px(bx - 3, by - 36, to565(0x0A2030));
    px(bx + 3, by - 36, to565(0x0A2030));
    rect(bx - 5, by - 38, 10, 2, to565(0xC9F0E4));
    for (k = 0; k < 3; k++) {
      int cx = bx - 5 + k * 5;
      rect(cx, by - 45, 1, 6, to565(0xE0607A));
      px(cx + 1, by - 46, to565(0xFF9AC8));
    }
    /* the trident */
    rect(bx + 15, by - 46, 2, 40, to565(0x8A6238));
    rect(bx + 12, by - 50, 8, 2, to565(0xCFEAE4));
    rect(bx + 11, by - 48, 2, 3, to565(0xCFEAE4));
    rect(bx + 19, by - 48, 2, 3, to565(0xCFEAE4));
    rect(bx + 15, by - 52, 2, 3, to565(0xCFEAE4));
    rect(bx - 8, by - 8, 6, 8, to565(0x143A48));
    rect(bx + 2, by - 8, 6, 8, to565(0x143A48));
    for (k = 0; k < 3; k++) {
      int sw = 18 - k * 4;
      rect(bx - sw / 2, by - 2 + k, sw, 1, to565(0x2A8AA0));
    }
    if (boss.cast_t > 0.0f) disc(bx + 26, by - 46, 4, to565(0xEAFAFF));
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
      /* It rolls along the planks, so it is hung off the deck and not off the
       * waterline: the wall stands on the boards instead of under them. */
      int dk = (int)((has_bridge && boss.base_y > 1.0f) ? arena_y : boss.base_y - 26.0f);
      for (i = 0; i < 3; i++) {
        int bw = 120 + i * 40;
        int top = dk - 30 - i * 6;
        rect(bx - bw / 2, top, bw, dk - 2 - top, i == 2 ? to565(0xDFF8F4) : to565(0xCFEAE4));
      }
    }
  }
  if (boss.flash > 0.0f) rect(bx - 18, by - 46, 36, 46, to565(0xFFFFFF));
}

/* The love and the tide, over the road she stands on. Hearts, kisses and fish
 * come out of one pool, and there is never more than one wave and one storm. */
static void draw_weather(void) {
  int i;
  for (i = 0; i < MAX_BOLTS; i++) {
    int bx, by;
    const Bolt *b = &bolts[i];
    if (!b->live) continue;
    bx = (int)(b->x - cam);
    by = (int)b->y;
    if (bx < -10 || bx > W + 10) continue;
    if (b->kind == 0) {
      /* a thrown heart: two lobes over a point, with the poison on it */
      disc(bx - 2, by - 1, 2, to565(0xFF5A96));
      disc(bx + 2, by - 1, 2, to565(0xFF5A96));
      rect(bx - 3, by, 7, 1, to565(0xFF5A96));
      rect(bx - 2, by + 1, 5, 1, to565(0xFF8AC0));
      px(bx, by + 2, to565(0xFFB0D8));
      px(bx, by + 3, to565(0xFFB0D8));
      px(bx - 1, by + 4, to565(0x9FFFB0));
    } else if (b->kind == 1) {
      /* a blown kiss: lips, and a green vapour trailing behind */
      rect(bx - 3, by, 3, 2, to565(0xFF5A96));
      rect(bx + 1, by, 3, 2, to565(0xFF5A96));
      px(bx - 1, by + 1, to565(0xB0264E));
      px(bx + 1, by + 1, to565(0xB0264E));
      px(bx - 6, by + 1, to565(0x7DE08A));
      px(bx - 5, by - 1, to565(0x4FB06A));
    } else {
      /* a fish out of the sky: body, tail and one bright eye */
      rect(bx - 4, by - 1, 6, 3, to565(0x7FD8FF));
      rect(bx - 3, by - 2, 4, 1, to565(0xBFE9FF));
      px(bx + 3, by - 2, to565(0x7FD8FF));
      px(bx + 3, by + 1, to565(0x7FD8FF));
      px(bx + 4, by, to565(0xBFE9FF));
      px(bx + 1, by, to565(0x0A2030));
    }
  }
  if (wave.live) {
    int wx = (int)(wave.x - cam);
    int wy = (int)ground_y(wave.x);
    int s;
    for (s = 0; s < 20; s++) {
      int hw = 10 + s / 3;
      rect(wx - hw, wy - 20 + s, hw * 2, 1,
           s < 3 ? to565(0xDFF8F4) : s < 8 ? to565(0x4FA8C8) : to565(0x2A6A8A));
    }
    disc(wx + wave.dir * 9, wy - 18, 4, to565(0xEAFAFF));
    px(wx + wave.dir * 13, wy - 20, to565(0xDFF8F4));
  }
  if (storm.live) {
    int sx = (int)(storm.x - cam);
    int sy = (int)ground_y(storm.x);
    int k;
    for (k = 0; k < 34; k++) {
      float a = (float)k * 0.55f + storm.life * 6.0f;
      int r = 4 + (k / 5) * 2;
      px(sx + (int)(cosf(a) * (float)r), sy - 4 - k,
         k < 12 ? to565(0xBFE9FF) : to565(0x7FD8FF));
      if ((k & 1) == 0)
        px(sx + (int)(cosf(a + 3.14159f) * (float)r), sy - 4 - k, to565(0x4FA8C8));
    }
    disc(sx, sy - 4, 7, to565(0x9FE0F0));
  }
}

/* Her life runs purple at full, then blue, green, yellow, orange and red.
 * At nothing there is no colour left: the pips go black. */
static uint16_t hp_colour(float frac) {
  if (frac <= 0.001f) return to565(0x050505);
  if (frac >= 0.999f) return to565(0xB06CFF);
  if (frac > 0.75f) return to565(0x4F8DFF);
  if (frac > 0.5f) return to565(0x4FD06A);
  if (frac > 0.3f) return to565(0xFFD84F);
  if (frac > 0.15f) return to565(0xFF9838);
  return to565(0xFF4A3C);
}

static void draw_hud(void) {
  char buf[16];
  int i;
  float hfrac = max_hp > 0 ? (float)p.hp / (float)max_hp : 0.0f;
  uint16_t hcol = hp_colour(hfrac);

  /* her ward: its own bar, set above her life, and only once she owns it */
  if (shield_on) {
    rect(6, 1, 44, 3, to565(0x101018));
    if (shield_hp > 0) rect(7, 2, 42, 1, to565(0x7FD0FF));
  }

  for (i = 0; i < max_hp && i < 5; i++) {
    uint16_t c = i < p.hp ? hcol : to565(0x141414);
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

  if (boss.alive && boss.active && boss.kind == 2 && boss.steam_t > 0.0f) {
    text_center("STEAM COMING - JUMP IT", H - 12, 1, to565(0xFFE0A0));
  } else if (fball.live) {
    text_center("FIREBALL - JUMP IT", H - 12, 1, to565(0xFFB066));
  } else if (wave.live) {
    text_center("THE WAVE - JUMP IT", H - 12, 1, to565(0x9FE0F0));
  } else if (storm.live) {
    text_center("TORNADO - RUN", H - 12, 1, to565(0x9FE0F0));
  }

  if (banner > 0.0f) {
    text_center(STAGES[stage_i].name, 56, 1, to565(0xF0ECDC));
    text_center("COLLECT THE MEMORIES", 66, 1, to565(0x9FB4C4));
    if (stage_i == 0 && banner > 1.2f) {
      text_center("LEFT RIGHT MOVE   X JUMP   C SHARD", H - 22, 1, to565(0xB8C4D0));
      text_center("HOLD C TO RUN - IT SPENDS HER BREATH", H - 12, 1, to565(0x8D869F));
      text_center("LEFT+RIGHT: THE MENU", H - 32, 1, to565(0x8D869F));
    }
  }
  if (keeper_t > 0.0f) text_center("THE GLADE CHANGES HANDS", 84, 1, to565(0xFFD9A0));

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

/* Half-brightness copy of the road, so a window can be read over it. */
static void dim_screen(void) {
  int i, x;
  for (i = 0; i < H; i++) {
    uint16_t *row = &fb[i * W];
    for (x = 0; x < W; x++) {
      uint16_t c = row[x];
      int r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
      row[x] = (uint16_t)(((r >> 1) << 11) | ((g >> 1) << 5) | (b >> 1));
    }
  }
}

static void draw_shop(void) {
  char buf[16];
  int i, y;

  /* dim the road behind the window */
  dim_screen();

  rect(24, 8, W - 48, H - 16, to565(0x161220));
  rect(24, 8, W - 48, 1, to565(0x8E7FB0));
  rect(24, H - 9, W - 48, 1, to565(0x8E7FB0));
  rect(24, 8, 1, H - 16, to565(0x8E7FB0));
  rect(W - 25, 8, 1, H - 16, to565(0x8E7FB0));

  text_center("THE SOUL PEDDLER", 14, 1, to565(0xF0E8F8));
  mem_str(buf, souls);
  text(buf, W - 30 - text_w(buf, 1), 24, 1, to565(0xD8C9A8));

  for (i = 0; i < SHOP_ROWS; i++) {
    int owned = 0;
    const char *label;
    y = 36 + i * 15;
    if (i == shop_sel) {
      rect(28, y - 3, W - 56, 11, to565(0x2A2140));
      px(30, y + 1, to565(0xFFD9A0));
      px(30, y + 2, to565(0xFFD9A0));
      px(31, y + 2, to565(0xFFD9A0));
      px(30, y + 3, to565(0xFFD9A0));
    }
    if (i == SHOP_ITEMS) {
      text("LEAVE THE STALL", 38, y, 1, shop_sel == i ? to565(0xFFFFFF) : to565(0xB9B2C8));
      text("WALK ON", W - 60, y, 1, to565(0x8A8296));
      continue;
    }
    label = SHOP_NAME[i];
    if (i == 0) owned = shield_hp > 0 ? 2 : 0;
    else if (i == 1) owned = pw_fire;
    else if (i == 2) owned = pw_ice;
    else if (i == 3) owned = pw_elec;
    else if (i == 4) owned = auto_kill;
    else owned = (p.hp >= max_hp) ? 2 : 0;

    if (owned == 1) {
      text(label, 38, y, 1, to565(0x6B6478));
      text("OWNED", W - 60, y, 1, to565(0x6B6478));
      continue;
    }
    if (owned == 2) {
      text(label, 38, y, 1, to565(0x6B6478));
      text(i == 0 ? "WARDED" : "WHOLE", W - 60, y, 1, to565(0x6B6478));
      continue;
    }
    text(label, 38, y, 1, souls >= SHOP_COST[i] ? to565(0xF6E2C0) : to565(0x8A8296));
    cost_str(buf, SHOP_COST[i]);
    text(buf, W - 30 - text_w(buf, 1), y, 1,
         souls >= SHOP_COST[i] ? to565(0xFFD9A0) : to565(0x8A8296));
  }

  if (shop_sel < SHOP_ITEMS) text_center(SHOP_DESC[shop_sel], 142, 1, to565(0xC9C1D6));
  else text_center("SHE TAKES HER SOULS WITH HER.", 142, 1, to565(0xC9C1D6));

  if (shop_t > 0.25f) {
    text_center("LEFT RIGHT CHOOSE   C BUY   X GO", H - 12, 1, to565(0x8D869F));
  }
}

/* ---- the pause menu ---------------------------------------------------- */
/* Four buttons have no room for a fifth, so both directions at once opens
 * this. It holds the two things a bad fight needs: the same road, from its
 * beginning, and the whole journey, from its first. There is no sound here,
 * so the desktop menu's SOUND row has nothing to do on this board. */
#define PAUSE_ROWS 3
static const char *PAUSE_NAME[PAUSE_ROWS] = {
  "RESUME", "RESTART THIS ROAD", "BEGIN AGAIN",
};

static void draw_pause(void) {
  int i, y;

  dim_screen();
  rect(64, 34, W - 128, 112, to565(0x161220));
  rect(64, 34, W - 128, 1, to565(0x8E7FB0));
  rect(64, 145, W - 128, 1, to565(0x8E7FB0));
  rect(64, 34, 1, 112, to565(0x8E7FB0));
  rect(W - 65, 34, 1, 112, to565(0x8E7FB0));

  text_center("PAUSED", 42, 2, to565(0xF0ECDC));
  text_center(STAGES[stage_i].name, 60, 1, to565(0x9089A2));

  for (i = 0; i < PAUSE_ROWS; i++) {
    y = 76 + i * 15;
    if (i == pause_sel) {
      rect(70, y - 3, W - 140, 11, to565(0x2A2140));
      px(72, y + 1, to565(0xFFD9A0));
      px(72, y + 2, to565(0xFFD9A0));
      px(73, y + 2, to565(0xFFD9A0));
      px(72, y + 3, to565(0xFFD9A0));
    }
    text(PAUSE_NAME[i], 80, y, 1, i == pause_sel ? to565(0xFFFFFF) : to565(0xB9B2C8));
  }

  text_center("LEFT RIGHT CHOOSE   C GO   X BACK", 130, 1, to565(0x8D869F));
  text_center("LEFT RIGHT CHOOSE   C GO   X BACK", H - 12, 1, to565(0x8D869F));
}

static void update_pause(int left_pressed, int right_pressed, int jump_pressed, int action_pressed) {
  if (left_pressed) pause_sel = (pause_sel + PAUSE_ROWS - 1) % PAUSE_ROWS;
  if (right_pressed) pause_sel = (pause_sel + 1) % PAUSE_ROWS;
  if (jump_pressed) { state = 0; return; }
  if (!action_pressed) return;

  if (pause_sel == 0) {
    state = 0;
  } else if (pause_sel == 1) {
    /* The road again from its first step, her breath whole, and everything
     * she has bought still hers — the stall is waiting at the start of it. */
    p.hp = max_hp;
    reset_stage(stage_i);
  } else {
    game_init();
  }
}

/* ---- public API -------------------------------------------------------- */
void game_update(float dt, GameInput in) {
  static GameInput prev;
  int jump_pressed = in.jump && !prev.jump;
  int action_pressed = in.action && !prev.action;
  int left_pressed = in.left && !prev.left;
  int right_pressed = in.right && !prev.right;
  /* both directions at once: the way into the menu */
  int menu_pressed = in.left && in.right && !(prev.left && prev.right);
  prev = in;

  if (dt <= 0) dt = 0.016f;
  if (dt > 0.1f) dt = 0.1f;
  if (banner > 0.0f) banner -= dt;
  if (keeper_t > 0.0f) keeper_t -= dt;

  if (state == 1) {
    state_t -= dt;
    if (state_t <= 0) {
      /* The next road begins at the peddler's stall. */
      if (stage_i < 3) reset_stage(stage_i + 1);
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
  } else if (state == 5) {
    update_pause(left_pressed, right_pressed, jump_pressed, action_pressed);
  } else {
    if (menu_pressed) {
      state = 5;
      pause_sel = 0;
    } else {
      update_play(dt, in, jump_pressed, action_pressed);
    }
  }

  draw_world();
  draw_hud();
  if (state == 4) draw_shop();
  else if (state == 5) draw_pause();
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
int game_boss_hp(void) { return (int)(boss.hp + 0.5f); }
int game_shield(void) { return shield_hp; }
int game_shield_owned(void) { return shield_on; }
int game_powers(void) { return power_count(); }
int game_bots_killed(void) { return bots_killed; }

#ifdef GAME_TEST
void game_test_souls(int n) { souls += n; }

/* Host harness only: jump straight to a road and one of its keepers, with a
 * whole heart bar, so each keeper's weather can be watched on its own. */
void game_test_warp(int stage, int keeper) {
  int qi = 0;
  reset_stage(stage);
  while (qi < keeper && qi + 1 < KEEPERS) {
    if (BOSS_KIND[stage][qi + 1] < 0) break;
    qi++;
    boss_qi = qi;
  }
  setup_boss(BOSS_KIND[stage][qi] >= 0 ? BOSS_KIND[stage][qi] : 0);
  boss.intro = 0.2f;
  p.hp = max_hp;
  p.x = clampf(boss.home - 120.0f, 8.0f, stage_len - 8.0f);
  p.y = ground_y(p.x);
  p.vx = 0;
  p.vy = 0;
  p.on_ground = 1;
  state = 0;
  state_t = 0;
  keeper_t = 0.0f;
  banner = 1.0f;
}

/* Host harness only: what of a keeper's weather is on the board right now. */
int game_test_fx(int which) {
  int i, n = 0;
  if (which == 1) return wave.live;
  if (which == 2) return storm.live;
  for (i = 0; i < MAX_BOLTS; i++) {
    if (!bolts[i].live) continue;
    if (which == 0) n++;
    else if (which == 3 && bolts[i].kind == 0) n++;
    else if (which == 4 && bolts[i].kind == 1) n++;
    else if (which == 5 && bolts[i].kind == 2) n++;
  }
  return n;
}
#endif

/* Extras for host tools and tests. */
float game_ground_at(float x) { return ground_y(x); }
float game_player_x(void) { return p.x; }
