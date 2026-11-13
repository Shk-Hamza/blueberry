#pragma once
#include <Arduino.h>

// Mounts LittleFS (always, used as internal "/home") and probes the SD card.
// Call once from setup(), after uiInit().
void storageInit();

bool storageSDPresent();   // true if the SD card is mounted and usable
bool storageSDRetry();     // call to re-probe the SD slot (e.g. after the user inserts a card)

// Cache: small key->text blobs. Prefers SD (so it survives reboots and doesn't
// wear the internal flash); falls back to LittleFS automatically when no SD
// card is present. Keys are turned into safe filenames internally.
bool storageCacheGet(const char *key, String &out);
void storageCachePut(const char *key, const String &data);
void storageCacheClear();
size_t storageCacheBytes();
