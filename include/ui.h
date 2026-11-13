#pragma once
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "config.h"

extern TFT_eSPI tft;
extern XPT2046_Touchscreen ts;

// cyber color palette, shared everywhere
extern uint16_t cBG, cPanel, cCyan, cGreen, cMag, cDim, cWhite, cAmber, cRed;

// status-bar flags other modules update; ui.cpp reads them when drawing the header
extern bool g_wifiConnected;
extern bool g_sdPresent;
extern bool g_timeSynced;

struct TouchState {
  bool down = false;
  bool justPressed = false;
  bool justReleased = false;
  bool longPress = false;
  int x = -1, y = -1;
  int startX = -1, startY = -1;
};

void uiInit();
void uiPollTouch(TouchState &t);          // call once per loop, before using touch
bool uiTapIn(const TouchState &t, int x, int y, int w, int h); // justReleased inside rect

// header: back chip + title + live status icons (wifi / sd / clock) on the right
void uiHeader(const char *title, uint16_t accent);
bool uiBackButton(const TouchState &t);   // draws back chip top-left, returns true if tapped
void uiToast(const char *text, uint16_t col);
void uiDrawToastIfActive();

// simple icon-tile button grid. Blocks until a tile is tapped or the back chip is tapped.
// Returns the tile index, or -1 if back was tapped.
struct MenuTile { const char *label; uint16_t color; };
int uiMenuGrid(const MenuTile *tiles, int count, int cols, const char *title);

// full-screen QWERTY touch keyboard. Edits buf in place (up to maxlen-1 chars).
// Returns true if the user tapped "Done", false if they tapped "Cancel"/back.
bool uiKeyboard(const char *title, char *buf, size_t maxlen, bool password);

// blocking yes/no confirmation dialog
bool uiConfirm(const char *title, const char *msg);

// simple progress / status overlay for long operations (connecting, loading...)
void uiBusy(const char *msg);

// NVS-backed high score
uint32_t uiLoadHighScore(const char *key);
void uiSaveHighScore(const char *key, uint32_t score);

void uiBeep(uint16_t freq, uint16_t ms);
