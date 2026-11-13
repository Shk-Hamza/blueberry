# blueberry

🚧 **Under construction — untested on real hardware, expect bugs.** 🚧

Custom phone-like firmware for the **CYD 2-USB** (ESP32-2432S028, ST7789 variant). Touch-driven home screen, Wi-Fi, a text-based browser, an SD/flash file explorer, interactive demoscene effects, and a handful of mini games — all built around one display/touch UI layer.

Made by [Shk-Hamza](https://github.com/Shk-Hamza) — [shkhamza.me](https://shkhamza.me)

## What works (on paper — needs real-hardware testing and PRs)

- **Home screen** — phone-style app grid with a status bar (Wi-Fi signal, SD presence, clock once NTP-synced).
- **Wi-Fi** — scan, tap-to-connect with an on-screen keyboard for the password, credentials saved to flash (NVS) so it reconnects on boot.
- **Browser** — minimal HTTP/HTTPS text browser. Strips HTML to wrapped text plus a tappable numbered link list, paginates, keeps history, and caches every page (to SD if present, else internal flash) so a revisit works offline.
- **Files** — touch file explorer for both the SD card and internal flash (mounted as `/home` on LittleFS), with a built-in text viewer.
- **Effects** — six touch-reactive demoscene effects: Matrix rain, starfield, plasma, fire, Conway's Game of Life, and a bouncing logo.
- **Games** — Snake, Pong (vs. a simple AI), Breakout, Flappy, and Whack-a-bug, each with high scores saved to flash.

## Hardware target

ESP32-2432S028R ("CYD" — Cheap Yellow Display), 2-USB variant, 320×240 ST7789 TFT + XPT2046 resistive touch, with a microSD slot. Pin mappings live in `include/config.h` — SD and touch pinouts vary a bit across board batches, so check there first if something doesn't mount or respond.

## Build

Requires [PlatformIO](https://platformio.org/).

```sh
git clone https://github.com/Shk-Hamza/blueberry
cd blueberry
pio run -t uploadfs   # formats and writes the internal /home filesystem
pio run -t upload     # flashes the firmware
```

If colors look inverted or swapped, or touch axes feel reversed, see the comments in `platformio.ini` and `include/config.h` / `src/ui.cpp`.

## Known rough edges

- HTTPS uses `setInsecure()` — no certificate validation. Fine for a hobby text browser, not for anything sensitive.
- No PSRAM assumed, so fetched pages are capped at ~20KB; heavy modern sites will truncate or fail to render meaningfully.
- SD pinout in `include/config.h` is the common wiring for this board — not guaranteed across every batch.
- Not yet tested on physical hardware — flashing feedback and issues/PRs very welcome.

## License

GPL-3.0 — see [LICENSE](LICENSE).
