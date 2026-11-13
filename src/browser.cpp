#include "browser.h"
#include "storage.h"
#include "wifi_mgr.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define MAX_BODY     20000   // cap how much of a page we pull over, for RAM safety
#define MAX_LINKS    24
#define LINE_CHARS   48
#define MAX_LINES    260
#define MAX_HISTORY  12

struct Link { String url; String text; };

static String history[MAX_HISTORY];
static int histLen = 0, histPos = -1;

static String pageTitle = "";
static String lines[MAX_LINES]; static int nLines = 0;
static Link links[MAX_LINKS]; static int nLinks = 0;
static int scroll = 0;
static bool fromCache = false;

#define TOP 56
#define LINE_H 14
static int visLines() { return (SCR_H - TOP - 26) / LINE_H; }

static String normalizeUrl(String u) {
  u.trim();
  if (u.length() == 0) return u;
  if (!u.startsWith("http://") && !u.startsWith("https://")) u = "http://" + u;
  return u;
}

// crude HTML -> text + link extraction, bounded by MAX_BODY/MAX_LINKS
static void parseHtml(const String &html, const String &baseUrl) {
  nLines = 0; nLinks = 0; pageTitle = "";
  String text;
  text.reserve(min((size_t)html.length(), (size_t)MAX_BODY));

  bool inTag = false, inScript = false, inStyle = false, inTitle = false;
  String tagBuf;
  String curHref;
  bool collectingHref = false;

  for (size_t i = 0; i < html.length() && (int)text.length() < MAX_BODY; i++) {
    char c = html[i];
    if (c == '<') { inTag = true; tagBuf = ""; continue; }
    if (inTag) {
      if (c == '>') {
        inTag = false;
        String tl = tagBuf; tl.toLowerCase();
        if (tl.startsWith("script")) inScript = true;
        else if (tl.startsWith("/script")) inScript = false;
        else if (tl.startsWith("style")) inStyle = true;
        else if (tl.startsWith("/style")) inStyle = false;
        else if (tl.startsWith("title")) inTitle = true;
        else if (tl.startsWith("/title")) inTitle = false;
        else if (tl.startsWith("br") || tl.startsWith("/p") || tl.startsWith("/div") ||
                 tl.startsWith("/li") || tl.startsWith("/h") || tl.startsWith("/tr")) {
          text += '\n';
        } else if (tl.startsWith("a ") || tl == "a") {
          int hi = tl.indexOf("href=");
          if (hi >= 0 && nLinks < MAX_LINKS) {
            char q = tagBuf[hi + 5];
            int start = hi + 6, end;
            if (q == '"' || q == '\'') end = tagBuf.indexOf(q, start);
            else { start = hi + 5; end = tagBuf.indexOf(' ', start); if (end < 0) end = tagBuf.length(); }
            curHref = tagBuf.substring(start, end < 0 ? tagBuf.length() : end);
            collectingHref = true;
            text += "\x01"; // marker: link start
          }
        } else if (tl == "/a" && collectingHref) {
          text += "\x02"; // marker: link end
          if (!curHref.startsWith("http")) {
            if (curHref.startsWith("//")) curHref = "http:" + curHref;
            else if (curHref.startsWith("/")) {
              int p = baseUrl.indexOf('/', 8);
              curHref = (p > 0 ? baseUrl.substring(0, p) : baseUrl) + curHref;
            } else curHref = baseUrl + "/" + curHref;
          }
          if (nLinks < MAX_LINKS) links[nLinks].url = curHref;
          collectingHref = false;
        }
        continue;
      }
      tagBuf += c;
      continue;
    }
    if (inScript || inStyle) continue;
    if (inTitle) { pageTitle += c; continue; }
    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    text += c;
  }

  // collapse link markers into "[n] text" and collapse whitespace
  String out; out.reserve(text.length());
  int linkIdx = -1;
  bool lastSpace = false;
  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c == '\x01') {
      linkIdx++;
      if (linkIdx < nLinks) { out += " ["; out += String(linkIdx + 1); out += "]"; links[linkIdx].text = ""; }
      lastSpace = false;
      continue;
    }
    if (c == '\x02') { lastSpace = false; continue; }
    if (c == ' ' ) { if (!lastSpace) out += ' '; lastSpace = true; continue; }
    lastSpace = (c == '\n');
    out += c;
    if (linkIdx >= 0 && linkIdx < nLinks && links[linkIdx].text.length() < 30 && c != '\n') links[linkIdx].text += c;
  }
  pageTitle.trim();

  // wrap into fixed-width lines
  int i = 0;
  while (i < (int)out.length() && nLines < MAX_LINES) {
    while (i < (int)out.length() && out[i] == '\n') { i++; }
    int remaining = out.length() - i;
    if (remaining <= 0) break;
    int take = min(LINE_CHARS, remaining);
    int nl = out.indexOf('\n', i);
    if (nl >= 0 && nl - i < take) take = nl - i;
    else if (take == LINE_CHARS) {
      int sp = out.lastIndexOf(' ', i + take);
      if (sp > i) take = sp - i;
    }
    String ln = out.substring(i, i + take); ln.trim();
    if (ln.length()) lines[nLines++] = ln;
    i += take;
  }
}

