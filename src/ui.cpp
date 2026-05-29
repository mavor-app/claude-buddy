#include "ui.h"
#include "app_state.h"
#include "display.h"
#include "buddy.h"
#include "protocol.h"
#include "stats_api.h"
#include "power.h"
#include "rtc.h"
#include "usage.h"
#include <Arduino_GFX_Library.h>

// PersonaState — matches the species state-function order.
enum PersonaState { P_SLEEP, P_IDLE, P_BUSY, P_ATTENTION, P_CELEBRATE, P_DIZZY, P_HEART };

static const uint16_t C_BG    = 0x0000;
static const uint16_t C_TEXT  = 0xEF7D;   // near-white
static const uint16_t C_DIM   = 0x8410;
static const uint16_t C_GREEN = 0x2DE5;
static const uint16_t C_RED   = 0xE9C4;
static const uint16_t C_AMBER = 0xFD20;
static const uint16_t C_CYAN  = 0x07FF;

static const int W = LCD_WIDTH;
static const int H = LCD_HEIGHT;

// one-shot persona overrides (celebrate/heart/dizzy)
static PersonaState oneShot = P_IDLE;
static uint32_t oneShotUntil = 0;
static int lastRunning = 0;
static uint32_t completedUntil = 0;   // brief celebrate when running drops to 0

// approval response display
static uint32_t sentClearAt = 0;

// ── helpers ───────────────────────────────────────────────────────────────
static void centerText(const char *s, int y, uint16_t color, uint8_t size) {
  int w = (int)strlen(s) * 6 * size;
  gfx->setTextSize(size);
  gfx->setTextColor(color, C_BG);
  gfx->setCursor(W / 2 - w / 2, y);
  gfx->print(s);
}

static void triggerOneShot(PersonaState s, uint32_t durMs) {
  oneShot = s;
  oneShotUntil = millis() + durMs;
}

// Approximate human-readable token count: 980 -> "980", 10500 -> "10K",
// 148075 -> "148K", 3120000 -> "3.1M". Not precise — just a vibe.
static void fmtK(uint32_t v, char *out, size_t n) {
  if (v >= 1000000UL)      snprintf(out, n, "%lu.%luM", (unsigned long)(v / 1000000UL),
                                    (unsigned long)((v % 1000000UL) / 100000UL));
  else if (v >= 1000UL)    snprintf(out, n, "%luK", (unsigned long)(v / 1000UL));
  else                     snprintf(out, n, "%lu", (unsigned long)v);
}

static const uint32_t NAP_AFTER_MS = 120000;  // long quiet -> the buddy naps

// Persona, following the reference's spirit: the buddy is awake (idle) by
// default — a momentary BLE drop (the desktop reconnects in the background)
// must NOT put it to sleep. Only a long stretch with no snapshot is a nap.
static PersonaState derive() {
  if ((int32_t)(oneShotUntil - millis()) > 0)   return oneShot;
  bool everSeen = g_state.lastSnapshotMs != 0;
  uint32_t sinceSnap = millis() - g_state.lastSnapshotMs;
  if (everSeen && sinceSnap > NAP_AFTER_MS)      return P_SLEEP;  // napping
  if (g_state.waiting > 0)                       return P_ATTENTION;
  if ((int32_t)(completedUntil - millis()) > 0)  return P_CELEBRATE;
  if (g_state.running > 0)                       return P_BUSY;
  return P_IDLE;   // awake, hanging out (also the fresh-boot state)
}

// ── approval button geometry (corner-safe, centered) ────────────────────────
static const int BTN_W = W - 120;          // 290 px
static const int BTN_X = (W - BTN_W) / 2;
static const int BTN_H = 70;
static const int APPROVE_Y = 300;
static const int DENY_Y    = 388;

static bool inRect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

