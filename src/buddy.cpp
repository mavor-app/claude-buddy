#include "buddy.h"
#include "buddy_common.h"
#include "display.h"      // global `gfx` canvas
#include <Preferences.h>
#include <string.h>

// Mirrors PersonaState in ui.cpp
enum { B_SLEEP, B_IDLE, B_BUSY, B_ATTENTION, B_CELEBRATE, B_DIZZY, B_HEART };

// ──────────────── shared geometry ────────────────
// Same 1× design space as the reference, re-centered for the 410-wide panel
// and scaled up. Species art is space-padded for 1× alignment; the helpers
// trim+recenter per line at higher scale so padding never pushes ink off.
const int BUDDY_X_CENTER  = 205;   // panel center (410/2)
const int BUDDY_CANVAS_W  = 410;
const int BUDDY_Y_BASE    = 24;    // sprite block top (1× units)
const int BUDDY_Y_OVERLAY = 6;     // overlay-particle base (1× units)
const int BUDDY_CHAR_W    = 6;
const int BUDDY_CHAR_H    = 8;

// ──────────────── shared colors (RGB565) ────────────────
const uint16_t BUDDY_BG     = 0x0000;
const uint16_t BUDDY_HEART  = 0xF810;
const uint16_t BUDDY_DIM    = 0x8410;
const uint16_t BUDDY_YEL    = 0xFFE0;
const uint16_t BUDDY_WHITE  = 0xFFFF;
const uint16_t BUDDY_CYAN   = 0x07FF;
const uint16_t BUDDY_GREEN  = 0x07E0;
const uint16_t BUDDY_PURPLE = 0xA01F;
const uint16_t BUDDY_RED    = 0xF800;
const uint16_t BUDDY_BLUE   = 0x041F;

// Home renders large; "peek" (used when a status panel shares the screen)
// renders smaller. Both keep the buddy in the upper safe band.
static uint8_t _scale = 3;
static const int Y_TOP = 78;       // px from top to the buddy region (safe area)

void buddyPrintLine(const char *line, int yPx, uint16_t color, int xOff) {
  int len = strlen(line);
  if (_scale > 1) {
    while (len && line[len - 1] == ' ') len--;
    while (len && *line == ' ') { line++; len--; }
  }
  int w = len * BUDDY_CHAR_W * _scale;
  int x = BUDDY_X_CENTER - w / 2 + xOff * _scale;
  gfx->setTextColor(color, BUDDY_BG);
  gfx->setTextSize(_scale);
  gfx->setCursor(x, yPx);
  for (int i = 0; i < len; i++) gfx->print(line[i]);
}

void buddyPrintSprite(const char *const *lines, uint8_t nLines, int yOffset,
                      uint16_t color, int xOff) {
  gfx->setTextSize(_scale);
  int yBase = Y_TOP + BUDDY_Y_BASE * _scale;
  for (uint8_t i = 0; i < nLines; i++) {
    buddyPrintLine(lines[i], yBase + (yOffset + i * BUDDY_CHAR_H) * _scale, color, xOff);
  }
}

// Species pass 1× coords relative to center/overlay; scale around center here
// so all 18 species files stay scale-agnostic.
void buddySetCursor(int x, int y) {
  gfx->setCursor(BUDDY_X_CENTER + (x - BUDDY_X_CENTER) * _scale, Y_TOP + y * _scale);
}
void buddySetColor(uint16_t fg) { gfx->setTextColor(fg, BUDDY_BG); }
void buddyPrint(const char *s)  { gfx->setTextSize(_scale); gfx->print(s); }

// ──────────────── species registry ────────────────
extern const Species CAPYBARA_SPECIES, DUCK_SPECIES, GOOSE_SPECIES, BLOB_SPECIES,
    CAT_SPECIES, DRAGON_SPECIES, OCTOPUS_SPECIES, OWL_SPECIES, PENGUIN_SPECIES,
    TURTLE_SPECIES, SNAIL_SPECIES, GHOST_SPECIES, AXOLOTL_SPECIES, CACTUS_SPECIES,
    ROBOT_SPECIES, RABBIT_SPECIES, MUSHROOM_SPECIES, CHONK_SPECIES;

