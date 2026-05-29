// rtc.h — PCF85063 real-time clock; set from the desktop's {"time":[epoch,tzoff]}.
#pragma once
#include <Arduino.h>
namespace rtc {
  bool begin();
  // epoch = UTC seconds, tzOffsetSec = local offset (e.g. -25200 for PDT).
  void setFromEpoch(long epoch, long tzOffsetSec);
  bool nowHM(int &hour, int &minute);   // local time, false if not yet set
}
