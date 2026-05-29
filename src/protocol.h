// protocol.h — the BLE "cable protocol": JSON line in -> state update + ack out.
#pragma once
#include <Arduino.h>

namespace protocol {
  // Handle one complete \n-delimited JSON line from the desktop.
  void handleLine(const String &line);

  // device -> desktop helpers
  void sendPermission(const String &id, const char *decision); // "once" | "deny"
  void sendStatus();                                           // status ack
}
