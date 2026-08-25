#include "displayDriver.h"

#ifdef MINERMAKER_DISPLAY

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include "monitor.h"
#include "OpenFontRender.h"
#include "media/myFonts.h"   // DigitalNumbers font (bundled in NerdMiner_v2)
#include "qrcode.h"
#include "webSettings.h"

// Own NVS namespace, separate from the project's own settings/stats
// storage - just persists whether the backlight was on or off, so a power
// cycle resumes in the same state instead of always waking up lit.
static Preferences mmPrefs;

TFT_eSPI mm_tft = TFT_eSPI();
TFT_eSprite mm_bg = TFT_eSprite(&mm_tft); // off-screen frame buffer: draw here, blit once -> no flicker
OpenFontRender mm_render;

#define MM_YELLOW 0xFEA0
#define MM_BLUE   0x0438
#define MM_GREY   0x8410
#define MM_DARK   0x18E3

// ---- backlight / screen-off state --------------------------------------
static bool backlightOn = true;
static bool autoWaking = false;
static unsigned long lastOffMillis = 0;
static unsigned long autoWakeStart = 0;
static unsigned long lastAnimateDrawSec = 0;

#define AUTO_WAKE_INTERVAL_MS (15UL * 60UL * 1000UL) // wake every 15 min...
#define AUTO_WAKE_DURATION_MS (60UL * 1000UL)         // ...and stay on for 1 min

// ---- backlight: PWM-driven, so on/off is a smooth fade rather than a hard cut ----
#define MM_BL_CHANNEL 0
#define MM_BL_FREQ    5000
#define MM_BL_RES     8 // 8-bit duty: 0-255
static int mm_blDuty = 0;

static void mm_fadeBacklightTo(int target) {
  target = constrain(target, 0, 255);
  int step = (target >= mm_blDuty) ? 5 : -5;
  while (mm_blDuty != target) {
    mm_blDuty += step;
    if ((step > 0 && mm_blDuty > target) || (step < 0 && mm_blDuty < target)) mm_blDuty = target;
    ledcWrite(MM_BL_CHANNEL, mm_blDuty);
    delay(8);
  }
}

static void mm_setBacklight(bool on) {
  backlightOn = on;
  mm_fadeBacklightTo(on ? 255 : 0);
}

// ---- small drawing helpers ----------------------------------------------

// bars: 0-3 signal strength (from RSSI), or -1 for "not connected"
static void mm_wifiIcon(int x, int y, int bars) {
  static const int barH[3] = {4, 7, 10};
  static const int barY[3] = {6, 3, 0};
  for (int i = 0; i < 3; i++) {
    uint16_t c = (bars > i) ? TFT_WHITE : MM_DARK;
    mm_bg.fillRect(x + i * 5, y + barY[i], 3, barH[i], c);
  }
}

static int mm_wifiBars() {
  if (WiFi.status() != WL_CONNECTED) return -1;
  int rssi = WiFi.RSSI();
  if (rssi > -60) return 3;
  if (rssi > -70) return 2;
  return 1;
}

static void mm_header(const char *title, const String &time) {
  mm_bg.setFreeFont(&FreeSansBold9pt7b);
  mm_bg.setTextColor(MM_YELLOW, TFT_BLACK);
  mm_bg.setTextDatum(TL_DATUM);
  mm_bg.drawString(title, 4, 2);

  mm_bg.setFreeFont(&FreeSans9pt7b);
  mm_bg.setTextColor(TFT_WHITE, TFT_BLACK);
  mm_bg.setTextDatum(TR_DATUM);
  mm_bg.drawString(time, 236, 4);

  uint16_t wifiW = mm_bg.textWidth(time);
  mm_wifiIcon(236 - wifiW - 20, 5, mm_wifiBars());
}

// Draws `value` centered on (cx, topY) using the digital font, returns its height
static void mm_bigNumber(const String &value, int cx, int topY, int fontSize, uint16_t fg, uint16_t bg) {
  mm_render.setFontSize(fontSize);
  uint32_t w = mm_render.getTextWidth(value.c_str());
  mm_render.drawString(value.c_str(), cx - (int)w / 2, topY, fg, bg);
}

static void mm_statCell(int x, int y, int w, int h, const char *label, const String &value) {
  mm_bg.drawRect(x, y, w, h, MM_DARK);
  mm_bg.setFreeFont(&FreeSans9pt7b);
  mm_bg.setTextColor(MM_GREY, TFT_BLACK);
  mm_bg.setTextDatum(TL_DATUM);
  mm_bg.drawString(label, x + 6, y + 4);

  mm_bg.setFreeFont(&FreeSansBold9pt7b);
  mm_bg.setTextColor(TFT_WHITE, TFT_BLACK);
  mm_bg.setTextDatum(BL_DATUM);
  mm_bg.drawString(value, x + 6, y + h - 6);
}

