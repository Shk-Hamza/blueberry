#include "storage.h"
#include "config.h"
#include "ui.h"  // for g_sdPresent
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>

static SPIClass sdSPI(HSPI);
static bool sdOk = false;

static void ensureDir(FS &fs, const char *path) {
  if (!fs.exists(path)) fs.mkdir(path);
}

void storageInit() {
  LittleFS.begin(true);          // format on first boot if needed
  ensureDir(LittleFS, HOME_ROOT);
  ensureDir(LittleFS, CACHE_DIR);

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdOk = SD.begin(SD_CS, sdSPI, 20000000);
  if (sdOk) ensureDir(SD, CACHE_DIR);
  g_sdPresent = sdOk;
}

bool storageSDRetry() {
  SD.end();
  sdOk = SD.begin(SD_CS, sdSPI, 20000000);
  if (sdOk) ensureDir(SD, CACHE_DIR);
  g_sdPresent = sdOk;
  return sdOk;
}

bool storageSDPresent() { return sdOk; }

// turn an arbitrary key (e.g. a URL) into a short, safe filename
static String keyToPath(const char *key) {
  uint32_t h = 2166136261u;
  for (const char *p = key; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; }
  char buf[24];
  snprintf(buf, sizeof(buf), "%s/%08lx.c", CACHE_DIR, (unsigned long)h);
  return String(buf);
}

bool storageCacheGet(const char *key, String &out) {
  String path = keyToPath(key);
  File f = sdOk ? SD.open(path) : LittleFS.open(path, "r");
  if (!f) return false;
  out = f.readString();
  f.close();
  return true;
}

void storageCachePut(const char *key, const String &data) {
  String path = keyToPath(key);
  File f = sdOk ? SD.open(path, FILE_WRITE) : LittleFS.open(path, "w");
  if (!f) return;
  f.print(data);
  f.close();
}

void storageCacheClear() {
  FS &fs = sdOk ? (FS &)SD : (FS &)LittleFS;
  File dir = fs.open(CACHE_DIR);
  if (!dir) return;
  File f = dir.openNextFile();
  while (f) {
    String name = f.name();
    f.close();
    String full = name.startsWith("/") ? name : String(CACHE_DIR) + "/" + name;
    fs.remove(full);
    f = dir.openNextFile();
  }
  dir.close();
}

size_t storageCacheBytes() {
  FS &fs = sdOk ? (FS &)SD : (FS &)LittleFS;
  File dir = fs.open(CACHE_DIR);
  if (!dir) return 0;
  size_t total = 0;
  File f = dir.openNextFile();
  while (f) { total += f.size(); f = dir.openNextFile(); }
  dir.close();
  return total;
}
