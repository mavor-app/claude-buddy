// sound.h — tiny beeper over the ES8311 codec + I2S (for approval alerts).
#pragma once
#include <stdint.h>
namespace sound {
  bool begin();                          // init I2S + ES8311; false if codec absent
  void beep(int freqHz = 2200, int ms = 90);
  void alert();                          // the "permission needed" chirp
}