// ---------------------------------------------------------------------------

void minerMaker154_Init(void) {
  Serial.println("MinerMaker 1.54 (240x240 ST7789 SPI) display driver initialized");

  // TFT_eSPI's own init() does `pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, ...)`
  // internally (see TFT_eSPI.cpp, guarded by `#if defined(TFT_BL) && defined(TFT_BACKLIGHT_ON)`)
  // which would silently detach our LEDC/PWM binding on that pin if we set
  // it up first. So: tft.init() FIRST, then claim the backlight pin for PWM.
  mm_tft.init();
  mm_tft.setRotation(0);
  mm_tft.setSwapBytes(true);
  mm_tft.fillScreen(TFT_BLACK);

  ledcSetup(MM_BL_CHANNEL, MM_BL_FREQ, MM_BL_RES);
  ledcAttachPin(TFT_BL, MM_BL_CHANNEL);
  mm_blDuty = 0;

  // Resume whatever backlight state was last explicitly set (survives
  // power cycles), instead of always waking up lit.
  mmPrefs.begin("mm154bl", false);
  bool savedOn = mmPrefs.getBool("on", true);
  mm_setBacklight(savedOn);
  if (!savedOn) lastOffMillis = millis();

  mm_bg.setColorDepth(16);
  mm_bg.createSprite(240, 240);
  mm_bg.setSwapBytes(true);
  mm_bg.fillSprite(TFT_BLACK);

  mm_render.setDrawer(mm_bg);
  mm_render.setLineSpaceRatio(0.9);
  if (mm_render.loadFont(DigitalNumbers, sizeof(DigitalNumbers))) {
    Serial.println("MinerMaker154: digital font failed to load");
  }

  minerMaker154_WebSettingsInit();
}

void minerMaker154_AlternateScreenState(void) {
  Serial.println("MinerMaker154: toggling backlight");
  mm_setBacklight(!backlightOn);
  mmPrefs.putBool("on", backlightOn); // persist across power cycles
  autoWaking = false;
  if (!backlightOn) {
    lastOffMillis = millis();
  }
}

void minerMaker154_AlternateRotation(void) {
  mm_tft.setRotation((mm_tft.getRotation() + 1) % 4);
}

void minerMaker154_MinerScreen(unsigned long mElapsed) {
  mining_data data = getMiningData(mElapsed);

  mm_bg.fillSprite(TFT_BLACK);
  mm_header("MINERMAKER154", data.currentTime);

  mm_bg.setFreeFont(&FreeSans9pt7b);
  mm_bg.setTextColor(MM_GREY, TFT_BLACK);
  mm_bg.setTextDatum(TL_DATUM);
  mm_bg.drawString("Uptime " + data.timeMining, 4, 20);

  // Blue hero box: valid blocks (left) + hashrate (right)
  mm_bg.fillRect(4, 38, 232, 52, MM_BLUE);
  mm_bg.drawFastVLine(72, 38, 52, TFT_BLACK);
  mm_bg.setFreeFont(&FreeSansBold9pt7b);
  mm_bg.setTextColor(TFT_WHITE, MM_BLUE);
  mm_bg.setTextDatum(TC_DATUM);
  mm_bg.drawString("BLOCKS", 38, 44);
  mm_bigNumber(data.valids, 38, 60, 18, TFT_WHITE, MM_BLUE);

  mm_bigNumber(data.currentHashRate, 160, 46, 26, TFT_WHITE, MM_BLUE);
  mm_bg.setFreeFont(&FreeSans9pt7b);
  mm_bg.setTextColor(TFT_WHITE, MM_BLUE);
  mm_bg.setTextDatum(TR_DATUM);
  mm_bg.drawString("KH/s", 230, 78);

  // 2x2 stat grid
  mm_statCell(4,   96, 114, 64, "TEMPLATES", data.templates);
  mm_statCell(122, 96, 114, 64, "BEST DIFF", data.bestDiff);
  mm_statCell(4,   164, 114, 64, "SHARES", data.completedShares);
  mm_statCell(122, 164, 114, 64, "MHASHES", data.totalMHashes);

  mm_bg.pushSprite(0, 0);

  Serial.printf(">>> Completed %s share(s), %s Khashes, avg. hashrate %s KH/s\n",
                data.completedShares.c_str(), data.totalKHashes.c_str(), data.currentHashRate.c_str());
}

