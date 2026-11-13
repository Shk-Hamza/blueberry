#include "wifi_mgr.h"
#include "ui.h"
#include "config.h"
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

static Preferences prefs;
static bool triedAutoConnect = false;

static void syncClockOnce() {
  if (g_timeSynced) return;
  configTime(TZ_OFFSET_SEC, 0, "pool.ntp.org", "time.google.com");
  struct tm ti;
  if (getLocalTime(&ti, 1500)) g_timeSynced = true;
}

void wifiMgrInit() {
  WiFi.mode(WIFI_STA);
  prefs.begin(NVS_WIFI_NAMESPACE, true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();
  if (ssid.length()) {
    WiFi.begin(ssid.c_str(), pass.c_str());
    triedAutoConnect = true;
  }
}

void wifiMgrPoll() {
  bool now = WiFi.status() == WL_CONNECTED;
  g_wifiConnected = now;
  if (now) syncClockOnce();
}

bool wifiIsConnected() { return WiFi.status() == WL_CONNECTED; }

static void saveCreds(const String &ssid, const String &pass) {
  prefs.begin(NVS_WIFI_NAMESPACE, false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

void wifiRun() {
  TouchState t;
  int n = -1;
  String ssids[20];
  int32_t rssi[20];
  bool secure[20];
  int count = 0;
  bool scanning = true;

  tft.fillScreen(cBG);
  uiHeader("WI-FI", cCyan);
  uiBusy("Scanning...");

  n = WiFi.scanNetworks();
  count = min(n, 20);
  for (int i = 0; i < count; i++) {
    ssids[i] = WiFi.SSID(i);
    rssi[i] = WiFi.RSSI(i);
    secure[i] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  scanning = false;

  bool needRedraw = true;
  while (true) {
    uiPollTouch(t);
    if (uiBackButton(t)) { WiFi.scanDelete(); return; }

    if (needRedraw) {
      tft.fillScreen(cBG);
      uiHeader("WI-FI", cCyan);
      tft.setTextFont(2);
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(wifiIsConnected() ? cGreen : cDim, cBG);
      String status = wifiIsConnected() ? ("Connected: " + WiFi.SSID()) : "Not connected";
      tft.drawString(status, 8, 30);

      int rowH = 26, top = 54;
      for (int i = 0; i < count && i < 7; i++) {
        int y = top + i * rowH;
        tft.fillRoundRect(6, y, SCR_W - 12, rowH - 4, 4, cPanel);
        tft.setTextColor(cWhite, cPanel);
        String label = ssids[i] + (secure[i] ? "  [lock]" : "  [open]");
        tft.drawString(label, 12, y + 10);
        int bars = rssi[i] > -55 ? 3 : rssi[i] > -75 ? 2 : 1;
        for (int b = 0; b < 3; b++)
          tft.fillRect(SCR_W - 34 + b * 7, y + 18 - (4 + b * 4), 5, 4 + b * 4, b < bars ? cCyan : cDim);
      }
      tft.fillRoundRect(SCR_W - 70, SCR_H - 30, 64, 24, 6, cAmber);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_BLACK, cAmber);
      tft.drawString("RESCAN", SCR_W - 38, SCR_H - 18);
      tft.setTextDatum(TL_DATUM);
      needRedraw = false;
    }

    if (t.justReleased) {
      int rowH = 26, top = 54;
      for (int i = 0; i < count && i < 7; i++) {
        int y = top + i * rowH;
        if (uiTapIn(t, 6, y, SCR_W - 12, rowH - 4)) {
          char pass[64] = "";
          bool ok = true;
          if (secure[i]) ok = uiKeyboard(ssids[i].c_str(), pass, sizeof(pass), true);
          if (ok) {
            uiBusy("Connecting...");
            WiFi.disconnect();
            WiFi.begin(ssids[i].c_str(), pass);
            uint32_t t0 = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) delay(200);
            if (WiFi.status() == WL_CONNECTED) {
              saveCreds(ssids[i], pass);
              g_wifiConnected = true;
              syncClockOnce();
              uiToast("CONNECTED", cGreen);
            } else {
              uiToast("FAILED", cRed);
            }
          }
          needRedraw = true;
          break;
        }
      }
      if (uiTapIn(t, SCR_W - 70, SCR_H - 30, 64, 24)) {
        uiBusy("Scanning...");
        int nn = WiFi.scanNetworks();
        count = min(nn, 20);
        for (int i = 0; i < count; i++) {
          ssids[i] = WiFi.SSID(i);
          rssi[i] = WiFi.RSSI(i);
          secure[i] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        }
        needRedraw = true;
      }
    }
    uiDrawToastIfActive();
    delay(10);
  }
}
