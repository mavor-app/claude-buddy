#include "rtc.h"
#include "config.h"
#include <Wire.h>
#include <time.h>
#include "SensorPCF85063.hpp"

static SensorPCF85063 clk;
static bool ready = false;
static bool timeSet = false;

namespace rtc {

bool begin() {
  ready = clk.begin(Wire, IIC_SDA, IIC_SCL);  // PCF85063 addr (0x51) is fixed internally
  Serial.printf("[rtc] PCF85063 %s\n", ready ? "online" : "NOT found");
  return ready;
}

void setFromEpoch(long epoch, long tzOffsetSec) {
  if (!ready) return;
  time_t local = (time_t)(epoch + tzOffsetSec);
  struct tm tmv;
  gmtime_r(&local, &tmv);   // treat shifted epoch as local wall-clock
  clk.setDateTime(tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
  timeSet = true;
  Serial.printf("[rtc] set to %04d-%02d-%02d %02d:%02d\n",
                tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                tmv.tm_hour, tmv.tm_min);
}

bool nowHM(int &hour, int &minute) {
  if (!ready || !timeSet) return false;
  RTC_DateTime t = clk.getDateTime();
  hour = t.getHour(); minute = t.getMinute();
  return true;
}

} // namespace rtc
