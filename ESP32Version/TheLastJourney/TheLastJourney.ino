/*
 * The Last Journey — Pocket Edition (ESP32 + TFT)
 *
 * Wiring (change the pins below to match yours):
 *   TFT via SPI, configured in TFT_eSPI's User_Setup.h
 *   Button LEFT   -> GPIO 32 -> GND
 *   Button RIGHT  -> GPIO 33 -> GND
 *   Button JUMP   -> GPIO 25 -> GND
 *   Button SHARD  -> GPIO 26 -> GND
 * Buttons use the internal pull-ups, so they read LOW when pressed.
 *
 * The game renders into a 320x180 RGB565 buffer; the display shows it
 * letterboxed in the middle of a 320x240 panel.
 */

#include <TFT_eSPI.h>

extern "C" {
#include "game.h"
}

TFT_eSPI tft = TFT_eSPI();

static const int PIN_LEFT = 32;
static const int PIN_RIGHT = 33;
static const int PIN_JUMP = 25;
static const int PIN_ACTION = 26;

static const int Y_OFFSET = (240 - GAME_H) / 2;
static const float FRAME_TIME = 1.0f / 30.0f;

static uint32_t lastFrame = 0;

void setup() {
  pinMode(PIN_LEFT, INPUT_PULLUP);
  pinMode(PIN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_JUMP, INPUT_PULLUP);
  pinMode(PIN_ACTION, INPUT_PULLUP);

  tft.init();
  tft.setRotation(1);          /* landscape, 320x240 on an ILI9341 */
  tft.fillScreen(TFT_BLACK);

  game_init();
  lastFrame = micros();
}

void loop() {
  uint32_t now = micros();
  float dt = (now - lastFrame) / 1000000.0f;
  if (dt < FRAME_TIME) {
    delay(1);
    return;
  }
  lastFrame = now;

  GameInput in;
  in.left = digitalRead(PIN_LEFT) == LOW;
  in.right = digitalRead(PIN_RIGHT) == LOW;
  in.jump = digitalRead(PIN_JUMP) == LOW;
  in.action = digitalRead(PIN_ACTION) == LOW;

  game_update(dt, in);

  tft.pushImage(0, Y_OFFSET, GAME_W, GAME_H, (uint16_t *)game_framebuffer());
}