// ── screens ─────────────────────────────────────────────────────────────────
static void drawHome() {
  PersonaState st = derive();
  buddyTick((uint8_t)st);   // draws the buddy into the top region of gfx

  // status block below the buddy region
  int y = 348;
  gfx->fillRect(0, y - 6, W, H - (y - 6), C_BG);

  const char *stateName[] = {"sleep","idle","busy","attention","celebrate","dizzy","heart"};
  uint16_t stateCol = st == P_ATTENTION ? C_RED : st == P_BUSY ? C_AMBER
                    : st == P_SLEEP ? C_DIM : C_GREEN;
  centerText(stateName[st], y, stateCol, 2); y += 30;

  char buf[64];
  snprintf(buf, sizeof(buf), "%d total  %d run  %d wait",
           g_state.total, g_state.running, g_state.waiting);
  centerText(buf, y, C_TEXT, 2); y += 28;

  // Approximate token usage: rolling 5h output + today's output.
  char k5[12], kt[12];
  fmtK(usage::last5h(), k5, sizeof(k5));
  fmtK(g_state.tokensToday, kt, sizeof(kt));
  snprintf(buf, sizeof(buf), "5h %s   today %s", k5, kt);
  centerText(buf, y, C_CYAN, 2); y += 34;

  // bottom line: time + power + owner
  char low[64]; low[0] = 0;
  int hh, mm;
  if (rtc::nowHM(hh, mm)) { char t[8]; snprintf(t, sizeof(t), "%02d:%02d  ", hh, mm); strcat(low, t); }
  BatteryInfo b = power::read();
  if (b.present)      { char t[16]; snprintf(t, sizeof(t), "%d%%%s", b.pct, b.mA < 0 ? "+" : ""); strcat(low, t); }
  else if (b.usb)     strcat(low, "USB");
  centerText(low, y, C_DIM, 2);

  if (st::owner()[0]) {
    char g[40]; snprintf(g, sizeof(g), "hi %s", st::owner());
    centerText(g, 40, C_DIM, 2);
  }

  // DEBUG: connection + RX activity, so we can see if the desktop is actually
  // delivering data. rx = bytes received, s = snapshots parsed, age = seconds
  // since last snapshot. If rx stays 0 -> nothing is reaching us over GATT.
  char dbg[64];
  uint32_t age = g_state.snapCount ? (millis() - g_state.lastSnapshotMs) / 1000 : 0;
  snprintf(dbg, sizeof(dbg), "%s%s rx%lu s%lu %lus",
           g_state.connected ? "C" : "-", g_state.encrypted ? "E" : "-",
           (unsigned long)g_state.rxBytes, (unsigned long)g_state.snapCount,
           (unsigned long)age);
  centerText(dbg, H - 22, C_DIM, 1);
}

static void wrapHint(const char *s, char out[3][40]) {
  out[0][0] = out[1][0] = out[2][0] = 0;
  int n = strlen(s), row = 0, col = 0;
  for (int i = 0; i < n && row < 3; i++) {
    if (col >= 26) { out[row][col] = 0; row++; col = 0; if (row >= 3) break; }
    out[row][col++] = s[i];
  }
  if (row < 3) out[row][col] = 0;
}

static void drawApproval() {
  gfx->fillScreen(C_BG);
  centerText("Permission needed", 70, C_AMBER, 2);

  uint32_t waited = (millis() - g_state.promptArrivedMs) / 1000;
  char w[24]; snprintf(w, sizeof(w), "approve? %lus", (unsigned long)waited);
  centerText(w, 104, waited >= 10 ? C_RED : C_DIM, 2);

  centerText(g_state.prompt.tool.c_str(), 150, C_TEXT, 4);

  char lines[3][40];
  wrapHint(g_state.prompt.hint.c_str(), lines);
  int hy = 210;
  for (int i = 0; i < 3 && lines[i][0]; i++) { centerText(lines[i], hy, C_DIM, 2); hy += 24; }

  int others = g_state.waiting > 1 ? g_state.waiting - 1 : 0;
  if (others > 0) { char m[24]; snprintf(m, sizeof(m), "+%d more waiting", others); centerText(m, 270, C_AMBER, 2); }

  if (g_state.responseSent) {
    centerText("sent...", APPROVE_Y + 30, C_DIM, 3);
    return;
  }
  // buttons
  gfx->fillRoundRect(BTN_X, APPROVE_Y, BTN_W, BTN_H, 12, C_GREEN);
  centerText("APPROVE", APPROVE_Y + 26, 0x0000, 3);
  gfx->fillRoundRect(BTN_X, DENY_Y, BTN_W, BTN_H, 12, C_RED);
  centerText("DENY", DENY_Y + 26, 0x0000, 3);
}

