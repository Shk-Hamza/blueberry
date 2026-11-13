#include "ui.h"
#include <Preferences.h>
#include <time.h>
#include <string.h>

TFT_eSPI tft = TFT_eSPI();
SPIClass tsSPI(VSPI);
XPT2046_Touchscreen ts(T_CS, T_IRQ);

uint16_t cBG, cPanel, cCyan, cGreen, cMag, cDim, cWhite, cAmber, cRed;
bool g_wifiConnected = false;
bool g_sdPresent = false;
bool g_timeSynced = false;

static const char *toastMsg = "";
static uint16_t toastCol = 0;
static uint32_t toastUntil = 0;

static Preferences prefs;

void uiInit() {
  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(CYD_INVERT);
  tft.fillScreen(TFT_BLACK);

  tsSPI.begin(T_CLK, T_MISO, T_MOSI, T_CS);
  ts.begin(tsSPI);
  ts.setRotation(1);

  cBG    = tft.color565(10, 16, 32);
  cPanel = tft.color565(18, 28, 52);
  cCyan  = tft.color565(0, 229, 255);
  cGreen = tft.color565(57, 255, 20);
  cMag   = tft.color565(255, 0, 170);
  cDim   = tft.color565(70, 90, 130);
  cWhite = tft.color565(235, 245, 255);
  cAmber = tft.color565(255, 176, 0);
  cRed   = tft.color565(255, 60, 60);

#ifdef PIN_SPEAKER
  pinMode(PIN_SPEAKER, OUTPUT);
#endif
}

void uiPollTouch(TouchState &t) {
  static uint32_t downAt = 0;
  bool now = ts.tirqTouched() && ts.touched();
  t.justPressed = false;
  t.justReleased = false;

  if (now) {
    TS_Point p = ts.getPoint();
    // flip (3700,200)/(200,3700) below if an axis feels reversed on your panel
    t.x = constrain(map(p.x, 200, 3700, 0, SCR_W), 0, SCR_W - 1);
    t.y = constrain(map(p.y, 3700, 200, 0, SCR_H), 0, SCR_H - 1);
  }

  if (now && !t.down) {
    t.down = true; t.justPressed = true; t.longPress = false;
    t.startX = t.x; t.startY = t.y;
    downAt = millis();
  } else if (!now && t.down) {
    t.down = false; t.justReleased = true;
  } else if (now && t.down && !t.longPress && millis() - downAt > 700) {
    t.longPress = true;
  }
}

bool uiTapIn(const TouchState &t, int x, int y, int w, int h) {
  if (!t.justReleased) return false;
  return (t.startX >= x && t.startX < x + w && t.startY >= y && t.startY < y + h);
}

static void drawWifiIcon(int x, int y, bool on) {
  uint16_t c = on ? cCyan : cDim;
  for (int i = 0; i < 3; i++) {
    int bh = 4 + i * 4;
    tft.fillRect(x + i * 6, y + 16 - bh, 4, bh, (on || i == 0) ? c : cDim);
  }
}

void uiHeader(const char *title, uint16_t accent) {
  tft.fillRect(0, 0, SCR_W, 26, cPanel);
  tft.drawFastHLine(0, 26, SCR_W, accent);
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(accent, cPanel);
  tft.drawString(title, 36, 13);
  tft.fillTriangle(10, 13, 22, 6, 22, 20, accent);

  // right-aligned status cluster: clock | SD | WiFi
  int rx = SCR_W - 8;
  drawWifiIcon(rx - 20, 4, g_wifiConnected);
  rx -= 26;
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(g_sdPresent ? cGreen : cDim, cPanel);
  tft.drawString("SD", rx, 13);
  rx -= 28;

  if (g_timeSynced) {
    struct tm ti;
    if (getLocalTime(&ti, 0)) {
      char buf[6];
      snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
      tft.setTextColor(cWhite, cPanel);
      tft.drawString(buf, rx, 13);
    }
  }
  tft.setTextDatum(TL_DATUM);
}

bool uiBackButton(const TouchState &t) {
  return uiTapIn(t, 0, 0, 30, 26);
}

void uiToast(const char *text, uint16_t col) {
  toastMsg = text; toastCol = col; toastUntil = millis() + 1100;
}

void uiDrawToastIfActive() {
  if (millis() > toastUntil) return;
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  int w = tft.textWidth(toastMsg) + 24;
  int x = (SCR_W - w) / 2;
  tft.fillRoundRect(x, 206, w, 24, 6, TFT_BLACK);
  tft.drawRoundRect(x, 206, w, 24, 6, toastCol);
  tft.setTextColor(toastCol, TFT_BLACK);
  tft.drawString(toastMsg, SCR_W / 2, 218);
  tft.setTextDatum(TL_DATUM);
}

