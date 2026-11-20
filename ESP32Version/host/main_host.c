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
  int hp_at_stage[3] = { 0, 0, 0 };

  game_init();

  for (f = 0; f < 12000; f++) {
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
        if (shop_frames == 3 || shop_frames == 5) in.right = 1;
        if (shop_frames == 7) in.action = 1;
      } else if (shop_visits == 2) {
        /* stage II: buy the guardian, never the boss-killer */
        if (shop_frames == 1) game_test_souls(200);
        if (shop_frames == 2) in.action = 1;
        if (shop_frames == 3) {
          printf("guardian owned %d souls %d\n", game_auto_kill(), game_souls());
          hp_at_stage[1] = game_hp();
        }
        if (shop_frames == 4 || shop_frames == 6) in.right = 1;
        if (shop_frames == 8) in.action = 1;
      } else {
        /* later roads: look, then go */
        if (shop_frames == 2) hp_at_stage[2] = game_hp();
        if (shop_frames == 3 || shop_frames == 5) in.right = 1;
        if (shop_frames == 7) in.action = 1;
      }
      game_update(dt, in);
      continue;
    }

    in_shop = 0;
    in.right = 1;
    /* jump when a pit is just ahead, otherwise hop now and then */
    if (game_ground_at(game_player_x() + 20.0f) > 300.0f && (f % 2) == 0) in.jump = 1;
    in.action = (f % 20) == 0 ? 1 : 0;

    game_update(dt, in);
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
  return 0;
}
