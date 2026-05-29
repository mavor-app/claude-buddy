// touch.h — FT3168 capacitive touch, raw coordinates (no LVGL).
#pragma once
namespace touch {
  bool begin();
  // True if a finger is down; fills x,y (panel pixels) when so.
  bool get(int &x, int &y);
}
