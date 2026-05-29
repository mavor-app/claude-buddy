// ui.h — direct-GFX screens + persona state machine (no LVGL).
#pragma once
#include <ArduinoJson.h>

namespace ui {
  void begin();
  void onSnapshot();            // a heartbeat arrived
  void onTurn(JsonDocument &d); // optional: recent assistant text
  void render();                // draw the current screen into gfx (main flushes)
  void onApprove();             // BOOT / green button / touch
  void onDeny();                // PWR / red button / touch
  void onTouch(int x, int y);   // route a touch press to on-screen buttons
  void onNextButton();          // BOOT short-press with no prompt: cycle species
}
