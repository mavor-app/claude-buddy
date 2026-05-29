// power.h — AXP2101 PMIC: turn on panel power rail, read battery.
#pragma once
#include <Arduino.h>

struct BatteryInfo {
  bool present = false;
  int  pct = 0;     // 0..100
  int  mV = 0;      // battery voltage
  int  mA = 0;      // charge/discharge current; negative == charging (per spec)
  bool usb = false; // VBUS present
};

namespace power {
  // Init I2C PMIC and enable the display power rail (DSI_PWR_EN). Returns false
  // if the AXP2101 isn't found — display will stay dark, so log loudly.
  bool begin();
  BatteryInfo read();
}
