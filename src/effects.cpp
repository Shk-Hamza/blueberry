#include "effects.h"
#include <math.h>

#define FW 160
#define FH 120

static uint8_t fb[FW * FH];
static uint8_t cells[FW * FH];
static uint8_t cellsN[FW * FH];
static uint16_t pal[256];
static uint16_t lineBuf[SCR_W * 2];
static uint8_t sinT[256];
static uint8_t palShift = 0;

static uint32_t rs = 2463534242UL;
static inline uint32_t xr() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

static void initSin() {
  static bool done = false;
  if (done) return;
  for (int i = 0; i < 256; i++) sinT[i] = (uint8_t)(127.5f + 127.5f * sinf(i * 6.2831853f / 256.0f));
  done = true;
}

// ---------- palettes ----------
static void palFire() {
  for (int i = 0; i < 256; i++) {
    int r, g, b;
    if (i < 85)       { r = i * 3; g = 0; b = 0; }
    else if (i < 170) { r = 255; g = (i - 85) * 3; b = 0; }
    else              { r = 255; g = 255; b = (i - 170) * 3; }
    pal[i] = tft.color565(r, g, b);
  }
}
static void palPlasma() { for (int i = 0; i < 256; i++) pal[i] = tft.color565(sinT[i], sinT[(i + 85) & 255], sinT[(i + 170) & 255]); }
static void palMatrix() {
  for (int i = 0; i < 256; i++) {
    int r = i > 235 ? (i - 235) * 12 : 0; if (r > 255) r = 255;
    pal[i] = tft.color565(r, i, r);
  }
}
static void palStars() { for (int i = 0; i < 256; i++) pal[i] = tft.color565(i * 7 / 8, i * 7 / 8, i); }
static void palLife() {
  for (int i = 0; i < 256; i++) {
    int r = i > 200 ? (i - 200) * 4 : 0; if (r > 255) r = 255;
    int b = 40 + i * 3 / 4; if (b > 255) b = 255;
    pal[i] = tft.color565(r, i * 9 / 10, b);
  }
}

// touch mapped into the FW x FH effect space
static bool fxTouch(const TouchState &t, int &fx, int &fy) {
  if (t.y < 30) return false; // reserve header for back button
  if (!t.down) return false;
  fx = t.x * FW / SCR_W;
  fy = (t.y - 30) * FH / (SCR_H - 30);
  return fx >= 0 && fx < FW && fy >= 0 && fy < FH;
}

// --- matrix (touch drops a bright column) ---
static int16_t mHead[FW]; static uint8_t mSpd[FW];
static void matrixInit() {
  palMatrix(); palShift = 0; memset(fb, 0, sizeof(fb));
  for (int x = 0; x < FW; x++) { mHead[x] = -(int)(xr() % FH); mSpd[x] = 1 + xr() % 3; }
}
static void matrixStep(const TouchState &t) {
  for (int i = 0; i < FW * FH; i++) { uint8_t v = fb[i]; fb[i] = v > 10 ? v - 10 : 0; }
  for (int x = 0; x < FW; x += 2) {
    int oh = mHead[x], nh = oh + mSpd[x];
    for (int r = oh; r < nh; r++) if (r >= 0 && r < FH) fb[r * FW + x] = (r == nh - 1) ? 255 : 170 + (xr() & 63);
    mHead[x] = nh;
    if (nh > FH + (int)(xr() % 40)) { mHead[x] = -(int)(xr() % 60); mSpd[x] = 1 + xr() % 3; }
  }
  int fx, fy;
  if (fxTouch(t, fx, fy)) for (int x = fx - 2; x <= fx + 2; x++) if (x >= 0 && x < FW) { mHead[x] = fy - 6; mSpd[x] = 4; }
}

// --- starfield (touch moves the vanishing point) ---
#define NSTAR 140
struct Star { int16_t x, y, z; int16_t px, py; };
static Star stars[NSTAR];
static int starCx = FW / 2, starCy = FH / 2;
static void starReset(Star &s, bool far) {
  s.x = (int)(xr() % 2048) - 1024; s.y = (int)(xr() % 2048) - 1024;
  s.z = far ? 16 + xr() % 496 : 512; s.px = s.py = -1;
}
static void starsInit() { palStars(); palShift = 0; memset(fb, 0, sizeof(fb)); for (auto &s : stars) starReset(s, true); }
static void starsStep(const TouchState &t) {
  int fx, fy;
  if (fxTouch(t, fx, fy)) { starCx = fx; starCy = fy; }
  else { starCx = FW / 2; starCy = FH / 2; }
  for (int i = 0; i < FW * FH; i++) { uint8_t v = fb[i]; fb[i] = v - (v >> 2) - (v ? 1 : 0); }
  for (auto &s : stars) {
    s.z -= 6;
    if (s.z < 16) { starReset(s, false); continue; }
    int sx = starCx + (s.x * 32) / s.z, sy = starCy + (s.y * 32) / s.z;
    if (sx < 0 || sx >= FW || sy < 0 || sy >= FH) { starReset(s, false); continue; }
    uint8_t br = 255 - (s.z * 200 / 512);
    if (s.px >= 0) for (int k = 1; k <= 3; k++) {
      int ix = s.px + (sx - s.px) * k / 3, iy = s.py + (sy - s.py) * k / 3;
      if (ix >= 0 && ix < FW && iy >= 0 && iy < FH) fb[iy * FW + ix] = br;
    }
    fb[sy * FW + sx] = br; s.px = sx; s.py = sy;
  }
}

