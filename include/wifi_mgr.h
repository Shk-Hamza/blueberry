#pragma once
#include <Arduino.h>

// Loads the last-saved SSID/password from NVS and starts a non-blocking
// connection attempt. Call once from setup(), after storageInit().
void wifiMgrInit();

// Call occasionally (e.g. once per home-screen frame) to update g_wifiConnected
// and sync the clock the first time a connection succeeds.
void wifiMgrPoll();

bool wifiIsConnected();

// Full-screen scan/connect UI. Blocks until the user taps the header back chip.
void wifiRun();
