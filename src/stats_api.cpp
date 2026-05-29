// The ONE translation unit that compiles stats.h (file-static state inside).
#include "stats_api.h"
#include "stats.h"

namespace st {

void begin() { statsLoad(); settingsLoad(); petNameLoad(); }

void onApproval(uint32_t s)       { statsOnApproval(s); }
void onDenial()                   { statsOnDenial(); }
void onBridgeTokens(uint32_t t)   { statsOnBridgeTokens(t); }
bool pollLevelUp()                { return statsPollLevelUp(); }
void onWake()                     { statsOnWake(); }
void onNapEnd(uint32_t s)         { statsOnNapEnd(s); }

uint8_t  level()          { return stats().level; }
uint16_t approvals()      { return stats().approvals; }
uint16_t denials()        { return stats().denials; }
uint32_t tokens()         { return stats().tokens; }
uint8_t  fedProgress()    { return statsFedProgress(); }
uint8_t  moodTier()       { return statsMoodTier(); }
uint8_t  energyTier()     { return statsEnergyTier(); }
uint16_t medianVelocity() { return statsMedianVelocity(); }

const char *petName()              { return ::petName(); }
void        setPetName(const char *n) { petNameSet(n); }
const char *owner()                { return ownerName(); }
void        setOwner(const char *n){ ownerSet(n); }

} // namespace st
