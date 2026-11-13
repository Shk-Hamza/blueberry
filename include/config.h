#pragma once
#include <Arduino.h>

// ---------- display geometry ----------
#define SCR_W 320
#define SCR_H 240

// ---------- touch (XPT2046 on VSPI, separate from TFT HSPI pins set in platformio.ini) ----------
#define T_CS   33
#define T_IRQ  36
#define T_CLK  25
#define T_MOSI 32
#define T_MISO 39
// if left/right feels reversed, swap the two numbers in the X map() inside ui.cpp's uiPollTouch()
// if up/down feels reversed, swap the two numbers in the Y map() the same way

// ---------- SD card ----------
// Most 2-USB CYD boards (ESP32-2432S028R) wire the SD slot on its own bus.
// This is the common pinout; if your card doesn't mount, check your board's
// silkscreen/schematic and adjust these four lines.
#define SD_CS   5
#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 23

// ---------- misc onboard I/O ----------
#define PIN_BACKLIGHT 21
#define PIN_RGB_LED   4     // some boards have no RGB LED fitted - harmless if absent
#define PIN_SPEAKER   26    // passive buzzer / amp input, if fitted - harmless if absent

// ---------- persistent storage namespaces ----------
#define NVS_NAMESPACE      "cyd"
#define NVS_WIFI_NAMESPACE "wifi"

// ---------- filesystem layout ----------
// LittleFS is mounted at root and treated as the device's internal "/home".
#define HOME_ROOT  "/home"
#define CACHE_DIR  "/cache"   // created on whichever storage is active (SD preferred)

// ---------- timezone ----------
#define TZ_OFFSET_SEC (5 * 3600)   // Asia/Karachi, UTC+5, no DST
