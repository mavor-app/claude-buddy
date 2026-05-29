#include "display.h"
#include "config.h"

static Arduino_DataBus  *bus   = nullptr;
static Arduino_CO5300   *panel = nullptr;
static Arduino_Canvas   *canvas = nullptr;
Arduino_GFX             *gfx   = nullptr;   // == canvas

namespace display {

bool begin() {
  bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
  // col_off1 = 22 (0x16): this panel's visible area starts at column 22.
  panel = new Arduino_CO5300(bus, LCD_RST, 0 /*rot*/, false /*IPS*/,
                             LCD_WIDTH, LCD_HEIGHT, 22, 0, 0, 0);

  // Offscreen framebuffer in PSRAM; flush() pushes it to the panel. This is
  // the flicker-free equivalent of the reference's TFT_eSprite.
  canvas = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);
  gfx = canvas;
  if (!canvas->begin()) {            // also inits the panel + QSPI bus
    Serial.println("[display] canvas/panel begin() failed (PSRAM?)");
    return false;
  }
  gfx->fillScreen(BLACK);
  gfx->flush();
  setBrightness(LCD_BRIGHTNESS);
  Serial.println("[display] ready (canvas)");
  return true;
}

void flush() { if (canvas) canvas->flush(); }

void setBrightness(uint8_t v) { if (panel) panel->setBrightness(v); } // reg 0x51

int16_t width()  { return LCD_WIDTH; }
int16_t height() { return LCD_HEIGHT; }

} // namespace display
