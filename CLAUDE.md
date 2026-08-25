# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

This is a **documentation and recovery-artifact archive**, not a buildable
source tree. It records how a bricked ESP32-S3 "MinerMaker" solo-miner
device was diagnosed, re-identified, and revived by building a custom
firmware variant on top of the [NerdMiner_v2](https://github.com/BitMaker-hub/NerdMiner_v2)
project. There is no compiler/toolchain invocation to run inside this repo —
the actual PlatformIO project lives externally (NerdMiner_v2, cloned
separately); this repo holds only the *overlay files* needed to add a new
board environment to it, plus prebuilt binaries and hardware docs.

`README.md` is the primary, authoritative document — it contains
the full diagnosis narrative, pinout, flashing instructions, and a
step-by-step guide for reapplying the custom files onto a fresh clone of
NerdMiner_v2. Read it before making changes; keep it in sync with any change
made here.

## Repository layout

- `README.md` — full write-up: symptoms, diagnosis, fix, pinout, reflash/rebuild steps, button behavior, ideas for future work.
- `firmware/MinerMaker154_factory.bin` — full flashable image (bootloader+partitions+app), flash at offset `0x0`.
- `firmware/MinerMaker154_firmware.bin` — app-only image for updates, flash at offset `0x10000`.
- `firmware/nerdminer-src-custom-files/` — the custom files that turn a plain NerdMiner_v2 checkout into the `MinerMaker154` board target (see "Rebuilding from source" below).
- `docs/interface.txt` — vendor-supplied display pinout (source of truth for the GPIO table).
- `docs/product_parametr.avif`, `docs/user_maual.pdf` — vendor product docs.
- `photos/` — photos of the disassembled board and working screen (useful for confirming board identity, connector layout).
- `xxx.wbapp` — file of unknown purpose, carried over as-is from the original device dump.

## Hardware facts (do not re-derive, verified by direct diagnosis)

- Chip: ESP32-S3-WROOM-1 module **N8R8** (8MB flash, 8MB PSRAM), silicon rev v0.2.
- Board: generic "ESP32S3 1.54 TFT LCD V1.0" by ZJYUNJIE, rebranded "MinerMaker" — a NerdMiner_v2 clone with swapped branding; vendor firmware/sources were never published.
- Display: ST7789, SPI, 240×240, 1.54".
- USB: native USB-Serial/JTAG (`/dev/ttyACM0` on Linux) — not a separate USB-UART bridge.
- Display pinout (SPI, from `docs/interface.txt`):

  | Signal | GPIO |
  |---|---|
  | DC | 42 |
  | CS | 41 |
  | SCK/CLK | 40 |
  | SDA/MOSI | 39 |
  | RESET | 38 |
  | LCD_BL (backlight) | 21 |
  | Button | 0 |

- Flash access on Linux requires the user to be in the `uucp` group (`sudo usermod -aG uucp $USER`) to read/write `/dev/ttyACM0`.

## Flashing a prebuilt image

Requires `esptool` (`pip install esptool`) and `/dev/ttyACM0` access:

```bash
esptool --port /dev/ttyACM0 erase-flash
esptool --port /dev/ttyACM0 --baud 460800 write-flash 0x0 firmware/MinerMaker154_factory.bin
```

After flashing: connect Wi-Fi to the `NerdMinerAP` access point it creates,
open `192.168.4.1`, select the home network, enter its password, and enter
a **BTC receiving address** (not an xpub/ypub/zpub — see gotcha below).

## Rebuilding the firmware from source

The `MinerMaker154` PlatformIO environment does not exist upstream; it is
added by overlaying files from `firmware/nerdminer-src-custom-files/` onto
a fresh NerdMiner_v2 checkout:

```bash
git clone https://github.com/BitMaker-hub/NerdMiner_v2.git
```

Then, relative to that checkout's root:

| File in `nerdminer-src-custom-files/` | Destination |
|---|---|
| `platformio.ini` | repo root (adds `[env:MinerMaker154]`; or manually merge just that section into the existing file) |
| `Setup999_MinerMaker154.h` | `lib/TFT_eSPI/User_Setups/` |
| `minerMaker154.h` | `src/drivers/devices/` |
| `minerMaker154DisplayDriver.cpp` | `src/drivers/displays/` |
| `webSettings.h`, `webSettings.cpp` | `src/` (self-guarded with `#ifdef MINERMAKER154`, so harmless if left in place for other envs) |

Plus three manual one-line additions to existing upstream files:

- `src/drivers/devices/device.h`: add `#elif defined(MINERMAKER154) #include "minerMaker154.h"`
- `src/drivers/displays/displayDriver.h`: add `extern DisplayDriver minerMaker154DisplayDriver;`
- `src/drivers/displays/display.cpp`: add `#ifdef MINERMAKER_DISPLAY` branch setting `currentDisplayDriver = &minerMaker154DisplayDriver;`
- `lib/TFT_eSPI/User_Setup_Select.h`: add `#ifdef MINERMAKER154` → `#include <User_Setups/Setup999_MinerMaker154.h>`

Then build with PlatformIO from that checkout:

```bash
pio run -e MinerMaker154
```

Output lands in that checkout's `firmware/dev/` (not in this repo — copy
the resulting `.bin` files back into this repo's `firmware/` if updating
the archived artifacts).