static bool fetchUrl(const String &url, String &outHtml) {
  HTTPClient http;
  bool https = url.startsWith("https://");
  WiFiClientSecure sClient;
  bool ok;
  if (https) {
    sClient.setInsecure(); // text browser: no cert pinning, accept any cert to stay usable
    ok = http.begin(sClient, url);
  } else {
    ok = http.begin(url);
  }
  if (!ok) return false;
  http.setTimeout(8000);
  http.addHeader("User-Agent", "CYD-Browser/1.0");
  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }

  WiFiClient *stream = http.getStreamPtr();
  int total = http.getSize();
  int toRead = (total > 0) ? min(total, MAX_BODY) : MAX_BODY;
  outHtml.reserve(toRead);
  uint32_t t0 = millis();
  while ((int)outHtml.length() < toRead && millis() - t0 < 10000) {
    while (http.connected() && stream->available() && (int)outHtml.length() < toRead) {
      outHtml += (char)stream->read();
    }
    if (!http.connected() && !stream->available()) break;
    delay(1);
  }
  http.end();
  return outHtml.length() > 0;
}

static void drawPage(const String &url, bool loading) {
  tft.fillScreen(cBG);
  uiHeader(pageTitle.length() ? pageTitle.c_str() : "BROWSER", cCyan);

  tft.fillRoundRect(4, 30, SCR_W - 8, 20, 4, cPanel);
  tft.setTextFont(1); tft.setTextDatum(ML_DATUM); tft.setTextColor(cDim, cPanel);
  String shown = url; if (shown.length() > 50) shown = shown.substring(0, 47) + "...";
  tft.drawString(shown.length() ? shown : "tap to enter a URL", 8, 40);

  if (loading) {
    tft.setTextDatum(MC_DATUM); tft.setTextColor(cAmber, cBG);
    tft.drawString("Loading...", SCR_W/2, SCR_H/2);
    tft.setTextDatum(TL_DATUM);
    return;
  }

  tft.setTextFont(1); tft.setTextColor(cWhite, cBG);
  for (int l = 0; l < visLines() && scroll + l < nLines; l++)
    tft.drawString(lines[scroll + l], 6, TOP + l * LINE_H);

  if (fromCache) {
    tft.setTextDatum(TR_DATUM); tft.setTextColor(cAmber, cBG);
    tft.drawString("CACHED", SCR_W - 6, 34 + 4);
    tft.setTextDatum(TL_DATUM);
  }

  // bottom bar: back / scroll up / scroll down / links / go
  int by = SCR_H - 24;
  tft.fillRect(0, by, SCR_W, 24, cPanel);
  tft.setTextFont(1); tft.setTextDatum(MC_DATUM); tft.setTextColor(cWhite, cPanel);
  tft.drawString("<PG", 24, by + 12);
  tft.drawString("PG>", 70, by + 12);
  tft.drawString("LINKS", 130, by + 12);
  tft.drawString("GO", SCR_W - 50, by + 12);
  tft.drawString("RELOAD", SCR_W - 16, by + 12);
  tft.setTextDatum(TL_DATUM);
}

