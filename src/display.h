// display.h — CO5300 AMOLED via an offscreen PSRAM canvas (Arduino_GFX).
// Everything draws into `gfx` (the canvas); display::flush() blits to the panel.
// No LVGL — the buddy renderer and screens draw directly, like the reference.
#pragma once
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx;   // the canvas — draw here, then display::flush()

namespace display {
  bool begin();
  void flush();              // push canvas framebuffer to the panel
  void setBrightness(uint8_t v);
  int16_t width();
  int16_t height();
}
