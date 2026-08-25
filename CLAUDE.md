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

`README.md` (Ukrainian) is the primary, authoritative document — it contains
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

### Custom-files architecture notes

- `minerMaker154.h` is the device definition: it sets `PIN_BUTTON_1` and
  defines `MINERMAKER_DISPLAY`, which gates the display driver file.
- `minerMaker154DisplayDriver.cpp` implements a plain, unbranded
  text/box UI (deliberately not replicating the original vendor's "MINER
  MAKER" branded firmware, which was never published) using `TFT_eSPI`
  directly. It follows NerdMiner_v2's `DisplayDriver` struct contract:
  init/toggle-screen/toggle-rotation/loading-screen/setup-screen/an array
  of cyclic screen-draw functions/animate/LED callback. Mining and clock
  data are pulled from upstream's `monitor.cpp` via `getMiningData()` /
  `getClockData()` — this file only renders, it doesn't compute.
  `getCoinData()` / `getMiningFeesData()` are already available from
  upstream `monitor.cpp` but not yet wired into any screen here (see
  README's "possible next steps").
- `Setup999_MinerMaker154.h` is a `TFT_eSPI` `User_Setup` (ID 999) — pins,
  `ST7789_DRIVER`, BGR order, inversion-on, SPI frequencies. This is
  TFT_eSPI's own config layer; the `[env:MinerMaker154]` build additionally
  ignores nothing special beyond upstream's usual `lib_ignore` list.
- `platformio.ini`'s `[env:MinerMaker154]` mirrors the sibling `NerdminerV2`
  env (same board `esp32-s3-devkitc-1`, `qio_opi` PSRAM, native USB CDC)
  but swaps in `-D MINERMAKER154=1` instead of `-D NERDMINERV2=1`.

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
| Single press | Toggle screen (mining ↔ clock) |
| Double press | Rotate screen |
| Several quick presses | Toggle backlight |
| Hold 5+ sec | Full settings reset (Wi-Fi + wallet + timezone) — forces the `NerdMinerAP` setup portal again |

Settings persist in SPIFFS across power cycles.
