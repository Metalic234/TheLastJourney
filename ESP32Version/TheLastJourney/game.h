#ifndef THE_LAST_JOURNEY_GAME_H
#define THE_LAST_JOURNEY_GAME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_W 320
#define GAME_H 180

typedef struct {
  uint8_t left;
  uint8_t right;
  uint8_t jump;
  uint8_t action;
} GameInput;

/* Boot the pocket edition. */
void game_init(void);

/* Advance one frame. dt is in seconds (clamped internally). */
void game_update(float dt, GameInput in);

/* 16-bit RGB565 framebuffer, GAME_W * GAME_H, top row first. */
const uint16_t *game_framebuffer(void);

int game_stage(void);
int game_hp(void);
int game_memories(void);
int game_total_memories(void);
int game_finished(void);
int game_state(void);
int game_purse(void);
int game_max_hp(void);
int game_auto_kill(void);
int game_boss_kind(void);
int game_boss_hp(void);
int game_shield(void);
int game_shield_owned(void);
int game_powers(void);
int game_bots_killed(void);
int game_souls(void);
int game_stamina(void);

#ifdef GAME_TEST
/* Host harness only: hand the run some souls so the wares can be exercised. */
void game_test_souls(int n);
#endif

/* Extras for host tools and tests. */
float game_ground_at(float x);
float game_player_x(void);

#ifdef __cplusplus
}
#endif

#endif
