#include "filemgr.h"
#include "storage.h"
#include "config.h"
#include <SD.h>
#include <LittleFS.h>

#define ROWS_VISIBLE 7
#define ROW_H 26
#define LIST_TOP 54

struct Entry { String name; bool isDir; uint32_t size; };

static bool useSD = true;           // which volume the explorer is browsing
static String curPath = "/";
static Entry entries[64];
static int entryCount = 0;
static int scrollTop = 0;

static FS &vol() { return useSD ? (FS &)SD : (FS &)LittleFS; }

static bool looksTexty(const String &name) {
  String n = name; n.toLowerCase();
  const char *exts[] = {".txt",".log",".md",".csv",".json",".ini",".cfg",".c",".cpp",".h",".html",".htm",".url"};
  for (auto e : exts) if (n.endsWith(e)) return true;
  return false;
}

static void loadDir() {
  entryCount = 0;
  scrollTop = 0;
  File dir = vol().open(curPath);
  if (!dir || !dir.isDirectory()) { curPath = "/"; dir = vol().open(curPath); }
  File f = dir.openNextFile();
  while (f && entryCount < 64) {
    String nm = f.name();
    int slash = nm.lastIndexOf('/');
    if (slash >= 0) nm = nm.substring(slash + 1);
    entries[entryCount++] = { nm, f.isDirectory(), (uint32_t)f.size() };
    f = dir.openNextFile();
  }
  dir.close();
  // simple alpha sort, directories first
  for (int i = 0; i < entryCount; i++) for (int j = i + 1; j < entryCount; j++) {
    bool swap = false;
    if (entries[j].isDir != entries[i].isDir) swap = entries[j].isDir && !entries[i].isDir;
    else swap = entries[j].name < entries[i].name;
    if (swap) { Entry tmp = entries[i]; entries[i] = entries[j]; entries[j] = tmp; }
  }
}

static String joinPath(const String &base, const String &name) {
  if (base == "/") return "/" + name;
  return base + "/" + name;
}

static String fmtSize(uint32_t s) {
  char b[16];
  if (s < 1024) snprintf(b, sizeof(b), "%luB", (unsigned long)s);
  else if (s < 1024 * 1024) snprintf(b, sizeof(b), "%luKB", (unsigned long)(s / 1024));
  else snprintf(b, sizeof(b), "%.1fMB", s / 1048576.0);
  return String(b);
}

static void drawList() {
  tft.fillScreen(cBG);
  uiHeader(useSD ? "SD CARD" : "HOME (FLASH)", useSD ? cGreen : cAmber);

  // volume tabs
  tft.fillRoundRect(SCR_W - 150, 30, 70, 20, 4, useSD ? cGreen : cPanel);
  tft.setTextFont(1); tft.setTextDatum(MC_DATUM);
  tft.setTextColor(useSD ? TFT_BLACK : cDim, useSD ? cGreen : cPanel);
  tft.drawString("SD", SCR_W - 115, 40);
  tft.fillRoundRect(SCR_W - 76, 30, 70, 20, 4, !useSD ? cAmber : cPanel);
  tft.setTextColor(!useSD ? TFT_BLACK : cDim, !useSD ? cAmber : cPanel);
  tft.drawString("HOME", SCR_W - 41, 40);

  tft.setTextFont(2); tft.setTextDatum(TL_DATUM);
  tft.setTextColor(cDim, cBG);
  String shown = curPath; if (shown.length() > 36) shown = "..." + shown.substring(shown.length() - 33);
  tft.drawString(shown, 8, 30);

  for (int i = 0; i < ROWS_VISIBLE && scrollTop + i < entryCount; i++) {
    int idx = scrollTop + i;
    int y = LIST_TOP + i * ROW_H;
    tft.fillRoundRect(6, y, SCR_W - 12, ROW_H - 4, 4, cPanel);
    tft.setTextColor(entries[idx].isDir ? cCyan : cWhite, cPanel);
    String label = (entries[idx].isDir ? "[DIR] " : "") + entries[idx].name;
    if (label.length() > 28) label = label.substring(0, 25) + "...";
    tft.drawString(label, 12, y + 10);
    if (!entries[idx].isDir) {
      tft.setTextDatum(TR_DATUM);
      tft.setTextColor(cDim, cPanel);
      tft.drawString(fmtSize(entries[idx].size), SCR_W - 16, y + 10);
      tft.setTextDatum(TL_DATUM);
    }
  }
  if (entryCount == 0) {
    tft.setTextColor(cDim, cBG);
    tft.drawString("(empty)", 16, LIST_TOP + 6);
  }
  // scroll hint
  if (scrollTop > 0) tft.fillTriangle(SCR_W/2-6, LIST_TOP-6, SCR_W/2+6, LIST_TOP-6, SCR_W/2, LIST_TOP-12, cDim);
  if (scrollTop + ROWS_VISIBLE < entryCount) {
    int y = LIST_TOP + ROWS_VISIBLE * ROW_H;
    tft.fillTriangle(SCR_W/2-6, y+4, SCR_W/2+6, y+4, SCR_W/2, y+10, cDim);
  }
}