static void showLinksMenu() {
  if (nLinks == 0) { uiToast("NO LINKS FOUND", cAmber); return; }
  tft.fillScreen(cBG);
  uiHeader("LINKS ON PAGE", cMag);
  int rowH = 22, top = 32;
  int shown = min(nLinks, (SCR_H - top) / rowH);
  for (int i = 0; i < shown; i++) {
    int y = top + i * rowH;
    tft.fillRoundRect(4, y, SCR_W - 8, rowH - 2, 3, cPanel);
    tft.setTextFont(1); tft.setTextDatum(ML_DATUM); tft.setTextColor(cCyan, cPanel);
    String label = "[" + String(i + 1) + "] " + (links[i].text.length() ? links[i].text : links[i].url);
    if (label.length() > 50) label = label.substring(0, 47) + "...";
    tft.drawString(label, 8, y + rowH/2 - 2);
  }
  tft.setTextDatum(TL_DATUM);

  TouchState t;
  while (true) {
    uiPollTouch(t);
    if (uiBackButton(t)) return;
    if (t.justReleased) {
      for (int i = 0; i < shown; i++) {
        int y = top + i * rowH;
        if (uiTapIn(t, 4, y, SCR_W - 8, rowH - 2)) {
          String chosen = links[i].url;
          if (histPos < MAX_HISTORY - 1) history[++histPos] = chosen; else { for(int k=0;k<MAX_HISTORY-1;k++) history[k]=history[k+1]; history[MAX_HISTORY-1]=chosen; }
          histLen = histPos + 1;
          return;
        }
      }
    }
    delay(10);
  }
}

static void loadAndRender(const String &url) {
  drawPage(url, true);
  fromCache = false;
  String html;

  if (!wifiIsConnected()) {
    if (storageCacheGet(url.c_str(), html)) { fromCache = true; }
    else { uiToast("NO WIFI / NOT CACHED", cRed); pageTitle=""; nLines=0; nLinks=0; drawPage(url, false); return; }
  } else {
    if (!fetchUrl(url, html)) {
      if (storageCacheGet(url.c_str(), html)) fromCache = true;
      else { uiToast("FETCH FAILED", cRed); pageTitle=""; nLines=0; nLinks=0; drawPage(url, false); return; }
    } else {
      storageCachePut(url.c_str(), html);
    }
  }
  parseHtml(html, url);
  scroll = 0;
  drawPage(url, false);
}

void browserRun() {
  histLen = 0; histPos = -1;
  nLines = 0; nLinks = 0; pageTitle = "";
  String current = "";
  drawPage(current, false);

  TouchState t;
  while (true) {
    uiPollTouch(t);
    if (uiBackButton(t)) {
      if (histPos > 0) { histPos--; current = history[histPos]; loadAndRender(current); continue; }
      return;
    }
    if (t.justReleased) {
      if (uiTapIn(t, 4, 30, SCR_W - 8, 20)) {
        char buf[96]; strncpy(buf, current.c_str(), sizeof(buf)); buf[sizeof(buf)-1]=0;
        if (uiKeyboard("GO TO URL", buf, sizeof(buf), false)) {
          current = normalizeUrl(buf);
          if (histPos < MAX_HISTORY - 1) history[++histPos] = current; else { for(int k=0;k<MAX_HISTORY-1;k++) history[k]=history[k+1]; history[MAX_HISTORY-1]=current; histPos=MAX_HISTORY-1; }
          histLen = histPos + 1;
          loadAndRender(current);
        } else drawPage(current, false);
        continue;
      }
      int by = SCR_H - 24;
      if (t.startY >= by) {
        if (t.startX < 48) { scroll = max(0, scroll - visLines()); drawPage(current, false); }
        else if (t.startX < 100) { scroll = min(max(0, nLines - visLines()), scroll + visLines()); drawPage(current, false); }
        else if (t.startX < 170) { showLinksMenu(); drawPage(current, false); }
        else if (t.startX < SCR_W - 70) { /* GO: reuse URL bar */
          char buf[96]; strncpy(buf, current.c_str(), sizeof(buf)); buf[sizeof(buf)-1]=0;
          if (uiKeyboard("GO TO URL", buf, sizeof(buf), false)) {
            current = normalizeUrl(buf);
            if (histPos < MAX_HISTORY - 1) history[++histPos] = current; else { for(int k=0;k<MAX_HISTORY-1;k++) history[k]=history[k+1]; history[MAX_HISTORY-1]=current; histPos=MAX_HISTORY-1; }
            histLen = histPos + 1;
            loadAndRender(current);
          } else drawPage(current, false);
        } else if (current.length()) {
          loadAndRender(current); // RELOAD
        }
      }
    }
    uiDrawToastIfActive();
    delay(10);
  }
}
