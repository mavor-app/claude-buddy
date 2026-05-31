// Claude Hardware Buddy — Waveshare ESP32-S3-Touch-AMOLED-2.06
// Direct Arduino_GFX rendering (no LVGL), ported buddy character system.
#include <Arduino.h>
#include "config.h"
#include "app_state.h"
#include "stats_api.h"
#include "power.h"
#include "display.h"
#include "touch.h"
#include "rtc.h"
#include "ble_link.h"
#include "protocol.h"
#include "ui.h"
#include "sound.h"

AppState g_state;

// ── buttons (active-low) ──
static void setupButtons() {
  pinMode(BTN_APPROVE, INPUT_PULLUP);
  pinMode(BTN_DENY,    INPUT_PULLUP);
}
// Robust against a pin that sits LOW at boot (GPIO10/PWR on this board is not
// a clean active-low input): we require a HIGH (released) observation before
// accepting any press, so a stuck-low pin never triggers anything.
static void pollButtons() {
  static bool aSeenHigh = false, dSeenHigh = false;
  static uint32_t aDownAt = 0; static bool aLong = false; static bool aDown = false;
  static bool dDown = false;
  int a = digitalRead(BTN_APPROVE), d = digitalRead(BTN_DENY);
  uint32_t now = millis();
  if (a == HIGH) aSeenHigh = true;
  if (d == HIGH) dSeenHigh = true;

  // BOOT (GPIO0): short = approve/next species; long(>1.5s) = erase bond + restart.
  if (aSeenHigh && a == LOW) {
    if (!aDown) { aDown = true; aDownAt = now; aLong = false; }
    if (!aLong && now - aDownAt > 1500) {
      aLong = true;
      Serial.println("[ble] BOOT long-press: erasing bonds + restart");
      ble::eraseBonds(); delay(50); ESP.restart();
    }
  } else if (aDown && a == HIGH) {              // released
    if (!aLong) { if (g_state.prompt.active) ui::onApprove(); else ui::onNextButton(); }
    aDown = false;
  }

  // PWR (GPIO10): deny on release. No-ops if the pin never goes HIGH.
  if (dSeenHigh && d == LOW) dDown = true;
  else if (dDown && d == HIGH) { dDown = false; if (g_state.prompt.active) ui::onDeny(); }
}

static bool pollTouch() {
  static bool prev = false;
  int x, y;
  bool down = touch::get(x, y);
  if (down && !prev) ui::onTouch(x, y);        // act on press edge
  prev = down;
  return down;
}

// Idle dimming: full brightness while you're interacting or a prompt is up;
// dim after IDLE_DIM_MS, panel ~off after IDLE_OFF_MS. Touch or a new prompt
// wakes it. Returns true if the panel is currently off (so we can skip flush).
static bool manageBrightness(bool touched) {
  static uint32_t lastActive = 0;
  static int curLevel = -1;
  uint32_t now = millis();
  if (touched || g_state.prompt.active || g_state.waiting > 0) lastActive = now;
  uint32_t idle = now - lastActive;
  int want = (idle < IDLE_DIM_MS) ? BRIGHT_ACTIVE
           : (idle < IDLE_OFF_MS) ? BRIGHT_DIM : 0;
  if (want != curLevel) { display::setBrightness(want); curLevel = want; }
  return want == 0;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[boot] Claude Hardware Buddy");

  setCpuFrequencyMhz(160);           // 240->160 MHz: noticeably cooler, ample for this
  g_state.bootMs = millis();
  st::begin();                       // stats + settings + names from NVS

  if (!power::begin())   Serial.println("[boot] WARNING: PMIC init failed");
  if (!display::begin()) Serial.println("[boot] WARNING: display init failed");
  touch::begin();
  rtc::begin();
  sound::begin();                    // BEFORE ui — the codec only outputs when its
                                     // first feed happens right after init, undisturbed
  ui::begin();
  ble::begin();
  setupButtons();
  st::onWake();
  Serial.println("[boot] ready");
}

void loop() {
  // inbound protocol (BLE + dev serial injection)
  String line;
  while (ble::nextLine(line)) protocol::handleLine(line);
  static String sbuf;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n')      { protocol::handleLine(sbuf); sbuf = ""; }
    else if (c != '\r') { sbuf += c; }
  }

  pollButtons();
  bool touched = pollTouch();
  bool panelOff = manageBrightness(touched);

  // render+flush throttled to ~16 fps (buddy animates at 5 fps internally).
  // When the panel is dimmed off, skip the (expensive) full-canvas flush.
  static uint32_t nextFrame = 0;
  uint32_t now = millis();
  if (!panelOff && (int32_t)(now - nextFrame) >= 0) {
    nextFrame = now + 100;
    ui::render();
    display::flush();
  }
  delay(2);
}