static void viewTextFile(const String &path) {
  File f = vol().open(path, "r");
  if (!f) { uiToast("CANNOT OPEN", cRed); return; }
  String content = f.readString();
  f.close();
  if (content.length() > 6000) content = content.substring(0, 6000) + "\n...[truncated]";

  int scroll = 0;
  const int lineH = 14, top = 32, visLines = (SCR_H - top) / lineH;
  // wrap into lines of ~46 chars
  String lines[400]; int nLines = 0;
  int i = 0;
  while (i < (int)content.length() && nLines < 400) {
    int take = min(46, (int)content.length() - i);
    int nl = content.indexOf('\n', i);
    if (nl >= 0 && nl - i < take) take = nl - i;
    lines[nLines++] = content.substring(i, i + take);
    i += take; if (i < (int)content.length() && content[i] == '\n') i++;
  }

  TouchState t;
  bool redraw = true;
  while (true) {
    uiPollTouch(t);
    if (uiBackButton(t)) return;
    if (t.justReleased) {
      if (t.startY > SCR_H - 30 && t.startX < 40) { scroll = max(0, scroll - visLines); redraw = true; }
      if (t.startY > SCR_H - 30 && t.startX > SCR_W - 40) { scroll = min(max(0, nLines - visLines), scroll + visLines); redraw = true; }
    }
    if (redraw) {
      tft.fillScreen(cBG);
      String name = path; int sl = name.lastIndexOf('/'); if (sl >= 0) name = name.substring(sl+1);
      uiHeader(name.c_str(), cCyan);
      tft.setTextFont(1); tft.setTextDatum(TL_DATUM); tft.setTextColor(cWhite, cBG);
      for (int l = 0; l < visLines && scroll + l < nLines; l++)
        tft.drawString(lines[scroll + l], 6, top + l * lineH);
      tft.fillTriangle(8, SCR_H-8, 20, SCR_H-20, 20, SCR_H-4, cDim);
      tft.fillTriangle(SCR_W-8, SCR_H-8, SCR_W-20, SCR_H-20, SCR_W-20, SCR_H-4, cDim);
      redraw = false;
    }
    delay(10);
  }
}

void filesRun() {
  useSD = storageSDPresent();
  curPath = "/";
  loadDir();
  drawList();

  TouchState t;
  while (true) {
    uiPollTouch(t);
    if (uiBackButton(t)) {
      if (curPath != "/") {
        int sl = curPath.lastIndexOf('/');
        curPath = (sl <= 0) ? "/" : curPath.substring(0, sl);
        loadDir(); drawList(); continue;
      }
      return;
    }
    if (t.justReleased) {
      if (uiTapIn(t, SCR_W - 150, 30, 70, 20) && !useSD) {
        if (storageSDPresent()) { useSD = true; curPath = "/"; loadDir(); drawList(); }
        else uiToast("NO SD CARD", cRed);
        continue;
      }
      if (uiTapIn(t, SCR_W - 76, 30, 70, 20) && useSD) {
        useSD = false; curPath = "/"; loadDir(); drawList(); continue;
      }
      for (int i = 0; i < ROWS_VISIBLE && scrollTop + i < entryCount; i++) {
        int idx = scrollTop + i, y = LIST_TOP + i * ROW_H;
        if (uiTapIn(t, 6, y, SCR_W - 12, ROW_H - 4)) {
          if (entries[idx].isDir) {
            curPath = joinPath(curPath, entries[idx].name);
            loadDir(); drawList();
          } else if (looksTexty(entries[idx].name)) {
            viewTextFile(joinPath(curPath, entries[idx].name));
            drawList();
          } else {
            uiToast("UNSUPPORTED TYPE", cAmber);
          }
          break;
        }
      }
      if (uiTapIn(t, 0, LIST_TOP - 16, SCR_W, 10) && scrollTop > 0) { scrollTop -= ROWS_VISIBLE; if (scrollTop<0) scrollTop=0; drawList(); }
      if (uiTapIn(t, 0, LIST_TOP + ROWS_VISIBLE*ROW_H, SCR_W, 14) && scrollTop + ROWS_VISIBLE < entryCount) { scrollTop += ROWS_VISIBLE; drawList(); }
    }
    uiDrawToastIfActive();
    delay(10);
  }
}