int uiMenuGrid(const MenuTile *tiles, int count, int cols, const char *title) {
  int rows = (count + cols - 1) / cols;
  int gap = 8;
  int gridW = SCR_W - gap * (cols + 1);
  int gridH = (SCR_H - 34) - gap * (rows + 1);
  int tw = gridW / cols, th = gridH / rows;

  tft.fillScreen(cBG);
  uiHeader(title, cCyan);

  for (int i = 0; i < count; i++) {
    int r = i / cols, c = i % cols;
    int x = gap + c * (tw + gap);
    int y = 32 + gap + r * (th + gap);
    tft.fillRoundRect(x, y, tw, th, 8, cPanel);
    tft.drawRoundRect(x, y, tw, th, 8, tiles[i].color);
    tft.fillRoundRect(x, y, tw, 4, 8, tiles[i].color);
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(cWhite, cPanel);
    tft.drawString(tiles[i].label, x + tw / 2, y + th / 2);
  }
  tft.setTextDatum(TL_DATUM);

  TouchState t;
  while (true) {
    uiPollTouch(t);
    if (uiBackButton(t)) return -1;
    if (t.justReleased) {
      for (int i = 0; i < count; i++) {
        int r = i / cols, c = i % cols;
        int x = gap + c * (tw + gap);
        int y = 32 + gap + r * (th + gap);
        if (uiTapIn(t, x, y, tw, th)) return i;
      }
    }
    delay(10);
  }
}

// ---------- on-screen QWERTY keyboard ----------
static const char *kbRows[3] = { "1234567890", "qwertyuiop", "asdfghjkl" };
static const char *kbRow4 = "zxcvbnm";

bool uiKeyboard(const char *title, char *buf, size_t maxlen, bool password) {
  bool shift = false;
  bool symbols = false;
  size_t len = strlen(buf);

  const int kx = 4, ky = 118, kw = (SCR_W - 8) / 10, kh = 24, kgap = 2;

  auto drawField = [&]() {
    tft.fillRect(4, 32, SCR_W - 8, 36, cPanel);
    tft.drawRect(4, 32, SCR_W - 8, 36, cCyan);
    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(cWhite, cPanel);
    char shown[64];
    if (password) {
      size_t n = min(len, sizeof(shown) - 2);
      memset(shown, '*', n); shown[n] = 0;
    } else {
      strncpy(shown, buf, sizeof(shown) - 1); shown[sizeof(shown) - 1] = 0;
    }
    tft.drawString(shown, 10, 50);
    tft.setTextDatum(TL_DATUM);
  };

  auto drawKeys = [&]() {
    tft.fillRect(0, 74, SCR_W, SCR_H - 74, cBG);
    tft.setTextFont(1);
    tft.setTextDatum(MC_DATUM);
    char sym1[] = "!@#$%^&*()";
    char sym2[] = "-_=+[]{};:";
    char sym3[] = "<>?/,.\\|'\"";
    for (int row = 0; row < 3; row++) {
      const char *src = symbols ? (row == 0 ? sym1 : row == 1 ? sym2 : sym3) : kbRows[row];
      int n = strlen(src);
      int offset = (row == 2) ? kw / 2 : 0;
      for (int i = 0; i < n; i++) {
        int x = kx + offset + i * kw, y = ky + row * (kh + kgap);
        tft.fillRoundRect(x, y, kw - kgap, kh, 3, cPanel);
        char c = src[i];
        if (!symbols && shift) c = toupper(c);
        char s[2] = { c, 0 };
        tft.drawString(s, x + (kw - kgap) / 2, y + kh / 2);
      }
    }
    int row4y = ky + 3 * (kh + kgap);
    // shift
    tft.fillRoundRect(kx, row4y, kw * 2 - kgap, kh, 3, shift ? cCyan : cPanel);
    tft.drawString("SHF", kx + kw - kgap / 2, row4y + kh / 2);
    const char *src4 = symbols ? "~`^°±×÷" : kbRow4;
    int n4 = strlen(src4);
    for (int i = 0; i < n4; i++) {
      int x = kx + kw * 2 + i * kw, y = row4y;
      tft.fillRoundRect(x, y, kw - kgap, kh, 3, cPanel);
      char c = src4[i];
      if (!symbols && shift) c = toupper(c);
      char s[2] = { c, 0 };
      tft.drawString(s, x + (kw - kgap) / 2, y + kh / 2);
    }
    // backspace
    int bx = kx + kw * 9, by = row4y;
    tft.fillRoundRect(bx, by, kw - kgap, kh, 3, cRed);
    tft.drawString("<-", bx + (kw - kgap) / 2, by + kh / 2);

    int row5y = row4y + kh + kgap;
    // 123 toggle
    tft.fillRoundRect(kx, row5y, kw * 2 - kgap, kh, 3, symbols ? cCyan : cPanel);
    tft.drawString(symbols ? "ABC" : "123", kx + kw - kgap / 2, row5y + kh / 2);
    // space
    tft.fillRoundRect(kx + kw * 2, row5y, kw * 5 - kgap, kh, 3, cPanel);
    tft.drawString("SPACE", kx + kw * 2 + (kw * 5 - kgap) / 2, row5y + kh / 2);
    // cancel
    tft.fillRoundRect(kx + kw * 7, row5y, kw * 1 - kgap, kh, 3, cPanel);
    tft.drawString("X", kx + kw * 7 + (kw - kgap) / 2, row5y + kh / 2);
    // done
    tft.fillRoundRect(kx + kw * 8, row5y, kw * 2 - kgap, kh, 3, cGreen);
    tft.drawString("DONE", kx + kw * 8 + (kw * 2 - kgap) / 2, row5y + kh / 2);
    tft.setTextDatum(TL_DATUM);
  };

  tft.fillScreen(cBG);
  uiHeader(title, cCyan);
  drawField();
  drawKeys();

  TouchState t;
  while (true) {
    uiPollTouch(t);
    if (!t.justReleased) { delay(8); continue; }
    int tx = t.startX, ty = t.startY;
    if (ty < 30 && tx < 30) return false; // header back = cancel

    bool changed = false;
    for (int row = 0; row < 3 && !changed; row++) {
      const char *src;
      char sym1[] = "!@#$%^&*()", sym2[] = "-_=+[]{};:", sym3[] = "<>?/,.\\|'\"";
      src = symbols ? (row == 0 ? sym1 : row == 1 ? sym2 : sym3) : kbRows[row];
      int n = strlen(src);
      int offset = (row == 2) ? kw / 2 : 0;
      int y = ky + row * (kh + kgap);
      if (ty < y || ty >= y + kh) continue;
      for (int i = 0; i < n; i++) {
        int x = kx + offset + i * kw;
        if (tx >= x && tx < x + kw - kgap) {
          char c = src[i];
          if (!symbols && shift) c = toupper(c);
          if (len < maxlen - 1) { buf[len++] = c; buf[len] = 0; }
          changed = true; break;
        }
      }
    }
    int row4y = ky + 3 * (kh + kgap);
    if (!changed && ty >= row4y && ty < row4y + kh) {
      if (tx >= kx && tx < kx + kw * 2 - kgap) { shift = !shift; changed = true; drawKeys(); }
      else if (tx >= kx + kw * 9 && tx < kx + kw * 10 - kgap) {
        if (len > 0) { len--; buf[len] = 0; } changed = true;
      } else {
        const char *src4 = symbols ? "~`^°±×÷" : kbRow4;
        int n4 = strlen(src4);
        for (int i = 0; i < n4; i++) {
          int x = kx + kw * 2 + i * kw;
          if (tx >= x && tx < x + kw - kgap) {
            char c = src4[i];
            if (!symbols && shift) c = toupper(c);
            if (len < maxlen - 1) { buf[len++] = c; buf[len] = 0; }
            changed = true; break;
          }
        }
      }
    }
    int row5y = row4y + kh + kgap;
    if (!changed && ty >= row5y && ty < row5y + kh) {
      if (tx >= kx && tx < kx + kw * 2 - kgap) { symbols = !symbols; changed = true; drawKeys(); }
      else if (tx >= kx + kw * 2 && tx < kx + kw * 7 - kgap) {
        if (len < maxlen - 1) { buf[len++] = ' '; buf[len] = 0; } changed = true;
      } else if (tx >= kx + kw * 7 && tx < kx + kw * 8 - kgap) {
        return false; // cancel
      } else if (tx >= kx + kw * 8 && tx < kx + kw * 10 - kgap) {
        return true; // done
      }
    }
    if (changed) drawField();
  }
}

