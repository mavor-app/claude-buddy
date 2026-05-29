#include "touch.h"
#include "config.h"
#include <Wire.h>

// Self-contained FT3168 reader (FT5x06/FT6x36-family register map):
//   0x02 = touch count (low nibble)
//   0x03 P1 XH (X[11:8]), 0x04 P1 XL, 0x05 P1 YH (Y[11:8]), 0x06 P1 YL

static bool readReg(uint8_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(FT3168_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)FT3168_ADDR, (int)len) != (int)len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

namespace touch {

bool begin() {
  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setClock(400000);
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);  delay(10);
  digitalWrite(TP_RST, HIGH); delay(60);

  Wire.beginTransmission(FT3168_ADDR);
  bool present = (Wire.endTransmission() == 0);
  Serial.printf("[touch] FT3168 %s @0x%02X\n", present ? "online" : "NOT found", FT3168_ADDR);
  return present;
}

bool get(int &x, int &y) {
  uint8_t r[7];
  if (!readReg(0x00, r, sizeof(r))) return false;
  if ((r[0x02] & 0x0F) == 0) return false;
  int px = ((r[0x03] & 0x0F) << 8) | r[0x04];
  int py = ((r[0x05] & 0x0F) << 8) | r[0x06];
  if (px < 0 || py < 0 || px >= LCD_WIDTH || py >= LCD_HEIGHT) return false;
  x = px; y = py;
  return true;
}

} // namespace touch