static const Species *SPECIES_TABLE[] = {
  &CAPYBARA_SPECIES, &DUCK_SPECIES, &GOOSE_SPECIES, &BLOB_SPECIES,
  &CAT_SPECIES, &DRAGON_SPECIES, &OCTOPUS_SPECIES, &OWL_SPECIES,
  &PENGUIN_SPECIES, &TURTLE_SPECIES, &SNAIL_SPECIES, &GHOST_SPECIES,
  &AXOLOTL_SPECIES, &CACTUS_SPECIES, &ROBOT_SPECIES, &RABBIT_SPECIES,
  &MUSHROOM_SPECIES, &CHONK_SPECIES,
};
static const uint8_t N_SPECIES = sizeof(SPECIES_TABLE) / sizeof(SPECIES_TABLE[0]);
static uint8_t currentSpeciesIdx = 0;

static uint32_t tickCount  = 0;
static uint32_t nextTickAt = 0;
static const uint32_t TICK_MS = 200;   // 5 fps animation

static uint8_t lastDrawnState   = 0xFF;
static uint8_t lastDrawnSpecies = 0xFF;

void buddyInit() {
  tickCount = 0; nextTickAt = 0;
  Preferences p; p.begin("buddy", true);
  uint8_t saved = p.getUChar("species", 0xFF);
  p.end();
  if (saved < N_SPECIES) currentSpeciesIdx = saved;
}

void buddySetSpeciesIdx(uint8_t idx) { if (idx < N_SPECIES) currentSpeciesIdx = idx; }
void buddySetSpecies(const char *name) {
  for (uint8_t i = 0; i < N_SPECIES; i++)
    if (strcmp(SPECIES_TABLE[i]->name, name) == 0) { currentSpeciesIdx = i; return; }
}
const char *buddySpeciesName() { return SPECIES_TABLE[currentSpeciesIdx]->name; }
uint8_t buddySpeciesCount()    { return N_SPECIES; }
uint8_t buddySpeciesIdx()      { return currentSpeciesIdx; }
void buddyNextSpecies() {
  currentSpeciesIdx = (currentSpeciesIdx + 1) % N_SPECIES;
  Preferences p; p.begin("buddy", false); p.putUChar("species", currentSpeciesIdx); p.end();
  buddyInvalidate();
}

void buddyInvalidate() { lastDrawnState = 0xFF; }

void buddySetPeek(bool peek) {
  uint8_t s = peek ? 2 : 3;
  if (s == _scale) return;
  _scale = s; buddyInvalidate();
}

// Redraw only when the tick or state changes (animation is 5 fps; the render
// loop is faster). Clears the buddy region then renders the current state.
void buddyTick(uint8_t personaState) {
  uint32_t now = millis();
  bool ticked = false;
  if ((int32_t)(now - nextTickAt) >= 0) { nextTickAt = now + TICK_MS; tickCount++; ticked = true; }

  if (personaState >= 7) personaState = B_IDLE;
  if (!ticked && personaState == lastDrawnState && currentSpeciesIdx == lastDrawnSpecies) return;
  lastDrawnState = personaState;
  lastDrawnSpecies = currentSpeciesIdx;

  int clearH = Y_TOP + (BUDDY_Y_BASE + 6 * BUDDY_CHAR_H + 12) * _scale;
  if (clearH > BUDDY_CANVAS_W) {}  // (no-op guard kept for clarity)
  gfx->fillRect(0, 0, BUDDY_CANVAS_W, clearH, BUDDY_BG);

  const Species *sp = SPECIES_TABLE[currentSpeciesIdx];
  if (sp->states[personaState]) sp->states[personaState](tickCount);
}
