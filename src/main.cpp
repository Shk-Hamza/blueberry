// CYD 2-USB (ESP32-2432S028 / ST7789) - phone-like shell
// Home screen + status bar -> Files (SD + /home) -> Browser (WiFi, cached) ->
// Wi-Fi settings -> interactive effects -> mini games.
//
// Put this file at src/main.cpp. Needs: include/config.h, ui.h/.cpp,
// storage.h/.cpp, wifi_mgr.h/.cpp, filemgr.h/.cpp, browser.h/.cpp,
// effects.h/.cpp, games.h/.cpp all in the same project.

#include <Arduino.h>
#include "ui.h"
#include "storage.h"
#include "wifi_mgr.h"
#include "filemgr.h"
#include "browser.h"
#include "effects.h"
#include "games.h"

static uint32_t rsIntro = 2463534242UL;
static inline uint32_t xrIntro() { rsIntro ^= rsIntro << 13; rsIntro ^= rsIntro >> 17; rsIntro ^= rsIntro << 5; return rsIntro; }

// ---------------- intro card ----------------
static void typeText(int x, int y, const char *s, uint16_t col, int font, int ms) {
  tft.setTextFont(font);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(col, cBG);
  tft.setCursor(x, y);
  for (const char *p = s; *p; p++) { tft.print(*p); delay(ms); }
}

static void drawName(bool glitch) {
  const char *name = "SHK-HAMZA";
  tft.setTextFont(4);
  tft.setTextSize(2);
  if (tft.textWidth(name) > 290) tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  int frames = glitch ? 14 : 1;
  for (int f = 0; f < frames; f++) {
    tft.fillRect(12, 50, 296, 62, cBG);
    int amp = glitch ? (frames - 1 - f) / 2 : 0;
    int jy = glitch ? (int)(xrIntro() % 5) - 2 : 0;
    if (amp > 0) {
      tft.setTextColor(cMag); tft.drawString(name, 160 - amp, 80 + jy);
      tft.setTextColor(cCyan); tft.drawString(name, 160 + amp, 80 - jy);
    }
    if (!(glitch && amp > 0 && (xrIntro() & 3) == 0)) {
      tft.setTextColor(cWhite); tft.drawString(name, 160, 80);
    }
    if (glitch) delay(40);
  }
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
}

static void introCard() {
  tft.fillScreen(cBG);
  for (int sx = 0; sx <= SCR_W; sx += 10) {
    tft.drawFastVLine(sx, 0, SCR_H, cCyan);
    delay(4);
    tft.drawFastVLine(sx, 0, SCR_H, cBG);
  }
  for (int s = 0; s <= 300; s += 15) {
    tft.drawFastHLine(10, 10, s, cCyan);
    tft.drawFastHLine(310 - s, 229, s, cCyan);
    if (s <= 220) { tft.drawFastVLine(10, 229 - s, s, cCyan); tft.drawFastVLine(309, 10, s, cCyan); }
    delay(6);
  }
  tft.fillRect(10, 10, 14, 3, cMag);  tft.fillRect(10, 10, 3, 14, cMag);
  tft.fillRect(296, 227, 14, 3, cMag); tft.fillRect(307, 216, 3, 14, cMag);

  typeText(24, 22, "> whoami", cGreen, 2, 45);
  delay(150);
  drawName(true);
  delay(150);
  typeText(24, 124, "> role", cGreen, 2, 45);
  typeText(24, 146, "GRAPHIC DESIGNER", cCyan, 2, 25);
  typeText(24, 166, "CYBERSECURITY RESEARCHER", cGreen, 2, 25);

  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(cMag, cBG);
  tft.drawString("design  meets  cybersecurity", 160, 196);
  tft.setTextFont(1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(cDim, cBG);
  tft.drawString("shkhamza.me", 18, 214);
  tft.setTextDatum(TR_DATUM);
  tft.drawString("github.com/Shk-Hamza", 302, 214);

  uint32_t t0 = millis(); bool on = true; uint32_t tb = 0;
  tft.setTextDatum(MC_DATUM);
  TouchState t;
  while (millis() - t0 < 4500) {
    if (millis() - tb > 450) { tb = millis(); on = !on; tft.setTextColor(on?cWhite:cBG, cBG); tft.drawString("[ tap to start ]", 160, 226); }
    uiPollTouch(t);
    if (t.justPressed) break;
    delay(10);
  }
  tft.setTextDatum(TL_DATUM);
  for (int pass = 0; pass < 8; pass++) {
    for (int y = pass; y < SCR_H; y += 8) tft.drawFastHLine(0, y, SCR_W, TFT_BLACK);
    delay(25);
  }
  tft.fillScreen(TFT_BLACK);
}

// ---------------- home screen ----------------
void setup() {
  Serial.begin(115200);
  uiInit();
  storageInit();
  wifiMgrInit();
  introCard();
}

void loop() {
  MenuTile tiles[] = {
    {"FILES", cGreen},
    {"BROWSER", cCyan},
    {"WI-FI", cAmber},
    {"EFFECTS", cMag},
    {"GAMES", cRed},
  };
  wifiMgrPoll();
  int choice = uiMenuGrid(tiles, 5, 2, "SHK-HAMZA  //  HOME");
  wifiMgrPoll();

  switch (choice) {
    case 0: filesRun(); break;
    case 1: browserRun(); break;
    case 2: wifiRun(); break;
    case 3: effectsRun(); break;
    case 4: gamesRun(); break;
    default: delay(50); break; // -1: back tapped on home itself, just redraw
  }
}
