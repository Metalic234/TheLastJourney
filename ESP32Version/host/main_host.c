/* Host harness: run the pocket edition without a display and dump frames.
 *   ./build-host.sh            (adds -DGAME_TEST, which exposes game_test_souls)
 *   cc -O2 -std=c99 -DGAME_TEST -o tlj_pocket host/main_host.c TheLastJourney/game.c -lm
 * The dumps are PPM (P6) so they can be inspected or converted to PNG.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"

static void write_ppm(const char *dir, int frame) {
  char path[512];
  FILE *f;
  const uint16_t *fb = game_framebuffer();
  int x, y;
  snprintf(path, sizeof(path), "%s/frame_%04d.ppm", dir, frame);
  f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", GAME_W, GAME_H);
  for (y = 0; y < GAME_H; y++) {
    for (x = 0; x < GAME_W; x++) {
      uint16_t c = fb[y * GAME_W + x];
      unsigned char rgb[3];
      rgb[0] = (unsigned char)((c >> 11) & 0x1F);
      rgb[1] = (unsigned char)((c >> 5) & 0x3F);
      rgb[2] = (unsigned char)(c & 0x1F);
      rgb[0] = (unsigned char)((rgb[0] * 255) / 31);
      rgb[1] = (unsigned char)((rgb[1] * 255) / 63);
      rgb[2] = (unsigned char)((rgb[2] * 255) / 31);
      fwrite(rgb, 1, 3, f);
    }
  }
  fclose(f);
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : ".";
  const float dt = 1.0f / 30.0f;
  int f;
  int shop_frames = 0;
  int shop_visits = 0;
  int in_shop = 0; /* the run starts at the stall */
  int hp_at_stage[4] = { 0, 0, 0, 0 };
  int seen_stage = -1;
  int boss_kind[4] = { 0, 0, 0, 0 };
  int boss_hp[4] = { 0, 0, 0, 0 };
  int last_keeper = 0;

  game_init();

  for (f = 0; f < 20000; f++) {
    GameInput in;
    memset(&in, 0, sizeof(in));

    if (game_state() == 4) {
      if (!in_shop) {
        in_shop = 1;
        shop_frames = 0;
        shop_visits++;
      }
      shop_frames++;

      if (shop_visits == 1) {
        /* stage I: look at the window, then step onto the road */
        if (shop_frames == 2) {
          write_ppm(dir, f);
          printf("shop at stage %d frame %d souls %d hp %d/%d\n",
                 game_stage(), f, game_souls(), game_hp(), game_max_hp());
        }
        if (shop_frames == 4) write_ppm(dir, 9998); /* the FIRE line, with % */
        if (shop_frames == 3 || shop_frames == 5) in.right = 1;
        if (shop_frames == 7) in.jump = 1;   /* leave the stall */
      } else if (shop_visits == 2) {
        /* stage II: buy the ward and all three elements, then the guardian */
        if (shop_frames == 1) game_test_souls(400);
        if (shop_frames >= 2 && shop_frames <= 14) {
          if ((shop_frames % 2) == 0) in.action = 1;
          else in.right = 1;
        }
        if (shop_frames == 2) write_ppm(dir, 9999);
        if (shop_frames == 13) {
          printf("ward owned %d on hand %d powers %d guardian %d souls %d\n",
                 game_shield_owned(), game_shield(), game_powers(),
                 game_auto_kill(), game_souls());
          hp_at_stage[1] = game_hp();
        }
      } else {
        /* later roads: look, then go */
        if (shop_frames == 2) hp_at_stage[2] = game_hp();
        if (shop_frames == 3 || shop_frames == 5) in.right = 1;
        if (shop_frames == 7) in.jump = 1;
      }
      game_update(dt, in);
      continue;
    }

    in_shop = 0;
    in.right = 1;
    /* jump when a pit is just ahead, otherwise hop now and then */
    if (game_ground_at(game_player_x() + 20.0f) > 300.0f && (f % 2) == 0) in.jump = 1;
    /* Keep the shards coming: a keeper needs a dozen of them. */
    in.action = (f % 6) == 0 ? 1 : 0;

    if (game_stage() != seen_stage) {
      if (seen_stage >= 0) {
        printf("left stage %d: boss kind %d at %d hp, hp %d/%d\n",
               seen_stage, boss_kind[seen_stage], boss_hp[seen_stage],
               game_hp(), game_max_hp());
      }
      seen_stage = game_stage();
    }
    boss_kind[game_stage()] = game_boss_kind();
    boss_hp[game_stage()] = game_boss_hp();
    /* every keeper that takes the glade is announced as it steps in */
    if (game_boss_kind() != last_keeper) {
      if (last_keeper > 0) {
        printf("keeper %d down, keeper %d stands (stage %d, frame %d)\n",
               last_keeper, game_boss_kind(), game_stage(), f);
      }
      last_keeper = game_boss_kind();
    }

    game_update(dt, in);
    {
      static int last_state = -1;
      if (game_state() == 2 && last_state != 2) {
        printf("died on stage %d at x %.0f (boss kind %d hp %d)\n",
               game_stage(), game_player_x(), game_boss_kind(), game_boss_hp());
      }
      last_state = game_state();
    }
    if ((f % 240) == 0) write_ppm(dir, f);
    if (game_finished()) {
      printf("finished at frame %d (stage %d)\n", f, game_stage());
      write_ppm(dir, f);
      break;
    }
  }
  printf("stage=%d hp=%d/%d souls=%d memories=%d/%d shops=%d stamina=%d finished=%d\n",
         game_stage(), game_hp(), game_max_hp(), game_souls(), game_memories(),
         game_total_memories(), shop_visits, game_stamina(), game_finished());
  printf("hp at stage II %d, at stage III %d\n", hp_at_stage[1], hp_at_stage[2]);
  printf("auto_kill=%d bots_killed=%d\n", game_auto_kill(), game_bots_killed());
  if (seen_stage >= 0) {
    boss_kind[seen_stage] = game_boss_kind();
    boss_hp[seen_stage] = game_boss_hp();
  }
  printf("bosses: stage1 kind %d hp %d, stage2 kind %d hp %d, stage3 kind %d hp %d\n",
         boss_kind[1], boss_hp[1], boss_kind[2], boss_hp[2],
         boss_kind[3], boss_hp[3]);
  return 0;
}
