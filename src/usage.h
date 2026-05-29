// usage.h — device-side rolling 5-hour OUTPUT-token meter.
//
// The BLE protocol only carries `tokens` (cumulative output since the desktop
// app started) and `tokens_today`. There is NO plan / 5-hour-limit field, so we
// approximate "5h usage" locally: bucket the deltas of `tokens` over time and
// sum the trailing 5 hours. Output tokens only — not the full plan accounting.
#pragma once
#include <stdint.h>

namespace usage {
  void onTokens(uint32_t cumulative);  // feed each heartbeat's `tokens`
  uint32_t last5h();                    // output tokens consumed in trailing ~5h
}
