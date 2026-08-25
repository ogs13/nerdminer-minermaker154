# ESP32 Bitcoin Miner ("MinerMaker") — project documentation

A gifted ESP32-S3 solo-mining device with a screen, bought on **AliExpress**
under the **"MinerMaker"** brand. Its firmware got corrupted (endless
reboot loop), so it had to be diagnosed, the exact board re-identified,
and a custom firmware built from scratch.

## Why this repository, and how it differs from what's already out there

If you're searching for a fix for the same problem — you have a
**"ESP32S3 1.54 TFT LCD V1.0"** board made by **ZJYUNJIE**, bought on
**AliExpress** under the **"MinerMaker"** brand (ESP32-S3, ST7789 1.54"
240×240 display) — and:

- the device keeps rebooting every 2-3 seconds
  (logs show `rst:0x7 TG0WDT_SYS_RST`), or
- the screen stays **black with the backlight on** after flashing the
  official/default NerdMiner_v2 firmware (the stock builds target
  different pins and a different interface type — parallel 8-bit instead
  of this board's SPI),

— then as of this repository's publication, the seller **ZJYUNJIE/"MinerMaker"**
has not published the original firmware, source code, or even the exact
board model anywhere publicly (only a product photo and a pin table in the
manual). This board is also missing from the official "Supported Boards"
list of the NerdMiner_v2 project. In other words, searching GitHub/forums
for "MinerMaker ESP32", "ESP32S3 1.54 TFT LCD V1.0", or "ZJYUNJIE nerdminer"
turns up nothing ready-made at the time of writing.

This repository fills exactly that gap:

- the exact display pinout (verified against the seller's file, not guessed);
- a ready-to-flash binary (`firmware/MinerMaker154_factory.bin`) — flash it
  directly, no build required;
- the complete set of files and step-by-step instructions to build support
  for this board (`MinerMaker154`) on top of a clean NerdMiner_v2 checkout,
  in case you need to change anything.

## Hardware

- **Chip:** ESP32-S3-WROOM-1, module **N8R8** (8MB Flash, 8MB PSRAM), revision v0.2
- **Board:** generic "**ESP32S3 1.54 TFT LCD V1.0**" board, made by **ZJYUNJIE**
  (sold under the **"MinerMaker"** brand on AliExpress — a
  [NerdMiner_v2](https://github.com/BitMaker-hub/NerdMiner_v2) clone with a
  swapped logo; the seller's original firmware/sources were never published)
- **Display:** ST7789, SPI, 240×240, 1.54"
- **USB:** native USB-Serial/JTAG (`/dev/ttyACM0` port on Linux), not a
  separate USB-UART bridge

### Display pinout (from `docs/interface.txt`, provided by the seller)

| Signal | GPIO |
|---|---|
| DC | 42 |
| CS | 41 |
| SCK/CLK | 40 |
| SDA/MOSI | 39 |
| RESET | 38 |
| LCD_BL (backlight) | 21 |
| Button | 0 |

## What was wrong and how it was fixed

1. **Symptom:** the device kept rebooting every 2-3 seconds
   (`rst:0x7 TG0WDT_SYS_RST` — reset by the system watchdog) — the firmware
   hung right after boot due to a corrupted/incomplete flash write.
2. **Permissions:** added user `ogs` to the `uucp` group
   (`sudo usermod -aG uucp ogs`) for access to `/dev/ttyACM0`.
3. **Erased the flash and installed the official NerdMiner_v2 firmware**
   (first for LILYGO T-Display S3, then the default build) — the reboot
   loop stopped, but the screen stayed black with the backlight on: both
   builds target **different pins and a different interface type**
   (parallel 8-bit instead of this board's SPI).
4. **Identified the exact board** from a photo of the board (seller
   "MinerMaker" on AliExpress) and from the `interface.txt` file with the
   exact pinout.
5. **Built a custom firmware** — added a new board/env to the NerdMiner_v2
   project itself (`MinerMaker154`) with the correct SPI pins and a simple
   text-based screen driver (without replicating the original vendor's
   "MINER MAKER" branding, which was never published — the underlying data
   is the same: hashrate, valid blocks, shares, best difficulty, uptime).
6. Flashed and verified — **the screen works, mining runs.**

### A second, separately discovered problem: 0 hashrate on first setup

The user initially entered a **`zpub...`** in the BTC address field — this
is an extended public key (xpub) for the whole wallet, not a receiving
address. The pool (`public-pool.io:3333`) silently rejects this format, so
Wi-Fi and the BTC price still worked, but hashrate stayed at 0. Fixed by
entering a regular SegWit receiving address (`bc1q...`) from BlueWallet
(the "Receive" button), after which mining started working.

⚠️ **An xpub/ypub/zpub is neither an address nor a private key**, but it
exposes the wallet's entire transaction history/balance. Don't enter it
anywhere or share it publicly.

## Directory structure

```
esp32_miner/
├── README.md                      — this file
├── LICENSE                        — MIT (inherited from NerdMiner_v2)
├── docs/
│   ├── interface.txt              — display pinout from the seller
│   ├── product_parametr.avif      — product specs
│   └── user_maual.pdf             — seller's user manual
└── firmware/
    ├── MinerMaker154_factory.bin  — ready-to-flash image (bootloader+partitions+app, offset 0x0)
    ├── MinerMaker154_firmware.bin — app only (for updates, offset 0x10000)
    └── nerdminer-src-custom-files/— custom files added to NerdMiner_v2 (see below)
```

## Compatibility & testing status

The prebuilt binary in `firmware/` has only been flashed and verified on
**one physical unit** (mine, ~12h of continuous mining as of this writing).
It should work as-is on any board that matches the hardware description
above (same "ESP32S3 1.54 TFT LCD V1.0" / ZJYUNJIE board, same
ESP32-S3-WROOM-1 **N8R8** chip variant, same pinout) — the firmware doesn't
do anything unit-specific, and the pinout was verified against the seller's
own `interface.txt`, not guessed. But since this is a generic board sourced
from AliExpress, there could in principle be undocumented hardware
revisions (different flash/PSRAM size, a shifted pin) that this hasn't been
tested against.

**If you flash this on the same board and it works (or doesn't), please
open an issue** — that feedback is exactly what turns "works on my unit"
into "confirmed working for this board."

## How to reflash (if needed)

Requires `esptool` (`pip install esptool`) and a serial port to the device:

- Linux: `/dev/ttyACM0` (add your user to the `uucp` group, or whichever
  group owns serial devices on your distro, for permission to access it)
- macOS: something like `/dev/cu.usbmodemXXXX`
- Windows: a `COMx` port

The board uses native USB-Serial/JTAG, so it should enumerate without
extra USB-UART drivers.

```bash
# full flash erase
esptool --port /dev/ttyACM0 erase-flash

# flash the combined image in one shot
esptool --port /dev/ttyACM0 --baud 460800 write-flash 0x0 firmware/MinerMaker154_factory.bin
```

(replace `/dev/ttyACM0` with your actual port)

After flashing: connect over Wi-Fi to the **`NerdMinerAP`** access point,
open **`192.168.4.1`** in a browser, select your home network, enter its
password, and enter **your own BTC receiving address** (not an xpub!).

### How to rebuild from source (if you need to change something in the firmware)

1. Clone `git clone https://github.com/BitMaker-hub/NerdMiner_v2.git`
2. Overlay the files from `firmware/nerdminer-src-custom-files/`:
   - `platformio.ini` → repo root (adds `[env:MinerMaker154]`; or manually
     merge just the `[env:MinerMaker154]` section into the current file)
   - `Setup999_MinerMaker154.h` → `lib/TFT_eSPI/User_Setups/`
   - `minerMaker154.h` → `src/drivers/devices/`
   - `minerMaker154DisplayDriver.cpp` → `src/drivers/displays/`
   - Add to `src/drivers/devices/device.h`:
     `#elif defined(MINERMAKER154) #include "minerMaker154.h"`
   - Add to `src/drivers/displays/displayDriver.h`:
     `extern DisplayDriver minerMaker154DisplayDriver;`
   - Add to `src/drivers/displays/display.cpp`:
     `#ifdef MINERMAKER_DISPLAY` → `currentDisplayDriver = &minerMaker154DisplayDriver;`
   - Add to `lib/TFT_eSPI/User_Setup_Select.h`:
     `#ifdef MINERMAKER154` → `#include <User_Setups/Setup999_MinerMaker154.h>`
3. `pio run -e MinerMaker154` (PlatformIO) → output in `firmware/dev/`

## Button (GPIO0)

| Action | Result |
|---|---|
| Single press | Toggle screen (mining ↔ clock) |
| Double press | Rotate screen orientation |
| Several quick presses | Toggle backlight on/off |
| Hold 5+ sec | **Full reset** of settings (Wi-Fi + wallet + timezone) — requires going through the whole `NerdMinerAP` portal again |

Settings are stored in SPIFFS (flash memory), so simply power-cycling the
device does **not** reset them.

## Possible next steps (not done, optional)

- Add "Global hash" (network-wide hashrate, difficulty, halving countdown)
  and "Fees" (network fees, block height) screens — the data is already
  available in the firmware (`getCoinData()` / `getMiningFeesData()` in
  `monitor.cpp`), only the display code in
  `minerMaker154DisplayDriver.cpp` is missing.
- Add a separate settings web page (reachable over Wi-Fi without a full
  reset) to change the timezone/address without re-entering the Wi-Fi
  password.