// --- plasma (touch adds a ripple center) ---
static uint16_t plasmaT = 0; static int rippleX = -1, rippleY = -1;
static void plasmaInit() { palPlasma(); plasmaT = 0; palShift = 0; rippleX = rippleY = -1; }
static void plasmaStep(const TouchState &t) {
  plasmaT += 2; uint8_t tt = plasmaT;
  int fx, fy;
  if (fxTouch(t, fx, fy)) { rippleX = fx; rippleY = fy; }
  for (int y = 0; y < FH; y++) {
    uint8_t b = sinT[(y * 4 + tt * 3) & 255];
    for (int x = 0; x < FW; x++) {
      int dx = x - FW / 2, dy = y - FH / 2;
      uint8_t a = sinT[(x * 3 + tt * 2) & 255];
      uint8_t c = sinT[((x + y) * 2 + tt) & 255];
      uint8_t d = sinT[(((dx * dx + dy * dy) >> 5) + tt) & 255];
      uint8_t v = ((int)a + b + c + d) >> 2;
      if (rippleX >= 0) {
        int rdx = x - rippleX, rdy = y - rippleY;
        int dist = (int)sqrtf(rdx * rdx + rdy * rdy);
        v += sinT[(dist * 6 - tt * 4) & 255] >> 2;
      }
      fb[y * FW + x] = v;
    }
  }
  palShift = tt;
}

// --- fire (touch adds a flame burst) ---
static void fireInit() { palFire(); palShift = 0; memset(fb, 0, sizeof(fb)); }
static void fireStep(const TouchState &t) {
  for (int x = 0; x < FW; x++) fb[(FH - 1) * FW + x] = 200 + (xr() & 55);
  int fx, fy;
  if (fxTouch(t, fx, fy)) for (int dx = -4; dx <= 4; dx++) {
    int x = fx + dx; if (x >= 0 && x < FW) fb[(FH - 1) * FW + x] = 255;
    if (fy < FH) fb[fy * FW + (x < FW ? x : FW - 1)] = 255;
  }
  for (int y = 1; y < FH; y++) for (int x = 0; x < FW; x++) {
    uint8_t v = fb[y * FW + x];
    uint32_t r = xr() & 3;
    int dx = x - (int)r + 1; if (dx < 0) dx = 0; if (dx >= FW) dx = FW - 1;
    uint8_t decay = (r & 1) * 5;
    fb[(y - 1) * FW + dx] = v > decay ? v - decay : 0;
  }
}

// --- life (touch paints cells, long press reseeds) ---
static void lifeSeed() { for (int i = 0; i < FW * FH; i++) { cells[i] = (xr() % 100) < 26; fb[i] = cells[i] ? 255 : 0; } }
static void lifeInit() { palLife(); palShift = 0; lifeSeed(); }
static void lifeStep(const TouchState &t) {
  if (t.longPress) { lifeSeed(); uiToast("RESEEDED", cGreen); }
  int fx, fy;
  if (fxTouch(t, fx, fy)) for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
    int x = fx + dx, y = fy + dy;
    if (x >= 0 && x < FW && y >= 0 && y < FH) cells[y * FW + x] = 1;
  }
  uint16_t pop = 0;
  for (int y = 0; y < FH; y++) {
    int ym = y ? y - 1 : FH - 1, yp = (y < FH - 1) ? y + 1 : 0;
    for (int x = 0; x < FW; x++) {
      int xm = x ? x - 1 : FW - 1, xp = (x < FW - 1) ? x + 1 : 0;
      int n = cells[ym*FW+xm]+cells[ym*FW+x]+cells[ym*FW+xp]+cells[y*FW+xm]+cells[y*FW+xp]+cells[yp*FW+xm]+cells[yp*FW+x]+cells[yp*FW+xp];
      int i = y * FW + x; uint8_t alive = cells[i];
      uint8_t nx = (n == 3) || (alive && n == 2);
      cellsN[i] = nx;
      if (nx) { pop++; fb[i] = alive ? (fb[i] > 86 ? fb[i] - 6 : 80) : 255; } else fb[i] = fb[i] >> 1;
    }
  }
  memcpy(cells, cellsN, sizeof(cells));
  if (pop < 20) lifeSeed();
}

