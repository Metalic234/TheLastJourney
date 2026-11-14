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

/* Extras for host tools and tests. */
float game_ground_at(float x);
float game_player_x(void);

#ifdef __cplusplus
}
#endif

#endif