bool uiConfirm(const char *title, const char *msg) {
  tft.fillScreen(cBG);
  uiHeader(title, cAmber);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(cWhite, cBG);
  tft.drawString(msg, SCR_W / 2, 90);

  int bw = 120, bh = 36, gap = 20;
  int yx = SCR_W / 2 - bw - gap / 2, nx = SCR_W / 2 + gap / 2, by = 140;
  tft.fillRoundRect(yx, by, bw, bh, 8, cGreen);
  tft.drawString("YES", yx + bw / 2, by + bh / 2);
  tft.fillRoundRect(nx, by, bw, bh, 8, cRed);
  tft.drawString("NO", nx + bw / 2, by + bh / 2);
  tft.setTextDatum(TL_DATUM);

  TouchState t;
  while (true) {
    uiPollTouch(t);
    if (uiBackButton(t)) return false;
    if (uiTapIn(t, yx, by, bw, bh)) return true;
    if (uiTapIn(t, nx, by, bw, bh)) return false;
    delay(10);
  }
}

void uiBusy(const char *msg) {
  tft.fillRect(40, 100, SCR_W - 80, 50, cPanel);
  tft.drawRect(40, 100, SCR_W - 80, 50, cCyan);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(cCyan, cPanel);
  tft.drawString(msg, SCR_W / 2, 125);
  tft.setTextDatum(TL_DATUM);
}

uint32_t uiLoadHighScore(const char *key) {
  prefs.begin(NVS_NAMESPACE, true);
  uint32_t v = prefs.getUInt(key, 0);
  prefs.end();
  return v;
}

void uiSaveHighScore(const char *key, uint32_t score) {
  prefs.begin(NVS_NAMESPACE, false);
  if (score > prefs.getUInt(key, 0)) prefs.putUInt(key, score);
  prefs.end();
}

void uiBeep(uint16_t freq, uint16_t ms) {
#ifdef PIN_SPEAKER
  tone(PIN_SPEAKER, freq, ms);
#endif
}