**PlatformIO is not preinstalled in a fresh environment.** It can be added
without sudo via `uv tool install platformio` (then `~/.local/bin/pio`) —
this is the fastest path if you need to actually verify a firmware change
compiles rather than just eyeballing it. The first build downloads the
ESP32-S3 Xtensa toolchain (a few hundred MB); subsequent builds are fast
(~15-30s). This has been done at least once and confirmed the
`MinerMaker154` env builds clean end-to-end with the overlay applied as
documented above.

### Custom-files architecture notes

- `minerMaker154.h` is the device definition: it sets `PIN_BUTTON_1` and
  defines `MINERMAKER_DISPLAY`, which gates the display driver file.
- `minerMaker154DisplayDriver.cpp` implements a themed (dark/yellow/blue,
  digital-font hero numbers) but not vendor-branded UI — deliberately not
  replicating the original vendor's "MINER MAKER" wordmark/logo, which was
  never published — using `TFT_eSPI` + `OpenFontRender` (for the
  `DigitalNumbers` font already bundled in upstream's `src/media/myFonts.h`).
  It follows NerdMiner_v2's `DisplayDriver` struct contract: init/toggle-
  screen/toggle-rotation/loading-screen/setup-screen/an array of cyclic
  screen-draw functions/animate/LED callback. Mining, clock and coin data
  are pulled from upstream's `monitor.cpp` via `getMiningData()` /
  `getClockData()` / `getCoinData()` — this file only renders, it doesn't compute.
  - **Rendering is sprite-based**: every screen draws into an off-screen
    `TFT_eSprite` (`mm_bg`) and calls `pushSprite(0,0)` once at the end,
    instead of drawing straight to the panel — this is what avoids the
    visible black flash on every 1-second data refresh (`drawCurrentScreen()`
    in upstream `mining.cpp`'s `runMonitor()` task calls the active cyclic
    screen function every 1000ms). Follow this pattern for any new screen.
  - **Backlight/auto-wake state** (`backlightOn`, `autoWaking`,
    `lastOffMillis`, `autoWakeStart`) lives in file-static variables and is
    driven from `minerMaker154_AnimateCurrentScreen()`, which upstream
    calls roughly every 100ms (`DELAY` in `mining.cpp`) regardless of
    mining/backlight state — this is also where `minerMaker154_WebSettingsLoop()`
    gets serviced, since there's no other per-tick hook available without
    editing upstream's `NerdMinerV2.ino.cpp`.
  - The **setup screen's QR code** uses the `ricmoo/QRCode` library
    (`lib_deps` in `platformio.ini`) to encode
    `WIFI:T:WPA;S:NerdMinerAP;P:MineYourCoins;;` — `NerdMinerAP` /
    `MineYourCoins` are NerdMiner_v2's own hardcoded AP defaults
    (`src/drivers/storage/storage.h`: `DEFAULT_SSID`/`DEFAULT_WIFIPW`), not
    board-specific, so this stays correct unless upstream changes those.
- `webSettings.cpp`/`.h` add a small always-on `WebServer` on port 8080
  (separate from WiFiManager's captive-portal server on 80, which is only
  alive during initial setup) exposing a form for `BtcWallet`/`PoolAddress`/
  `PoolPort`/`PoolPassword`/`Timezone` — the same fields the setup portal
  collects. It reads/writes the global `Settings` (`TSettings`, defined in
  `src/drivers/storage/storage.h`) and calls `nvMem.saveConfig(&Settings)`
  (both declared `extern` from `wManager.cpp`, which owns them but doesn't
  expose them via `wManager.h`) then `ESP.restart()`, mirroring how the
  setup portal itself applies changes. Wi-Fi SSID/password are intentionally
  *not* editable here — those still require the full portal/reset flow.
  Gated by fixed-constant HTTP Basic Auth (`MM_WEB_USER`/`MM_WEB_PASS`,
  not stored in `TSettings`/not user-editable). Advertised via mDNS
  (`ESPmDNS`, hostname `minermaker` → `minermaker.local:8080`), registered
  once WiFi is first seen connected inside `minerMaker154_WebSettingsLoop()`.
- **Backlight is PWM-driven** (ESP32 LEDC, channel `MM_BL_CHANNEL`) rather
  than a plain `digitalWrite`, so on/off is a ~400ms fade
  (`mm_fadeBacklightTo()`) — this uses the arduino-esp32 2.x channel-based
  LEDC API (`ledcSetup`/`ledcAttachPin`/`ledcWrite(channel, duty)`), which
  is what `platform = espressif32@6.6.0` pins (arduino-esp32 core ~2.0.14);
  don't switch to the pin-based `ledcAttach(pin,...)` API from core 3.x
  without also bumping the pinned platform version.
  **Ordering matters and bit us once**: `ledcSetup`/`ledcAttachPin` for
  `TFT_BL` must happen *after* `mm_tft.init()`, never before — `TFT_eSPI`'s
  own `init()` does `pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, ...)`
  internally whenever `TFT_BL`/`TFT_BACKLIGHT_ON` are defined (see
  `TFT_eSPI.cpp`), which silently detaches any earlier LEDC binding on that
  pin. Confirmed on hardware: with the wrong order, the multi-click handler
  fires correctly (visible in serial log) but the backlight never visibly
  dims — `ledcWrite()` calls no longer reach the pin. Fixed and confirmed
  working (both directions of the fade) after reflashing.
- **Backlight on/off state persists across power cycles** via a dedicated
  `Preferences` (NVS) namespace, `"mm154bl"` — deliberately separate from
  the project's own `TSettings`/`nvMemory` storage (avoids touching
  `storage.h`/`nvMemory.cpp`, which are upstream-owned and shared by every
  board). Written only from the manual toggle
  (`minerMaker154_AlternateScreenState()`); the temporary auto-wake fades
  intentionally don't touch it.
