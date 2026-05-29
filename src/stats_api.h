// stats_api.h — thin namespace over the reference's header-only stats.h.
// stats.h keeps file-static state and must be compiled in ONE translation
// unit (stats_api.cpp); everyone else calls through here.
#pragma once
#include <stdint.h>

namespace st {
  void begin();                       // load stats + settings + names from NVS

  // events
  void onApproval(uint32_t secondsToRespond);
  void onDenial();
  void onBridgeTokens(uint32_t bridgeCumulativeTokens);
  bool pollLevelUp();                 // true once after a level boundary is crossed
  void onWake();
  void onNapEnd(uint32_t seconds);

  // getters for display + status ack
  uint8_t  level();
  uint16_t approvals();
  uint16_t denials();
  uint32_t tokens();
  uint8_t  fedProgress();             // 0..10 within current level
  uint8_t  moodTier();                // 0..4
  uint8_t  energyTier();              // 0..5
  uint16_t medianVelocity();          // seconds, 0 if no data

  // identity
  const char *petName();   void setPetName(const char *);
  const char *owner();     void setOwner(const char *);
}
