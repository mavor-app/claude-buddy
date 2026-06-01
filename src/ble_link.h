// ble_link.h — NimBLE peripheral exposing the Nordic UART Service with
// LE Secure Connections bonding. Independent of protocol.cpp to avoid cycles:
// inbound lines are queued and drained by the main loop.
#pragma once
#include <Arduino.h>

namespace ble {
  void begin();                       // init stack, service, advertising
  void send(const String &line);      // notify TX (no-op if not subscribed)
  bool nextLine(String &out);         // pop one reassembled inbound line; false if none
  void eraseBonds();                  // {"cmd":"unpair"}
  void ensureAdvertising();           // re-advertise if disconnected (call from loop)
}
