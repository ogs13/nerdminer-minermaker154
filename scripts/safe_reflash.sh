#!/usr/bin/env bash
#
# safe_reflash.sh - update MinerMaker154 firmware WITHOUT touching
# Wi-Fi credentials, wallet/pool/timezone settings, or saved mining stats.
#
# How: it writes ONLY the app image (MinerMaker154_firmware.bin) at flash
# offset 0x10000. It never runs erase-flash, and never touches 0x0 (the
# bootloader) or 0x8000 (the partition table). Wi-Fi credentials (ESP-IDF's
# own NVS partition) and this project's own settings/stats (SPIFFS + a
# separate NVS namespace, both defined by the huge_app.csv partition table)
# live in other flash regions and are left alone.
#
# By default it also takes a full 8MB flash backup first, so a bad flash
# can always be undone with:
#   esptool --port <port> write-flash 0x0 <backup file>
#
# Usage:
#   ./scripts/safe_reflash.sh [-p PORT] [-f FIRMWARE_BIN] [--no-backup] [-y]
#
# Options:
#   -p, --port PORT       Serial port (default: auto-detect)
#   -f, --firmware FILE   App image to flash (default: firmware/MinerMaker154_firmware.bin)
#       --no-backup       Skip the full-flash backup (faster, less safe)
#   -y, --yes             Don't ask for confirmation before writing
#   -h, --help            Show this help
#
# Examples:
#   ./scripts/safe_reflash.sh                       # auto-detect, ask before writing
#   ./scripts/safe_reflash.sh -p /dev/ttyACM0 -y     # explicit port, no prompt

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"

PORT=""
FW_BIN="$REPO_DIR/firmware/MinerMaker154_firmware.bin"
DO_BACKUP=1
ASSUME_YES=0
APP_OFFSET=0x10000
FLASH_SIZE_BYTES=0x800000   # 8MB - N8R8 module, see README "Hardware"

usage() {
  sed -n '2,/^set -e/p' "$0" | sed -e '$d' -e 's/^#//' -e 's/^ //'
  exit 1
}

while [ $# -gt 0 ]; do
  case "$1" in
    -p|--port) PORT="$2"; shift 2 ;;
    -f|--firmware) FW_BIN="$2"; shift 2 ;;
    --no-backup) DO_BACKUP=0; shift ;;
    -y|--yes) ASSUME_YES=1; shift ;;
    -h|--help) usage ;;
    *) echo "Unknown option: $1" >&2; usage ;;
  esac
done

# ---- 1. locate esptool -----------------------------------------------------
ESPTOOL=""
for cand in esptool esptool.py; do
  if command -v "$cand" >/dev/null 2>&1; then ESPTOOL="$cand"; break; fi
done
if [ -z "$ESPTOOL" ]; then
  echo "esptool not found - installing it for this user..."
  if command -v uv >/dev/null 2>&1; then
    uv tool install esptool >/dev/null
    ESPTOOL="$HOME/.local/bin/esptool"
  elif command -v pip3 >/dev/null 2>&1; then
    pip3 install --user esptool
    ESPTOOL="esptool.py"
  else
    echo "Neither uv nor pip3 found. Install esptool manually: pip install esptool" >&2
    exit 1
  fi
fi
echo "Using esptool: $ESPTOOL"

# ---- 2. find the firmware image --------------------------------------------
if [ ! -f "$FW_BIN" ]; then
  echo "Firmware image not found: $FW_BIN" >&2
  exit 1
fi
echo "Firmware image: $FW_BIN ($(wc -c < "$FW_BIN") bytes)"

# ---- 3. find the serial port ------------------------------------------------
if [ -z "$PORT" ]; then
  for cand in /dev/ttyACM* /dev/ttyUSB* /dev/cu.usbmodem* /dev/cu.SLAB_USBtoUART*; do
    if [ -e "$cand" ]; then PORT="$cand"; break; fi
  done
fi
if [ -z "$PORT" ]; then
  echo "Could not auto-detect the device's serial port (checked /dev/ttyACM*," >&2
  echo "/dev/ttyUSB*, /dev/cu.usbmodem*, /dev/cu.SLAB_USBtoUART*)." >&2
  echo "Plug the device in and/or pass the port explicitly: $0 -p /dev/ttyACM0" >&2
  exit 1
fi
echo "Using port: $PORT"

if [ ! -w "$PORT" ]; then
  echo "No write access to $PORT." >&2
  echo "On Linux: sudo usermod -aG uucp \$USER, then log out and back in." >&2
  exit 1
fi

# ---- 4. confirm --------------------------------------------------------------
echo
echo "About to write ONLY the app partition (offset $APP_OFFSET) from:"
echo "  $FW_BIN"
echo "to the device on $PORT."
echo "Bootloader, partition table, Wi-Fi credentials, and this project's"
echo "settings/stats partitions will NOT be touched."
echo
if [ "$ASSUME_YES" -ne 1 ]; then
  read -r -p "Proceed? [y/N] " REPLY
  case "$REPLY" in
    [yY]|[yY][eE][sS]) ;;
    *) echo "Aborted."; exit 1 ;;
  esac
fi

# ---- 5. backup ---------------------------------------------------------------
if [ "$DO_BACKUP" -eq 1 ]; then
  BACKUP_DIR="$REPO_DIR/backups"
  mkdir -p "$BACKUP_DIR"
  BACKUP_FILE="$BACKUP_DIR/full-flash-backup-$(date +%Y%m%d-%H%M%S).bin"
  echo "Backing up the full 8MB flash to: $BACKUP_FILE"
  echo "(takes a minute or two over serial - do not disconnect the device)"
  "$ESPTOOL" --port "$PORT" read-flash 0x0 "$FLASH_SIZE_BYTES" "$BACKUP_FILE"
  echo "Backup done. To restore it if anything goes wrong:"
  echo "  $ESPTOOL --port $PORT write-flash 0x0 $BACKUP_FILE"
else
  echo "Skipping backup (--no-backup given)."
fi

# ---- 6. write only the app partition -----------------------------------------
echo
echo "Writing app partition..."
"$ESPTOOL" --port "$PORT" --baud 460800 write-flash "$APP_OFFSET" "$FW_BIN"

# ---- 7. verify -----------------------------------------------------------------
echo
echo "Verifying..."
"$ESPTOOL" --port "$PORT" verify-flash "$APP_OFFSET" "$FW_BIN"

echo
echo "Done. Wi-Fi credentials, wallet/pool/timezone settings, and saved"
echo "mining stats were not touched. The device should reboot into the new"
echo "firmware automatically."
echo "Settings page (once back on Wi-Fi): http://minermaker.local:8080/ (admin / MinerMaker154)"
