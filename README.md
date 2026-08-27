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
7. **Restyled the UI and added features** (see "Custom UI & features" below):
   a themed layout closer to the original vendor look, a flicker fix, two
   extra screens, a QR code for the setup Wi-Fi, a true backlight-off with
   periodic auto-wake, and a settings web page.

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

## Custom UI & features

Beyond the base mining/clock screens, the `MinerMaker154` display driver adds:

- **Themed layout** closer to the original vendor firmware's look (dark
  background, yellow/blue accent, a digital-style font for the big numbers)
  instead of a plain text dump — see `minerMaker154DisplayDriver.cpp` for
  the implementation. It's a stylistic approximation, not a pixel-for-pixel
  clone of the original branded firmware (which, as noted above, was never
  published anywhere to clone from).
- **Flicker fix:** every screen now draws into an off-screen sprite
  (`TFT_eSprite`) and is blitted to the panel in one shot, instead of
  calling `fillScreen()` directly on the panel every second — the visible
  black flash on every data refresh is gone.
- **Three extra screens**, cycled with a single press same as before: a
  **Network** screen (block height, global hashrate, difficulty, halving
  progress, network fee) and a **Wi-Fi/Web** screen (network SSID, IP
  address, signal strength, and the settings-page URL/credentials),
  alongside the existing Mining and Clock screens. Network data comes from
  `getCoinData()` in `monitor.cpp`; this board doesn't define
  `NERDMINER_T_HMI`, so only the single `halfHourFee` field is available
  (not the fastest/economy/minimum fee tiers some other boards show).
- **QR code on the setup screen:** scan it with a phone camera to join the
  device's own `NerdMinerAP` Wi-Fi directly, instead of typing the SSID/password.
- **True backlight-off:** the existing "several quick presses" gesture now
  reliably cuts power to the backlight (not just a dark screen with the
  light still on).
- **Auto screen-on:** while the backlight is off, it now automatically
  turns on by itself every 30 minutes, shows the Clock screen for 45
  seconds then the main Mining screen for 15 seconds, then turns back
  off — no button press needed to check on it occasionally. Press the
  button as usual to turn it on/off manually at any time.