// --- bounce (tap spawns an extra logo) ---
TFT_eSprite logoSpr(&tft);
#define MAXLOGO 5
struct Logo { int x, y, dx, dy, col; };
static Logo logos[MAXLOGO]; static int logoCount = 1;
static int bw = 0; static uint16_t bpal[6];
static void bounceDraw(int col) {
  logoSpr.fillSprite(TFT_BLACK);
  logoSpr.setTextDatum(MC_DATUM);
  logoSpr.setTextColor(bpal[col]);
  logoSpr.drawString("SHK-HAMZA", bw / 2, 16, 4);
}
static void bounceInit() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(4);
  bw = tft.textWidth("SHK-HAMZA") + 8;
  logoSpr.setColorDepth(16);
  logoSpr.createSprite(bw, 32);
  bpal[0]=tft.color565(0,229,255); bpal[1]=tft.color565(57,255,20); bpal[2]=tft.color565(255,0,170);
  bpal[3]=tft.color565(255,200,0); bpal[4]=tft.color565(160,100,255); bpal[5]=tft.color565(255,255,255);
  logoCount = 1;
  logos[0] = {40, 60, 3, 2, 0};
}
static void bounceLeave() { logoSpr.deleteSprite(); }
static void bounceStep(const TouchState &t) {
  if (t.justPressed && t.y > 30 && logoCount < MAXLOGO) {
    logos[logoCount] = {t.x, t.y, (int)(xr()%5)-2+1, (int)(xr()%5)-2+1, (int)(xr()%6)};
    logoCount++;
  }
  for (int i = 0; i < logoCount; i++) {
    Logo &l = logos[i];
    l.x += l.dx; l.y += l.dy;
    bool hit = false;
    if (l.x <= 0) { l.x = 0; l.dx = -l.dx; hit = true; }
    if (l.x + bw >= SCR_W) { l.x = SCR_W - bw; l.dx = -l.dx; hit = true; }
    if (l.y <= 30) { l.y = 30; l.dy = -l.dy; hit = true; }
    if (l.y + 32 >= SCR_H) { l.y = SCR_H - 32; l.dy = -l.dy; hit = true; }
    if (hit) l.col = (l.col + 1) % 6;
    bounceDraw(l.col);
    logoSpr.pushSprite(l.x, l.y);
  }
}

// ---------- engine ----------
enum FX { FX_MATRIX, FX_STARS, FX_PLASMA, FX_FIRE, FX_LIFE, FX_BOUNCE, FX_COUNT };
static const char *fxNames[] = {"MATRIX", "STARFIELD", "PLASMA", "FIRE", "LIFE", "BOUNCE"};

static void pushFrame() {
  tft.startWrite();
  tft.setAddrWindow(0, 30, SCR_W, SCR_H - 30);
  for (int y = 0; y < FH; y++) {
    const uint8_t *s = &fb[y * FW];
    for (int x = 0; x < FW; x++) {
      uint16_t c = pal[(uint8_t)(s[x] + palShift)];
      lineBuf[2 * x] = c; lineBuf[2 * x + 1] = c;
    }
    memcpy(lineBuf + SCR_W, lineBuf, SCR_W * 2);
    tft.pushColors(lineBuf, SCR_W * 2, true);
  }
  tft.endWrite();
}

void effectsRun() {
  initSin();
  int cur = 0;
  bool needInit = true;
  TouchState t;
  uint32_t lastFrame = 0;

  while (true) {
    uiPollTouch(t);
    if (uiBackButton(t)) { if (cur == FX_BOUNCE) bounceLeave(); return; }

    if (uiTapIn(t, 0, SCR_H - 28, 44, 28)) { if (cur == FX_BOUNCE) bounceLeave(); cur = (cur + FX_COUNT - 1) % FX_COUNT; needInit = true; }
    if (uiTapIn(t, SCR_W - 44, SCR_H - 28, 44, 28)) { if (cur == FX_BOUNCE) bounceLeave(); cur = (cur + 1) % FX_COUNT; needInit = true; }

    if (needInit) {
      switch (cur) {
        case FX_MATRIX: matrixInit(); break;
        case FX_STARS:  starsInit();  break;
        case FX_PLASMA: plasmaInit(); break;
        case FX_FIRE:   fireInit();   break;
        case FX_LIFE:   lifeInit();   break;
        case FX_BOUNCE: bounceInit(); break;
      }
      tft.fillScreen(cBG);
      uiHeader(fxNames[cur], cCyan);
      uiToast(cur == FX_BOUNCE ? "TAP: ADD LOGO" : "TOUCH THE EFFECT", cDim);
      needInit = false;
    }

    switch (cur) {
      case FX_MATRIX: matrixStep(t); pushFrame(); break;
      case FX_STARS:  starsStep(t);  pushFrame(); break;
      case FX_PLASMA: plasmaStep(t); pushFrame(); break;
      case FX_FIRE:   fireStep(t);   pushFrame(); break;
      case FX_LIFE:   lifeStep(t);   pushFrame(); break;
      case FX_BOUNCE: bounceStep(t); break;
    }

    tft.fillTriangle(8, SCR_H-8, 20, SCR_H-20, 20, SCR_H-4, cDim);
    tft.fillTriangle(SCR_W-8, SCR_H-8, SCR_W-20, SCR_H-20, SCR_W-20, SCR_H-4, cDim);
    uiDrawToastIfActive();

    uint32_t now = millis();
    if (now - lastFrame < 30) delay(30 - (now - lastFrame));
    lastFrame = millis();
  }
}
