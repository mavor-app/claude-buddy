// app_state.h — shared snapshot/link state, mutated only on the main loop.
// (Stats live in stats_api; persona state machine lives in ui.)
#pragma once
#include <Arduino.h>
#include "config.h"

struct PendingPrompt {
  bool   active = false;
  String id;       // echo verbatim in the reply
  String tool;
  String hint;
};

struct AppState {
  // last heartbeat snapshot
  int      total = 0, running = 0, waiting = 0;
  String   msg;
  String   entries[MAX_ENTRIES];
  int      entryCount = 0;
  uint32_t tokens = 0, tokensToday = 0;

  PendingPrompt prompt;
  uint32_t promptArrivedMs = 0;   // for "approve? Ns" + fast-approve heart
  bool     responseSent = false;

  // link / lifecycle
  bool      connected = false;
  bool      encrypted = false;     // sec:true
  uint32_t  lastSnapshotMs = 0;
  uint32_t  snapCount = 0;         // total heartbeats received (debug)
  uint32_t  rxBytes = 0;           // total bytes received over RX (debug)
  uint16_t  disconnects = 0;       // BLE drop count this boot (debug)

  // pairing
  bool      showPasskey = false;
  uint32_t  passkey = 0;

  uint32_t  bootMs = 0;
};

extern AppState g_state;
