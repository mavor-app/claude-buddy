#include "usage.h"
#include <Arduino.h>

// 30 buckets × 10 min = 5 hours. Each bucket holds tokens consumed in its slot;
// a bucket older than the window is overwritten (and thus excluded) on reuse.
static const int      NB = 30;
static const uint32_t BUCKET_MS = 10UL * 60 * 1000;

static uint32_t buck[NB]      = {0};
static uint32_t buckEpoch[NB] = {0};   // millis()/BUCKET_MS index this bucket holds
static uint32_t lastCum = 0;
static bool     have = false;

namespace usage {

void onTokens(uint32_t cum) {
  if (!have) { lastCum = cum; have = true; return; }   // first sight: just baseline
  uint32_t delta = (cum >= lastCum) ? (cum - lastCum) : 0;  // desktop restart -> tokens drops; skip
  lastCum = cum;

  uint32_t idx = millis() / BUCKET_MS;
  int slot = idx % NB;
  if (buckEpoch[slot] != idx) { buck[slot] = 0; buckEpoch[slot] = idx; }
  buck[slot] += delta;
}

uint32_t last5h() {
  uint32_t idx = millis() / BUCKET_MS;
  uint32_t sum = 0;
  for (int i = 0; i < NB; i++)
    if (idx - buckEpoch[i] < (uint32_t)NB) sum += buck[i];  // within window
  return sum;
}

} // namespace usage