- **Every stat box clips its own text** (`mm_clipBegin()`/`mm_clipEnd()`,
  thin wrappers around `TFT_eSprite::setViewport(x,y,w,h,false)` /
  `resetViewport()` — `vpDatum=false` keeps coordinates absolute/sprite-
  space, so existing draw calls don't need rewriting relative to the
  viewport). `mm_statCell()` clips internally; screens that draw directly
  into a decorative box (the Mining screen's blue hero box, the Wi-Fi/Web
  settings box) wrap that region the same way. This was added after real
  hardware photos showed labels/values bleeding into their neighbour or
  off the panel edge (`GLOBAL HASH`/`DIFFICULTY` merging, `HASHRATE (KH/s)`
  running into `SHARES`, `BLOCKS LEFT`'s value running off-screen) — don't
  add a new stat box or hero number without clipping it the same way,
  since upstream's data strings (price, hashrate, block height, ...) vary
  in length and nothing here previously guaranteed they'd fit.
- **`mm_bigNumber()` auto-shrinks to fit, on BOTH axes**: it takes
  `maxFontSize`, `maxWidth` AND `maxHeight`, measures via
  `OpenFontRender::getTextWidth()`/`getTextHeight()`, and steps the font
  size down (2px steps, floor 12px) until the string fits both — rather
  than a single hardcoded size that happens to fit whatever value was on
  screen when it was tuned.
  **Do NOT wrap a `mm_bigNumber()` call in `mm_clipBegin`/`mm_clipEnd` and
  assume that bounds it** — confirmed on hardware (two rounds of photos)
  that `OpenFontRender` draws straight into the sprite buffer and ignores
  `TFT_eSprite::setViewport()` clipping entirely; a clipped call still
  overflowed its box exactly as if unclipped. `setViewport` clipping only
  actually constrains plain `mm_bg.drawString()` calls (native TFT_eSPI
  text) — that's what `mm_statCell()` and the decorative-box wraps rely
  on, and why only measurement-based sizing works for the digital font.
  **`OpenFontRender::getTextHeight()` is unreliable for strings containing
  `:`** — confirmed on hardware (a third round of photos) that it
  under-reports height for a colon-bearing string in this font
  specifically (pure-digit strings like a block height measured and fit
  correctly; `"HH:MM"` did not, even with the maxHeight fix above in
  place). Any clock-like value should avoid `mm_bigNumber()`/
  `OpenFontRender` entirely — use the bundled `DSEG7_Classic_Bold_{12,17,32}`
  `GFXfont`s instead (`src/media/myFonts.h`) via the normal
  `setFreeFont()`/`setTextSize()`/`textWidth()` path, which measures
  correctly because it's the ordinary TFT_eSPI font engine, not
  OpenFontRender. This is also what `mm_statCell()`'s values and the
  Clock/Status screens' big time now use — don't reintroduce
  `mm_bigNumber()` for anything that renders a colon.
- **CSS gotcha**: don't reuse a `0xRRGGBB`-style RGB565 display color
  literal (e.g. `MM_YELLOW`/`0xFEA0`) as a CSS hex color string in
  `webSettings.cpp`'s HTML. A 4-hex-digit CSS color is `#RGBA` shorthand,
  not a truncated RGB565 value — `#FEA0` parsed as `#FFEEAA00`, i.e. fully
  transparent, which is why the settings page's submit button was
  invisible until this was caught. Use real 6-digit hex for web CSS.
- **Wi-Fi signal bars** in the header come from `WiFi.RSSI()` (`mm_wifiBars()`),
  not just `WiFi.status()`.
- `Setup999_MinerMaker154.h` is a `TFT_eSPI` `User_Setup` (ID 999) — pins,
  `ST7789_DRIVER`, BGR order, inversion-on, SPI frequencies. This is
  TFT_eSPI's own config layer; the `[env:MinerMaker154]` build additionally
  ignores nothing special beyond upstream's usual `lib_ignore` list.
- `platformio.ini`'s `[env:MinerMaker154]` mirrors the sibling `NerdminerV2`
  env (same board `esp32-s3-devkitc-1`, `qio_opi` PSRAM, native USB CDC)
  but swaps in `-D MINERMAKER154=1` instead of `-D NERDMINERV2=1`, and adds
  `ricmoo/QRCode` to `lib_deps`.

## Known gotcha: esptool read-flash stalls on this board's native USB-JTAG

Confirmed by actually flashing a real unit: `esptool read-flash` of a large
region reliably stalls (`Packet content transfer stopped`) at the same
absolute flash address every time, independent of baud rate or where the
read started — a quirk of continuous reads over the ESP32-S3's native
USB-Serial/JTAG mode on this board, not a bad cable/link. `write-flash` is
unaffected (different, block-acknowledged protocol; self-verifies via a
hash check) and completed cleanly on the first try. `scripts/safe_reflash.sh`
works around this by reading its pre-update backup in small chunks
(separate connections per chunk) instead of one continuous read. Keep this
in mind before adding any other read-flash-based tooling for this board.

## Known gotcha: wallet address field

The pool (`public-pool.io:3333`) silently rejects an **xpub/ypub/zpub**
(extended public key) entered in the BTC address field — Wi-Fi and BTC
price still work, but hashrate stays at 0 with no visible error. Use a
plain receiving address (e.g. a SegWit `bc1q...` address from a wallet's
"Receive" screen), not an xpub — an xpub also leaks the entire wallet's
transaction history/balance and should never be shared.

## Button behavior (GPIO0), for reference when editing the display driver

| Action | Result |
|---|---|
| Single press | Toggle screen (Mining → Network → Clock → ...) |
| Double press | Rotate screen |
| Several quick presses (3+) | Toggle backlight fully off/on |
| Hold 5+ sec | Full settings reset (Wi-Fi + wallet + timezone) — forces the `NerdMinerAP` setup portal again |

This mapping is wired in upstream `NerdMinerV2.ino.cpp`'s `setup()`
(`OneButton` `attachClick`/`attachDoubleClick`/`attachMultiClick`/
`attachLongPressStart`), not in our custom files — `minerMaker154DisplayDriver.cpp`
only implements what each of those calls into
(`alternateScreenState`/`alternateScreenRotation`/the cyclic-screen array).
Settings persist in SPIFFS across power cycles.

One line in that upstream `setup()` block *is* board-specific for us:
`#ifdef MINERMAKER154 button1.setClickMs(700); #endif` right after
`button1.setPressMs(...)`. Confirmed on real hardware: OneButton's default
400ms multi-click window (`_click_ms` in `OneButton.cpp`, finalizes a click
sequence as click/doubleClick/multiClick once no new press arrives within
that window since the last release) is too tight to reliably land 3+ rapid
clicks by hand, so this widens it to 700ms for this board only. Trade-off:
a lone single-click also now takes up to 700ms (not 400ms) to fire, since
the FSM can't yet know it won't be followed by another click.
