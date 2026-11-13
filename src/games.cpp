#include "games.h"

#define PY0 30
#define PH  (SCR_H - PY0)

static uint64_t rs2 = 88172645463325252ULL;
static inline uint32_t gr() { rs2 ^= rs2 << 13; rs2 ^= rs2 >> 7; rs2 ^= rs2 << 17; return (uint32_t)rs2; }

// returns true to retry, false to exit to the games menu
static bool gameOverScreen(const char *title, uint32_t score, const char *key) {
  uint32_t hi = uiLoadHighScore(key);
  bool newHi = score > hi;
  if (newHi) uiSaveHighScore(key, score);
  tft.fillScreen(cBG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(cRed, cBG);
  tft.drawString(title, SCR_W/2, 60);
  tft.setTextFont(2);
  tft.setTextColor(cWhite, cBG);
  char buf[32];
  snprintf(buf, sizeof(buf), "Score: %lu", (unsigned long)score);
  tft.drawString(buf, SCR_W/2, 96);
  snprintf(buf, sizeof(buf), "Best: %lu", (unsigned long)max(score, hi));
  tft.setTextColor(newHi ? cAmber : cDim, cBG);
  tft.drawString(newHi ? "NEW BEST!" : buf, SCR_W/2, 118);

  int bw = 110, bh = 34, gap = 16, by = 150;
  int rx = SCR_W/2 - bw - gap/2, nx = SCR_W/2 + gap/2;
  tft.fillRoundRect(rx, by, bw, bh, 8, cGreen);
  tft.setTextColor(TFT_BLACK, cGreen);
  tft.drawString("RETRY", rx + bw/2, by + bh/2);
  tft.fillRoundRect(nx, by, bw, bh, 8, cPanel);
  tft.drawRoundRect(nx, by, bw, bh, 8, cRed);
  tft.setTextColor(cRed, cPanel);
  tft.drawString("EXIT", nx + bw/2, by + bh/2);
  tft.setTextDatum(TL_DATUM);

  TouchState t;
  while (true) {
    uiPollTouch(t);
    if (uiBackButton(t)) return false;
    if (uiTapIn(t, rx, by, bw, bh)) return true;
    if (uiTapIn(t, nx, by, bw, bh)) return false;
    delay(10);
  }
}

// =================== SNAKE ===================
#define CELL 10
#define GX (SCR_W / CELL)
#define GY (PH / CELL)
static void snakePlay() {
  restart:
  int16_t sx[GX*GY], sy[GX*GY];
  int len = 4, dirX = 1, dirY = 0, pendDirX = 1, pendDirY = 0;
  sx[0]=8; sy[0]=GY/2; sx[1]=7; sy[1]=GY/2; sx[2]=6; sy[2]=GY/2; sx[3]=5; sy[3]=GY/2;
  int fx = GX*3/4, fy = GY/2;
  uint32_t score = 0;
  uint32_t lastStep = 0, stepMs = 150;
  TouchState t;

  tft.fillScreen(cBG);
  uiHeader("SNAKE", cGreen);

  while (true) {
    uiPollTouch(t);
    if (uiBackButton(t)) return;
    if (t.justPressed && t.y > PY0) {
      int cx = t.x - (sx[0]*CELL+CELL/2), cy = (t.y-PY0) - (sy[0]*CELL+CELL/2);
      if (abs(cx) > abs(cy)) { pendDirX = cx > 0 ? 1 : -1; pendDirY = 0; }
      else { pendDirY = cy > 0 ? 1 : -1; pendDirX = 0; }
    }
    if (millis() - lastStep >= stepMs) {
      lastStep = millis();
      if (!(pendDirX == -dirX && pendDirY == -dirY)) { dirX = pendDirX; dirY = pendDirY; }
      int nx = (sx[0] + dirX + GX) % GX, ny = (sy[0] + dirY + GY) % GY;
      bool dead = false;
      for (int i = 0; i < len; i++) if (sx[i]==nx && sy[i]==ny) dead = true;
      if (dead) { if (gameOverScreen("GAME OVER", score, "snake")) goto restart; else return; }
      for (int i = len; i > 0; i--) { sx[i]=sx[i-1]; sy[i]=sy[i-1]; }
      sx[0]=nx; sy[0]=ny;
      if (nx==fx && ny==fy) {
        len++; score += 10; uiBeep(900,30);
        if (stepMs > 60) stepMs -= 3;
        fx = gr()%GX; fy = gr()%GY;
      } else {
        tft.fillRect(sx[len]*CELL, PY0+sy[len]*CELL, CELL, CELL, cBG);
      }
      for (int i = 0; i < len; i++) tft.fillRect(sx[i]*CELL+1, PY0+sy[i]*CELL+1, CELL-2, CELL-2, i==0?cWhite:cGreen);
      tft.fillRect(fx*CELL+2, PY0+fy*CELL+2, CELL-4, CELL-4, cMag);
      tft.fillRect(60, 2, 120, 24, cBG);
      tft.setTextDatum(ML_DATUM); tft.setTextFont(2); tft.setTextColor(cWhite, cBG);
      char b[24]; snprintf(b,sizeof(b),"Score %lu",(unsigned long)score); tft.drawString(b, 60, 13);
      tft.setTextDatum(TL_DATUM);
    }
    delay(5);
  }
}

// =================== PONG ===================
static void pongPlay() {
  restart:
  float px = PY0+10, py = PH/2 - 20, ph = 40;
  float ax = SCR_W-14, ay = PH/2 - 20;
  float bx = SCR_W/2, by = PY0+PH/2, bvx = -2.6, bvy = 1.6;
  int ps=0, as=0; float aiSpeed = 1.6;
  TouchState t; uint32_t lastFrame=0;
  tft.fillScreen(cBG); uiHeader("PONG", cCyan);
  tft.drawFastVLine(SCR_W/2, PY0, PH, cDim);

  while (true) {
    uiPollTouch(t);
    if (uiBackButton(t)) return;
    if (t.down && t.y > PY0) py = constrain(t.y - ph/2, PY0, PY0+PH-ph);

    if (ay + ph/2 < by - 3) ay += aiSpeed; else if (ay + ph/2 > by + 3) ay -= aiSpeed;
    ay = constrain(ay, PY0, PY0+PH-ph);

    bx += bvx; by += bvy;
    if (by <= PY0 || by >= PY0+PH) bvy = -bvy;
    if (bx <= px+8 && bx >= px && by >= py && by <= py+ph) { bvx = fabs(bvx)*1.05; bx = px+8; uiBeep(600,20); }
    if (bx >= ax-8 && bx <= ax && by >= ay && by <= ay+ph) { bvx = -fabs(bvx)*1.05; bx = ax-8; uiBeep(600,20); }
    if (bx < PY0) { as++; aiSpeed += 0.15; bx = SCR_W/2; by = PY0+PH/2; bvx = 2.6; uiBeep(200,150); }
    if (bx > SCR_W) { ps++; bx = SCR_W/2; by = PY0+PH/2; bvx = -2.6; uiBeep(900,80); }
    if (ps >= 7) { if (gameOverScreen("YOU WIN", ps*10, "pong")) goto restart; else return; }
    if (as >= 7) { if (gameOverScreen("AI WINS", ps*10, "pong")) goto restart; else return; }

    tft.fillRect(0, PY0, SCR_W, PH, cBG);
    tft.drawFastVLine(SCR_W/2, PY0, PH, cDim);
    tft.fillRect(px, py, 6, ph, cCyan);
    tft.fillRect(ax, ay, 6, ph, cMag);
    tft.fillCircle(bx, by, 4, cWhite);
    tft.setTextDatum(MC_DATUM); tft.setTextFont(4); tft.setTextColor(cWhite, cBG);
    char b[8]; snprintf(b,sizeof(b),"%d",ps); tft.drawString(b, SCR_W/2-30, PY0+16);
    snprintf(b,sizeof(b),"%d",as); tft.drawString(b, SCR_W/2+30, PY0+16);
    tft.setTextDatum(TL_DATUM);

    uint32_t now=millis(); if (now-lastFrame<20) delay(20-(now-lastFrame)); lastFrame=millis();
  }
}

// =================== BREAKOUT ===================
static void breakoutPlay() {
  restart:
  const int rows=4, cols=10, bw=28, bh=10, gap=2, top=PY0+20;
  bool bricks[rows][cols]; for(int r=0;r<rows;r++)for(int c=0;c<cols;c++) bricks[r][c]=true;
  float px = SCR_W/2-20, pw=40, ph=8, py = PY0+PH-16;
  float bx = SCR_W/2, by = py-6, bvx=2.2, bvy=-2.6;
  int lives=3; uint32_t score=0;
  TouchState t; uint32_t lastFrame=0;

  auto drawBricks=[&](){
    uint16_t cols6[4]={cRed,cAmber,cGreen,cCyan};
    for(int r=0;r<rows;r++)for(int c=0;c<cols;c++) if(bricks[r][c])
      tft.fillRect(c*(bw+gap)+gap, top+r*(bh+gap), bw, bh, cols6[r%4]);
  };
  tft.fillScreen(cBG); uiHeader("BREAKOUT", cAmber); drawBricks();

  while(true) {
    uiPollTouch(t);
    if (uiBackButton(t)) return;
    if (t.down && t.y>PY0) px = constrain(t.x-pw/2, 0, SCR_W-pw);

    bx+=bvx; by+=bvy;
    if (bx<=2||bx>=SCR_W-2) bvx=-bvx;
    if (by<=PY0+2) bvy=-bvy;
    if (by>=py-4 && by<=py+ph && bx>=px && bx<=px+pw) { bvy=-fabs(bvy); bvx += (bx-(px+pw/2))*0.05; uiBeep(600,15); }
    bool hitBrick=false;
    for(int r=0;r<rows && !hitBrick;r++) for(int c=0;c<cols;c++) if(bricks[r][c]) {
      int bxp=c*(bw+gap)+gap, byp=top+r*(bh+gap);
      if (bx>=bxp && bx<=bxp+bw && by>=byp && by<=byp+bh) {
        bricks[r][c]=false; bvy=-bvy; score+=10; uiBeep(900,20);
        tft.fillRect(bxp, byp, bw, bh, cBG); hitBrick=true; break;
      }
    }
    if (by > PY0+PH) {
      lives--; uiBeep(150,200);
      if (lives<=0) { if (gameOverScreen("GAME OVER", score, "breakout")) goto restart; else return; }
      bx=SCR_W/2; by=py-6; bvx=2.2; bvy=-2.6;
    }
    bool cleared=true; for(int r=0;r<rows&&cleared;r++)for(int c=0;c<cols;c++) if(bricks[r][c]) cleared=false;
    if (cleared) { if (gameOverScreen("YOU WIN", score, "breakout")) goto restart; else return; }

    tft.fillRect(0, py-ph-2, SCR_W, PH-(py-ph-2-PY0), cBG);
    tft.fillRect(px, py, pw, ph, cCyan);
    tft.fillCircle(bx, by, 3, cWhite);
    tft.fillRect(SCR_W-70, 2, 68, 24, cBG);
    tft.setTextDatum(MR_DATUM); tft.setTextFont(2); tft.setTextColor(cWhite, cBG);
    char b[24]; snprintf(b,sizeof(b),"%lu  L%d",(unsigned long)score,lives); tft.drawString(b, SCR_W-6, 13);
    tft.setTextDatum(TL_DATUM);

    uint32_t now=millis(); if (now-lastFrame<18) delay(18-(now-lastFrame)); lastFrame=millis();
  }
}

// =================== FLAPPY ===================
static void flappyPlay() {
  restart:
  float by = PH/2+PY0, bvy=0;
  struct Pipe{float x; int gapY;}; const int NP=3; Pipe pipes[NP];
  for(int i=0;i<NP;i++){pipes[i].x=SCR_W+i*120; pipes[i].gapY=PY0+40+gr()%(PH-140);}
  uint32_t score=0; bool passed[NP]={false,false,false};
  TouchState t; uint32_t lastFrame=0;
  tft.fillScreen(cBG); uiHeader("FLAPPY", cMag);

  while(true) {
    uiPollTouch(t);
    if (uiBackButton(t)) return;
    if (t.justPressed) { bvy=-4.2; uiBeep(700,25); }

    bvy += 0.22; by += bvy;
    tft.fillRect(0, PY0, SCR_W, PH, cBG);
    bool dead = by<PY0 || by>PY0+PH-8;

    for (int i=0;i<NP;i++) {
      pipes[i].x -= 2.4;
      if (pipes[i].x < -24) { pipes[i].x += NP*120; pipes[i].gapY = PY0+40+gr()%(PH-140); passed[i]=false; }
      int gy=pipes[i].gapY, gh=60;
      tft.fillRect(pipes[i].x, PY0, 22, gy-PY0-gh/2, cGreen);
      tft.fillRect(pipes[i].x, gy+gh/2, 22, PY0+PH-(gy+gh/2), cGreen);
      if (40>=pipes[i].x && 40<=pipes[i].x+22 && (by<gy-gh/2 || by>gy+gh/2)) dead=true;
      if (!passed[i] && pipes[i].x+22 < 40) { passed[i]=true; score+=10; }
    }
    tft.fillCircle(40, by, 6, cAmber);
    tft.setTextDatum(TL_DATUM); tft.setTextFont(2); tft.setTextColor(cWhite,cBG);
    char b[16]; snprintf(b,sizeof(b),"%lu",(unsigned long)score); tft.drawString(b, 60, 4);

    if (dead) { if (gameOverScreen("GAME OVER", score, "flappy")) goto restart; else return; }
    uint32_t now=millis(); if (now-lastFrame<20) delay(20-(now-lastFrame)); lastFrame=millis();
  }
}

// =================== WHACK-A-BUG ===================
static void whackPlay() {
  restart:
  const int cols=4, rows=2, cw=SCR_W/cols, ch=PH/rows;
  int activeIdx=-1; uint32_t activeUntil=0, nextSpawn=0;
  uint32_t score=0, combo=0; uint32_t gameEnd = millis()+30000;
  TouchState t;
  tft.fillScreen(cBG); uiHeader("WHACK-A-BUG", cRed);

  auto drawCell=[&](int idx, bool on){
    int r=idx/cols, c=idx%cols;
    int x=c*cw, y=PY0+r*ch;
    tft.fillRect(x+2,y+2,cw-4,ch-4, on?cAmber:cPanel);
    tft.drawRect(x+2,y+2,cw-4,ch-4, cDim);
  };
  for(int i=0;i<cols*rows;i++) drawCell(i,false);

  while (true) {
    uiPollTouch(t);
    if (uiBackButton(t)) return;
    if (millis() > gameEnd) { if (gameOverScreen("TIME UP", score, "whack")) goto restart; else return; }

    if (activeIdx>=0 && millis()>activeUntil) { drawCell(activeIdx,false); activeIdx=-1; combo=0; }
    if (activeIdx<0 && millis()>nextSpawn) {
      activeIdx = gr() % (cols*rows);
      uint32_t remaining = gameEnd > millis() ? gameEnd - millis() : 0;
      uint32_t life = 900 - min((uint32_t)500, (uint32_t)((30000-remaining)/3));
      activeUntil = millis()+life;
      nextSpawn = millis()+150;
      drawCell(activeIdx,true);
    }
    if (t.justReleased && activeIdx>=0) {
      int r=activeIdx/cols, c=activeIdx%cols;
      int x=c*cw, y=PY0+r*ch;
      if (t.startX>=x && t.startX<x+cw && t.startY>=y && t.startY<y+ch) {
        combo++; score += 10*combo; uiBeep(1000,20);
        drawCell(activeIdx,false); activeIdx=-1;
      }
    }
    tft.fillRect(SCR_W-100,2,98,24,cBG);
    tft.setTextDatum(MR_DATUM); tft.setTextFont(2); tft.setTextColor(cWhite,cBG);
    char b[32]; snprintf(b,sizeof(b),"%lu  (%ds)",(unsigned long)score, (int)((gameEnd>millis()?gameEnd-millis():0)/1000));
    tft.drawString(b, SCR_W-6, 13);
    tft.setTextDatum(TL_DATUM);
    delay(10);
  }
}

// =================== menu ===================
void gamesRun() {
  MenuTile tiles[] = {
    {"SNAKE", cGreen}, {"PONG", cCyan}, {"BREAKOUT", cAmber},
    {"FLAPPY", cMag}, {"WHACK-A-BUG", cRed},
  };
  while (true) {
    int choice = uiMenuGrid(tiles, 5, 2, "MINI GAMES");
    if (choice < 0) return; // back chip tapped
    switch (choice) {
      case 0: snakePlay(); break;
      case 1: pongPlay(); break;
      case 2: breakoutPlay(); break;
      case 3: flappyPlay(); break;
      case 4: whackPlay(); break;
    }
  }
}