static void drawPasskey() {
  gfx->fillScreen(C_BG);
  centerText("BLUETOOTH PAIRING", 150, C_TEXT, 2);
  centerText("enter on desktop:", 200, C_DIM, 2);
  char b[8]; snprintf(b, sizeof(b), "%06lu", (unsigned long)g_state.passkey);
  centerText(b, 260, C_GREEN, 5);
}

// ── decision ────────────────────────────────────────────────────────────────
static void decide(const char *decision, bool approved) {
  if (!g_state.prompt.active || g_state.responseSent) return;
  uint32_t secs = (millis() - g_state.promptArrivedMs) / 1000;
  Serial.printf("[ui] decision=%s id=%s tool=%s in %lus\n",
                decision, g_state.prompt.id.c_str(), g_state.prompt.tool.c_str(),
                (unsigned long)secs);
  protocol::sendPermission(g_state.prompt.id, decision);
  if (approved) { st::onApproval(secs); if (secs < 5) triggerOneShot(P_HEART, 2500); }
  else          { st::onDenial(); }
  g_state.responseSent = true;
  sentClearAt = millis() + 1200;   // show "sent..." briefly, then back home
}

// ── public ───────────────────────────────────────────────────────────────────
namespace ui {

void begin() {
  buddyInit();
  gfx->fillScreen(C_BG);
}

void onSnapshot() {
  // recently-completed → brief celebrate when running drops to 0
  if (lastRunning > 0 && g_state.running == 0) completedUntil = millis() + 2500;
  lastRunning = g_state.running;

  if (st::pollLevelUp()) triggerOneShot(P_CELEBRATE, 3000);

  if (g_state.prompt.active && !g_state.responseSent) {
    g_state.promptArrivedMs = millis();   // (re)start the approval timer on first sight
  }
  if (!g_state.prompt.active) g_state.responseSent = false;
}

void onTurn(JsonDocument &d) {
  if (!d["content"].is<JsonArray>()) return;
  for (JsonVariant c : d["content"].as<JsonArray>())
    if (strcmp(c["type"] | "", "text") == 0) { Serial.printf("[turn] %s\n", (const char *)(c["text"] | "")); break; }
}

void onApprove() { decide("once", true); }
void onDeny()    { decide("deny", false); }

void onTouch(int x, int y) {
  if (!g_state.prompt.active || g_state.responseSent) return;
  if (inRect(x, y, BTN_X, APPROVE_Y, BTN_W, BTN_H)) onApprove();
  else if (inRect(x, y, BTN_X, DENY_Y, BTN_W, BTN_H)) onDeny();
}

void onNextButton() { buddyNextSpecies(); }

void render() {
  // clear a finished "sent..." prompt
  if (g_state.responseSent && (int32_t)(sentClearAt - millis()) <= 0) {
    g_state.prompt.active = false;
    g_state.responseSent = false;
  }

  // Which screen do we want this frame?
  enum Scr { S_HOME, S_APPROVAL, S_PASSKEY };
  Scr cur = g_state.showPasskey ? S_PASSKEY
          : (g_state.prompt.active ? S_APPROVAL : S_HOME);

  // On ANY screen change, wipe the whole canvas once — otherwise leftovers from
  // the previous screen (e.g. the green APPROVE button) bleed through the gaps
  // that the per-frame partial redraws don't cover.
  static int lastScr = -1;
  if ((int)cur != lastScr) {
    gfx->fillScreen(C_BG);
    buddyInvalidate();
    lastScr = (int)cur;
  }

  if (cur == S_PASSKEY)       drawPasskey();
  else if (cur == S_APPROVAL) drawApproval();
  else                        drawHome();
}

} // namespace ui
