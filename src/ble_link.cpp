#include "ble_link.h"
#include "config.h"
#include "app_state.h"
#include <NimBLEDevice.h>
#include <queue>
#include "esp_mac.h"
#include "esp_random.h"

// NOTE: written against NimBLE-Arduino 1.4.x. For 2.x, the callback signatures
// take NimBLEConnInfo& instead of ble_gap_conn_desc* — see comments marked 2.x.

static NimBLEServer         *server  = nullptr;
static NimBLECharacteristic *txChar  = nullptr;
static NimBLECharacteristic *rxChar  = nullptr;

// inbound line reassembly (BLE task -> main loop)
static portMUX_TYPE qMux = portMUX_INITIALIZER_UNLOCKED;
static std::queue<std::string> lineQ;
static std::string rxAccum;   // bytes not yet terminated by '\n'

static void pushBytes(const uint8_t *data, size_t len) {
  g_state.rxBytes += len;   // debug: total bytes received over RX
  for (size_t i = 0; i < len; i++) {
    char c = (char)data[i];
    if (c == '\n') {
      portENTER_CRITICAL(&qMux);
      lineQ.push(rxAccum);
      portEXIT_CRITICAL(&qMux);
      rxAccum.clear();
    } else if (c != '\r') {
      if (rxAccum.size() < MAX_LINE_BYTES) rxAccum.push_back(c);
      else rxAccum.clear();   // overrun guard: drop the runaway line
    }
  }
}

// ---- characteristic write (desktop -> device) ----------------------------
class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c) override {       // 2.x: (c, connInfo)
    std::string v = c->getValue();
    Serial.printf("[ble] RX onWrite %u bytes\n", (unsigned)v.size());
    pushBytes((const uint8_t *)v.data(), v.size());
  }
};

// ---- server + security ----------------------------------------------------
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *s, ble_gap_conn_desc *desc) override {  // 2.x: (s, connInfo)
    g_state.connected = true;
    NimBLEDevice::setMTU(247);
    // NOTE: do NOT force connection params here — macOS is picky and rejecting
    // them dropped the link right after connect ("scan finds it, can't
    // connect"). Use whatever the central negotiates.
    Serial.printf("[ble] connect: encrypted=%d bonded=%d\n",
                  desc->sec_state.encrypted, desc->sec_state.bonded);
    // Show our (known, static-this-boot) passkey as soon as a non-bonded
    // central connects — NimBLE doesn't always call onPassKeyRequest once a
    // fixed passkey is set, so don't rely on that callback alone.
#if BLE_REQUIRE_ENCRYPTION
    if (!desc->sec_state.bonded) {
      g_state.showPasskey = true;
      Serial.printf("[ble] show passkey %06u (not yet bonded)\n", (unsigned)g_state.passkey);
    }
#endif
  }
  void onDisconnect(NimBLEServer *s) override {
    g_state.connected = false;
    g_state.encrypted = false;
    g_state.showPasskey = false;
    g_state.disconnects++;
    rxAccum.clear();
    Serial.println("[ble] disconnect; re-advertising");
    NimBLEDevice::startAdvertising();
  }

  // DisplayOnly: the device shows a passkey, the desktop user types it.
  uint32_t onPassKeyRequest() override {
    g_state.showPasskey = true;            // UI raises the passkey screen
    Serial.printf("[ble] onPassKeyRequest -> %06u\n", (unsigned)g_state.passkey);
    return g_state.passkey;                // 6-digit, generated at begin()
  }
  bool onConfirmPIN(uint32_t pin) override {
    Serial.printf("[ble] onConfirmPIN %06u (auto-accept)\n", (unsigned)pin);
    return true;
  }
  void onAuthenticationComplete(ble_gap_conn_desc *desc) override {    // 2.x: (connInfo)
    g_state.showPasskey = false;
    g_state.encrypted = desc->sec_state.encrypted;
    Serial.printf("[ble] auth complete, encrypted=%d bonded=%d\n",
                  desc->sec_state.encrypted, desc->sec_state.bonded);
  }
};

namespace ble {

void begin() {
  // 6-digit passkey for this boot (000000..999999).
  g_state.passkey = esp_random() % 1000000U;

  String adv = String(BLE_NAME_PREFIX) + "-";
  uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_BT);
  char suffix[5]; snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
  adv += suffix;                              // e.g. "Claude-9F3A"

  NimBLEDevice::init(adv.c_str());
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setMTU(247);

#if BLE_REQUIRE_ENCRYPTION
  // LE Secure Connections + bonding + MITM, DisplayOnly IO capability.
  NimBLEDevice::setSecurityAuth(true /*bond*/, true /*MITM*/, true /*SC*/);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  NimBLEDevice::setSecurityPasskey(g_state.passkey);
#endif

  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService *svc = server->createService(NUS_SERVICE_UUID);

  // RX: desktop writes here. NUS clients write WITHOUT response, so WRITE_NR
  // is mandatory — without it those writes are silently dropped (rx stays 0).
  // WRITE_ENC is added only when encryption is required.
  uint32_t rxProps = NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR;
  uint32_t txProps = NIMBLE_PROPERTY::NOTIFY;
#if BLE_REQUIRE_ENCRYPTION
  rxProps |= NIMBLE_PROPERTY::WRITE_ENC;
  txProps |= NIMBLE_PROPERTY::READ_ENC;   // forces an encrypted CCCD subscribe
#endif
  rxChar = svc->createCharacteristic(NUS_RX_UUID, rxProps);
  rxChar->setCallbacks(new RxCallbacks());
  txChar = svc->createCharacteristic(NUS_TX_UUID, txProps);

  svc->start();

  NimBLEAdvertising *advrt = NimBLEDevice::getAdvertising();
  advrt->addServiceUUID(NUS_SERVICE_UUID);
  advrt->setName(adv.c_str());
  advrt->setScanResponse(true);
  advrt->start();

  Serial.printf("[ble] advertising as %s, passkey=%06u\n", adv.c_str(), g_state.passkey);
}

void send(const String &line) {
  if (!txChar || !g_state.connected) return;
  // Split across MTU; the desktop reassembles. notify() takes the negotiated
  // MTU into account, but chunking keeps us safe on small-MTU centrals.
  const size_t mtu = 180;
  size_t off = 0, n = line.length();
  const char *p = line.c_str();
  while (off < n) {
    size_t len = min(mtu, n - off);
    txChar->setValue((const uint8_t *)(p + off), len);
    txChar->notify();
    off += len;
    if (off < n) delay(4);   // let the BLE stack flush before the next chunk
  }
}

bool nextLine(String &out) {
  bool got = false;
  portENTER_CRITICAL(&qMux);
  if (!lineQ.empty()) { out = lineQ.front().c_str(); lineQ.pop(); got = true; }
  portEXIT_CRITICAL(&qMux);
  return got;
}

void eraseBonds() { NimBLEDevice::deleteAllBonds(); }

// Safety net: after an out-of-range drop, NimBLE doesn't always come back
// advertising, so the desktop can't find us again. Whenever we're not
// connected and not advertising, (re)start advertising. Called from the loop.
void ensureAdvertising() {
  // Use the controller's real connection count, NOT g_state.connected — the
  // flag lags the callback, and acting in that window restarted advertising
  // mid-handshake, churning the link and wedging the desktop app.
  if (!server || server->getConnectedCount() > 0) return;
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  if (adv && !adv->isAdvertising()) {
    adv->start();
    Serial.println("[ble] re-advertising (was idle)");
  }
}

} // namespace ble