void minerMaker154_ClockScreen(unsigned long mElapsed) {
  clock_data data = getClockData(mElapsed);

  mm_bg.fillSprite(TFT_BLACK);
  mm_header("CLOCK", data.currentTime);

  mm_bigNumber(data.currentTime, 120, 70, 34, MM_YELLOW, TFT_BLACK);

  mm_statCell(4,   150, 232, 40, "BTC PRICE", data.btcPrice);
  mm_statCell(4,   194, 114, 40, "HASHRATE (KH/s)", data.currentHashRate);
  mm_statCell(122, 194, 114, 40, "SHARES", data.completedShares);

  mm_bg.pushSprite(0, 0);
}

void minerMaker154_WifiInfoScreen(unsigned long mElapsed) {
  clock_data data = getClockData(mElapsed); // only used for the header clock

  mm_bg.fillSprite(TFT_BLACK);
  mm_header("WI-FI / WEB", data.currentTime);

  bool connected = (WiFi.status() == WL_CONNECTED);
  String ssid = connected ? WiFi.SSID() : "(not connected)";
  String ip = connected ? WiFi.localIP().toString() : "-";
  String rssi = connected ? (String(WiFi.RSSI()) + " dBm") : "-";

  mm_statCell(4,   38, 232, 40, "NETWORK (SSID)", ssid);
  mm_statCell(4,   82, 114, 40, "IP ADDRESS", ip);
  mm_statCell(122, 82, 114, 40, "SIGNAL", rssi);

  mm_bg.fillRect(4, 128, 232, 100, MM_BLUE);
  mm_bg.setFreeFont(&FreeSansBold9pt7b);
  mm_bg.setTextColor(TFT_WHITE, MM_BLUE);
  mm_bg.setTextDatum(TL_DATUM);
  mm_bg.drawString("SETTINGS PAGE", 10, 132);

  mm_bg.setFreeFont(&FreeSans9pt7b);
  mm_bg.setTextColor(MM_YELLOW, MM_BLUE);
  mm_bg.drawString("http://" MM_WEB_HOST ":" MM_WEB_PORT_S "/", 10, 154);
  mm_bg.setTextColor(TFT_WHITE, MM_BLUE);
  String ipLine = connected ? (String("or http://") + ip + ":" MM_WEB_PORT_S "/") : String("(connect to Wi-Fi first)");
  mm_bg.drawString(ipLine, 10, 172);

  mm_bg.setTextColor(TFT_WHITE, MM_BLUE);
  mm_bg.drawString(String("user: ") + MM_WEB_USER, 10, 196);
  mm_bg.drawString(String("pass: ") + MM_WEB_PASS, 10, 214);

  mm_bg.pushSprite(0, 0);
}

void minerMaker154_GlobalHashScreen(unsigned long mElapsed) {
  coin_data data = getCoinData(mElapsed);

  mm_bg.fillSprite(TFT_BLACK);
  mm_header("NETWORK", data.currentTime);

  mm_bg.setFreeFont(&FreeSans9pt7b);
  mm_bg.setTextColor(MM_GREY, TFT_BLACK);
  mm_bg.setTextDatum(TC_DATUM);
  mm_bg.drawString("BLOCK HEIGHT", 120, 30);
  mm_bigNumber(data.blockHeight, 120, 44, 22, TFT_WHITE, TFT_BLACK);

  mm_statCell(4,   96, 114, 54, "GLOBAL HASH", data.globalHashRate);
  mm_statCell(122, 96, 114, 54, "DIFFICULTY", data.netwrokDifficulty);
  mm_statCell(4,   154, 114, 54, "FEE (sat/vB)", data.halfHourFee);
  mm_statCell(122, 154, 114, 54, "BLOCKS LEFT", data.remainingBlocks);

  // Halving progress bar
  mm_bg.drawRect(4, 212, 232, 18, MM_DARK);
  int filled = (int)(228.0f * data.progressPercent / 100.0f);
  if (filled > 0) mm_bg.fillRect(6, 214, filled, 14, MM_YELLOW);
  mm_bg.setTextColor(TFT_BLACK, MM_YELLOW);
  mm_bg.setTextDatum(TC_DATUM);
  mm_bg.drawString("Halving " + String(data.progressPercent, 1) + "%", 120, 214);

  mm_bg.pushSprite(0, 0);
}

// Not part of the normal click cycle - only shown during the periodic auto-wake (see animate below)
static void minerMaker154_StatusScreen(void) {
  clock_data data = getClockData(1000);

  mm_bg.fillSprite(TFT_BLACK);
  mm_header("STATUS", data.currentTime);

  mm_bigNumber(data.currentTime, 120, 60, 30, TFT_WHITE, TFT_BLACK);

  mm_statCell(4,   140, 114, 44, "HASHRATE (KH/s)", data.currentHashRate);
  mm_statCell(122, 140, 114, 44, "SHARES", data.completedShares);
  mm_bg.setFreeFont(&FreeSans9pt7b);
  mm_bg.setTextColor(MM_GREY, TFT_BLACK);
  mm_bg.setTextDatum(TC_DATUM);
  mm_bg.drawString("(auto screen-on, sleeping again shortly)", 120, 200);

  mm_bg.pushSprite(0, 0);
}