- **Settings web page**, reachable at `http://minermaker.local:8080/`
  (mDNS — works on macOS/Linux out of the box; on Windows install [Bonjour](https://support.apple.com/en-us/106380)
  or use the IP instead) or `http://<device-ip>:8080/` once the device has
  joined your Wi-Fi — lets you change the BTC address, pool URL/port/
  password, and timezone **without** doing a full Wi-Fi/wallet reset
  (which would otherwise require redoing the whole `NerdMinerAP` portal).
  Wi-Fi credentials themselves still go through the normal setup portal.
  Protected with HTTP Basic Auth (user `admin`, password `MinerMaker154` —
  fixed, not configurable from the page itself; same trust tier as the
  setup AP's own hardcoded password, i.e. enough to stop casual access on
  your home LAN, not a real secret).
- **Smooth backlight fade** instead of an instant on/off cut (PWM via the
  ESP32's LEDC peripheral), for both the manual toggle and the auto-wake.
- **Backlight state survives power cycles** — if you turn the screen off
  and then power-cycle the device, it comes back up with the screen still
  off (instead of always waking up lit). Stored in its own NVS namespace,
  separate from the wallet/pool settings.
- **Full-screen Clock**: just a big time display (BTC price as a small
  line above it, hashrate/shares as a small line below) instead of
  competing with a stats grid for space - much more readable at a glance.
- **Digital-display styling throughout**: every stat box value (not just
  the hero hashrate/clock) now uses the same 7-segment-style font as the
  reference vendor look, closer to the original photo.
- **Overflow-safe layout**: every stat box clips its own text instead of
  letting a long label/value bleed into its neighbour or off the right
  edge of the panel, and the big "hero" numbers (clock, hashrate, block
  height) auto-shrink their font size to fit whatever the actual value
  turns out to be.
- **Wi-Fi signal bars** in the header (from `WiFi.RSSI()`) instead of a
  plain connected/not-connected dot.

## Directory structure

```
esp32_miner/
├── README.md                      — this file
├── LICENSE                        — MIT (inherited from NerdMiner_v2)
├── docs/
│   ├── interface.txt              — display pinout from the seller
│   ├── product_parametr.avif      — product specs
│   └── user_maual.pdf             — seller's user manual
├── scripts/
│   └── safe_reflash.sh            — update the firmware without erasing Wi-Fi/wallet/stats
└── firmware/
    ├── MinerMaker154_factory.bin  — ready-to-flash image (bootloader+partitions+app, offset 0x0)
    ├── MinerMaker154_firmware.bin — app only (for updates, offset 0x10000)
    └── nerdminer-src-custom-files/— custom files added to NerdMiner_v2 (see below)
```

## Compatibility & testing status

Both the base mining firmware and the UI described above have been
**flashed to and run on a real physical unit** (mine) — not just
compiled. The base firmware ran 12h+ continuously before the UI was added;
the UI went through several rounds of hardware testing and photo-driven
fixes (font rendering, layout, backlight control) via
`scripts/safe_reflash.sh`, each time confirmed booting and mining normally
afterward with the pre-existing wallet/pool/Wi-Fi settings and accumulated
mining stats untouched.

It should work as-is on any board matching the hardware description above
(same "ESP32S3 1.54 TFT LCD V1.0" / ZJYUNJIE board, same ESP32-S3-WROOM-1
**N8R8** chip variant, same pinout) — nothing in the firmware is
unit-specific, and the pinout was verified against the seller's own
`interface.txt`, not guessed. But since this is a generic board sourced
from AliExpress, there could in principle be undocumented hardware
revisions (different flash/PSRAM size, a shifted pin) this hasn't been
tested against.

**A note on `esptool` and this board:** its native USB-Serial/JTAG mode
can stall on a large continuous `read-flash` (a quirk of the chip/tool
combination, not a bad cable) — `scripts/safe_reflash.sh` already works
around this by reading its pre-update backup in small chunks; `write-flash`
itself is unaffected.

**If you flash this and it works (or doesn't), please open an issue** —
that feedback is exactly what turns "works on my unit" into "confirmed
working for this board."

## How to reflash (if needed)

### Updating without losing your Wi-Fi/wallet settings or mining stats

To install a newer build of this firmware on a device that's already set
up, use `scripts/safe_reflash.sh` instead of the full erase below — it
writes **only** the app partition (`MinerMaker154_firmware.bin` at
`0x10000`), never touches the bootloader/partition table, and never runs
`erase-flash` — so your Wi-Fi credentials, wallet/pool/timezone settings,
and saved mining stats (all in other flash partitions) are left alone. It
also takes a full flash backup first by default, so there's always a way
back if something goes wrong:

```bash
./scripts/safe_reflash.sh              # auto-detects the port, asks before writing
```

Run `./scripts/safe_reflash.sh --help` for options (explicit port,
skipping the backup, non-interactive mode). Requires `esptool` (installed
automatically via `uv`/`pip` if missing) and access to the device's serial
port (see below).

### First-time flash / full reset

For a brand new device (or to intentionally wipe everything and start the
setup portal from scratch), erase the whole flash first. **This deletes
Wi-Fi/wallet settings and mining stats too.**

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

After flashing: connect over Wi-Fi to the **`NerdMinerAP`** access point
(password **`MineYourCoins`** — NerdMiner_v2's built-in default; scan the
QR code shown on screen to join without typing it), open **`192.168.4.1`**
in a browser, select your home network, enter its password, and enter
**your own BTC receiving address** (not an xpub!).

Afterwards, the BTC address/pool/timezone can be changed anytime from
`http://minermaker.local:8080/` (user `admin` / password `MinerMaker154`,
see "Custom UI & features" above) without repeating this whole portal flow.

### How to rebuild from source (if you need to change something in the firmware)

1. Clone `git clone https://github.com/BitMaker-hub/NerdMiner_v2.git`
2. Overlay the files from `firmware/nerdminer-src-custom-files/`:
   - `platformio.ini` → repo root (adds `[env:MinerMaker154]`; or manually
     merge just the `[env:MinerMaker154]` section into the current file)
   - `Setup999_MinerMaker154.h` → `lib/TFT_eSPI/User_Setups/`
   - `minerMaker154.h` → `src/drivers/devices/`
   - `minerMaker154DisplayDriver.cpp` → `src/drivers/displays/`
   - `webSettings.h`, `webSettings.cpp` → `src/`
   - Add to `src/drivers/devices/device.h`:
     `#elif defined(MINERMAKER154) #include "minerMaker154.h"`
   - Add to `src/drivers/displays/displayDriver.h`:
     `extern DisplayDriver minerMaker154DisplayDriver;`
   - Add to `src/drivers/displays/display.cpp`:
     `#ifdef MINERMAKER_DISPLAY` → `currentDisplayDriver = &minerMaker154DisplayDriver;`
   - Add to `lib/TFT_eSPI/User_Setup_Select.h`:
     `#ifdef MINERMAKER154` → `#include <User_Setups/Setup999_MinerMaker154.h>`
   - Add to `src/NerdMinerV2.ino.cpp`, right after `button1.setPressMs(5*SECOND_MS);`
     in the one-button `setup()` block:
     `#ifdef MINERMAKER154 button1.setClickMs(700); #endif`
     (widens the multi-click window - see "Button (GPIO0)" below)
3. `pio run -e MinerMaker154` (PlatformIO) → output in `firmware/dev/`

## Button (GPIO0)

| Action | Result |
|---|---|
| Single press | Toggle screen (Mining → Network → Clock → Wi-Fi/Web → ...) |
| Double press | Rotate screen orientation 90° |
| Several quick presses (3+) | Toggle backlight fully on/off |
| Hold 5+ sec | **Full reset** of settings (Wi-Fi + wallet + timezone) — requires going through the whole `NerdMinerAP` portal again |

3+ clicks only register if every gap between clicks is under the click
window (`button1.setClickMs()` in `NerdMinerV2.ino.cpp`, upstream default
400ms). On this board that's widened to **700ms** since 400ms proved too
tight to land reliably by hand — the trade-off is a single click also
takes up to 700ms (was 400ms) to register, since the button can't tell
it's "just one click" until that window passes without a follow-up press.

While the backlight is off, the screen automatically wakes itself up for
1 minute every 30 minutes — 45 seconds on the Clock screen, then 15
seconds on the main Mining screen — then turns back off. This doesn't
count as a manual toggle and doesn't reset the timer.

Settings are stored in SPIFFS (flash memory), so simply power-cycling the
device does **not** reset them.

## Possible next steps (not done, optional)

- Confirm the setup-screen QR code actually scans well on a real phone
  camera (only exercised on a fresh Wi-Fi setup, not an in-place update).
- Tune the auto-wake interval/duration (`AUTO_WAKE_INTERVAL_MS` /
  `AUTO_WAKE_DURATION_MS` in `minerMaker154DisplayDriver.cpp`) to taste.
- The settings-page password (`MM_WEB_USER`/`MM_WEB_PASS` in
  `webSettings.cpp`) is a fixed constant, not editable from the page or the
  setup portal — fine for a home LAN, but if you want it configurable
  you'd add it as a new `TSettings` field alongside `BtcWallet` etc.
