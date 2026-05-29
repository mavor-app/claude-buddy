#include "protocol.h"
#include "app_state.h"
#include "ble_link.h"
#include "stats_api.h"
#include "power.h"
#include "rtc.h"
#include "ui.h"
#include "usage.h"
#include <ArduinoJson.h>

static void sendAck(const char *cmd, bool ok, long n = 0, const char *err = nullptr) {
  JsonDocument d;
  d["ack"] = cmd; d["ok"] = ok; d["n"] = n;
  if (err) d["error"] = err;
  String out; serializeJson(d, out); out += '\n';
  ble::send(out);
}

namespace protocol {

void sendPermission(const String &id, const char *decision) {
  JsonDocument d;
  d["cmd"] = "permission"; d["id"] = id; d["decision"] = decision;
  String out; serializeJson(d, out); out += '\n';
  ble::send(out);
}

void sendStatus() {
  JsonDocument d;
  d["ack"] = "status"; d["ok"] = true;
  JsonObject data = d["data"].to<JsonObject>();
  data["name"] = st::petName();
  data["sec"]  = g_state.encrypted;

  BatteryInfo b = power::read();
  if (b.present) {
    JsonObject bat = data["bat"].to<JsonObject>();
    bat["pct"] = b.pct; bat["mV"] = b.mV; bat["mA"] = b.mA; bat["usb"] = b.usb;
  } else if (b.usb) {
    data["bat"]["usb"] = true;
  }
  JsonObject sys = data["sys"].to<JsonObject>();
  sys["up"]   = (millis() - g_state.bootMs) / 1000;
  sys["heap"] = ESP.getFreeHeap();

  JsonObject stt = data["stats"].to<JsonObject>();
  stt["appr"] = st::approvals();
  stt["deny"] = st::denials();
  stt["vel"]  = st::medianVelocity();
  stt["lvl"]  = st::level();

  String out; serializeJson(d, out); out += '\n';
  ble::send(out);
}

static void handleSnapshot(JsonDocument &d) {
  g_state.total   = d["total"]   | 0;
  g_state.running = d["running"] | 0;
  g_state.waiting = d["waiting"] | 0;
  g_state.msg     = (const char *)(d["msg"] | "");
  g_state.tokens      = d["tokens"]       | 0;
  g_state.tokensToday = d["tokens_today"] | 0;

  g_state.entryCount = 0;
  if (d["entries"].is<JsonArray>())
    for (JsonVariant v : d["entries"].as<JsonArray>()) {
      if (g_state.entryCount >= MAX_ENTRIES) break;
      g_state.entries[g_state.entryCount++] = (const char *)(v | "");
    }

  bool wasActive = g_state.prompt.active;
  if (d["prompt"].is<JsonObject>()) {
    JsonObject p = d["prompt"];
    g_state.prompt.active = true;
    g_state.prompt.id   = (const char *)(p["id"]   | "");
    g_state.prompt.tool = (const char *)(p["tool"] | "");
    g_state.prompt.hint = (const char *)(p["hint"] | "");
    if (!wasActive) g_state.responseSent = false;
  } else {
    g_state.prompt.active = false;
  }

  st::onBridgeTokens(g_state.tokens);   // feeds leveling
  usage::onTokens(g_state.tokens);      // feeds the rolling 5h meter
  g_state.lastSnapshotMs = millis();
  g_state.snapCount++;
  Serial.printf("[proto] snapshot total=%d running=%d waiting=%d prompt=%d\n",
                g_state.total, g_state.running, g_state.waiting, g_state.prompt.active);
  ui::onSnapshot();
}

void handleLine(const String &line) {
  if (line.length() == 0) return;
  JsonDocument d;
  if (deserializeJson(d, line)) { Serial.println("[proto] bad JSON"); return; }

  if (!d["cmd"].is<const char *>() && !d["evt"].is<const char *>()) {
    if (d["total"].is<int>())      { handleSnapshot(d); return; }
    if (d["time"].is<JsonArray>()) { rtc::setFromEpoch(d["time"][0] | 0L, d["time"][1] | 0L); return; }
    return;
  }

  const char *evt = d["evt"] | "";
  if (strcmp(evt, "turn") == 0) { if (line.length() <= TURN_EVT_MAX) ui::onTurn(d); return; }

  const char *cmd = d["cmd"] | "";
  if (strcmp(cmd, "status") == 0)      sendStatus();
  else if (strcmp(cmd, "name") == 0)  { st::setPetName((const char *)(d["name"] | "")); sendAck("name", true); }
  else if (strcmp(cmd, "owner") == 0) { st::setOwner((const char *)(d["name"] | "")); sendAck("owner", true); }
  else if (strcmp(cmd, "unpair") == 0){ ble::eraseBonds(); sendAck("unpair", true); }
  else Serial.printf("[proto] unhandled cmd: %s\n", cmd);
}

} // namespace protocol