void minerMaker154_LoadingScreen(void) {
  mm_bg.fillSprite(TFT_BLACK);
  mm_bg.setFreeFont(&FreeSansBold18pt7b);
  mm_bg.setTextColor(MM_YELLOW, TFT_BLACK);
  mm_bg.setTextDatum(MC_DATUM);
  mm_bg.drawString("MINER", 120, 100);
  mm_bg.drawString("MAKER", 120, 140);
  mm_bg.setTextDatum(TL_DATUM);
  mm_bg.pushSprite(0, 0);
}

void minerMaker154_SetupScreen(void) {
  mm_bg.fillSprite(TFT_BLACK);
  mm_bg.setFreeFont(&FreeSansBold9pt7b);
  mm_bg.setTextColor(MM_YELLOW, TFT_BLACK);
  mm_bg.setTextDatum(TC_DATUM);
  mm_bg.drawString("Scan to connect", 120, 8);

  // QR code: WiFi join for the device's own setup access point.
  // NerdMinerAP / MineYourCoins are NerdMiner_v2's built-in defaults (storage.h).
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(4)];
  const char *wifiQrPayload = "WIFI:T:WPA;S:NerdMinerAP;P:MineYourCoins;;";
  qrcode_initText(&qrcode, qrcodeData, 4, ECC_LOW, wifiQrPayload);

  int scale = 4;
  int qrPix = qrcode.size * scale;
  int qx = (240 - qrPix) / 2;
  int qy = 30;

  mm_bg.fillRect(qx - 6, qy - 6, qrPix + 12, qrPix + 12, TFT_WHITE);
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        mm_bg.fillRect(qx + x * scale, qy + y * scale, scale, scale, TFT_BLACK);
      }
    }
  }

  int textY = qy + qrPix + 16;
  mm_bg.setFreeFont(&FreeSans9pt7b);
  mm_bg.setTextColor(TFT_WHITE, TFT_BLACK);
  mm_bg.setTextDatum(TC_DATUM);
  mm_bg.drawString("or connect manually:", 120, textY);
  mm_bg.setTextColor(MM_YELLOW, TFT_BLACK);
  mm_bg.drawString("NerdMinerAP", 120, textY + 18);
  mm_bg.setTextColor(TFT_WHITE, TFT_BLACK);
  mm_bg.drawString("pass: MineYourCoins", 120, textY + 38);
  mm_bg.setTextColor(MM_YELLOW, TFT_BLACK);
  mm_bg.drawString("then open 192.168.4.1", 120, textY + 58);

  mm_bg.pushSprite(0, 0);
}

void minerMaker154_DoLedStuff(unsigned long frame) {}

void minerMaker154_AnimateCurrentScreen(unsigned long frame) {
  // Service the settings web server (see webSettings.cpp)
  minerMaker154_WebSettingsLoop();

  if (backlightOn) return;

  unsigned long now = millis();

  if (!autoWaking && (now - lastOffMillis >= AUTO_WAKE_INTERVAL_MS)) {
    autoWaking = true;
    autoWakeStart = now;
    mm_fadeBacklightTo(255); // temporary wake, doesn't flip `backlightOn`
  }

  if (autoWaking) {
    unsigned long nowSec = now / 1000UL;
    if (nowSec != lastAnimateDrawSec) {
      lastAnimateDrawSec = nowSec;
      minerMaker154_StatusScreen();
    }
    if (now - autoWakeStart >= AUTO_WAKE_DURATION_MS) {
      autoWaking = false;
      lastOffMillis = now; // restart the 15-min countdown from now
      mm_fadeBacklightTo(0);
    }
  }
}

CyclicScreenFunction minerMaker154CyclicScreens[] = {
    minerMaker154_MinerScreen,
    minerMaker154_GlobalHashScreen,
    minerMaker154_ClockScreen,
    minerMaker154_WifiInfoScreen,
};

DisplayDriver minerMaker154DisplayDriver = {
    minerMaker154_Init,
    minerMaker154_AlternateScreenState,
    minerMaker154_AlternateRotation,
    minerMaker154_LoadingScreen,
    minerMaker154_SetupScreen,
    minerMaker154CyclicScreens,
    minerMaker154_AnimateCurrentScreen,
    minerMaker154_DoLedStuff,
    SCREENS_ARRAY_SIZE(minerMaker154CyclicScreens),
    0,
    240,
    240,
};

#endif
