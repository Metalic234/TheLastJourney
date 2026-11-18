/* Host harness: run the pocket edition without a display and dump frames.
 *   cc -O2 -std=c99 -o tlj_pocket host/main_host.c TheLastJourney/game.c -lm
 *   ./tlj_pocket out/
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

  game_init();
  int shop_frames = 0;
  int shop_shots = 0;
  for (f = 0; f < 6000; f++) {
    GameInput in;
    memset(&in, 0, sizeof(in));

    if (game_state() == 4) {
      /* the peddler: walk the rows to the bot killer, buy it, then take the road */
      shop_frames++;
      if (shop_frames == 2) { write_ppm(dir, f); printf("shop window at frame %d (purse %d)\n", f, game_purse()); }
      in.left = (shop_frames == 1 || shop_frames == 3);
      in.action = shop_frames == 5;
      if (shop_frames > 40 && (shop_frames % 2) == 0) in.jump = 1;
      game_update(dt, in);
      if (shop_frames == 7) {
        printf("bot killer owned at frame %d (purse %d)\n", f, game_purse());
      }
      continue;
    }
    if (shop_frames) { shop_shots++; shop_frames = 0; }

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
  printf("stage=%d hp=%d/%d purse=%d memories=%d/%d shops=%d finished=%d\n",
         game_stage(), game_hp(), game_max_hp(), game_purse(), game_memories(),
         game_total_memories(), shop_shots, game_finished());
  printf("auto_kill=%d bots_killed=%d\n", game_auto_kill(), game_bots_killed());
  return 0;
}
