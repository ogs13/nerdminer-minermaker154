#include "displayDriver.h"

#ifdef MINERMAKER_DISPLAY

#include <TFT_eSPI.h>
#include "monitor.h"

TFT_eSPI mm_tft = TFT_eSPI();

#define MM_YELLOW 0xFEA0
#define MM_BLUE   0x0438

void minerMaker154_Init(void) {
  Serial.println("MinerMaker 1.54 (240x240 ST7789 SPI) display driver initialized");
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  mm_tft.init();
  mm_tft.setRotation(0);
  mm_tft.fillScreen(TFT_BLACK);
}

void minerMaker154_AlternateScreenState(void) {
  int screen_state = digitalRead(TFT_BL);
  digitalWrite(TFT_BL, !screen_state);
}

void minerMaker154_AlternateRotation(void) {
  mm_tft.setRotation((mm_tft.getRotation() + 1) % 4);
}

static void mm_header(const char *title) {
  mm_tft.fillRect(0, 0, 240, 22, MM_YELLOW);
  mm_tft.setTextColor(TFT_BLACK, MM_YELLOW);
  mm_tft.setTextDatum(TL_DATUM);
  mm_tft.setFreeFont(&FreeSansBold9pt7b);
  mm_tft.drawString(title, 6, 3);
}

static void mm_field(int y, const char *label, const String &value) {
  mm_tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  mm_tft.setFreeFont(&FreeSans9pt7b);
  mm_tft.drawString(label, 6, y);
  mm_tft.setTextColor(TFT_WHITE, TFT_BLACK);
  mm_tft.drawString(value, 130, y);
}

void minerMaker154_MinerScreen(unsigned long mElapsed) {
  mining_data data = getMiningData(mElapsed);

  mm_tft.fillScreen(TFT_BLACK);
  mm_header("MINER MAKER");

  // Big hashrate box
  mm_tft.fillRect(4, 28, 232, 46, MM_BLUE);
  mm_tft.setTextColor(TFT_WHITE, MM_BLUE);
  mm_tft.setTextDatum(MC_DATUM);
  mm_tft.setFreeFont(&FreeSansBold18pt7b);
  String hr = data.currentHashRate + " KH/s";
  mm_tft.drawString(hr, 120, 51);
  mm_tft.setTextDatum(TL_DATUM);

  mm_field(92,  "Valid blocks",  data.valids);
  mm_field(114, "Block templ.",  data.templates);
  mm_field(136, "Best diff.",    data.bestDiff);
  mm_field(158, "32bit shares",  data.completedShares);
  mm_field(180, "Mhashes",       data.totalMHashes);
  mm_field(202, "Uptime",        data.timeMining);

  Serial.printf(">>> Completed %s share(s), %s Khashes, avg. hashrate %s KH/s\n",
                data.completedShares.c_str(), data.totalKHashes.c_str(), data.currentHashRate.c_str());
}

void minerMaker154_ClockScreen(unsigned long mElapsed) {
  clock_data data = getClockData(mElapsed);

  mm_tft.fillScreen(TFT_BLACK);
  mm_header("CLOCK");

  mm_tft.setTextDatum(MC_DATUM);
  mm_tft.setTextColor(TFT_WHITE, TFT_BLACK);
  mm_tft.setFreeFont(&FreeSansBold18pt7b);
  mm_tft.drawString(data.currentTime, 120, 90);
  mm_tft.setTextDatum(TL_DATUM);

  mm_field(140, "BTC price",  data.btcPrice);
  mm_field(162, "Hashrate",   data.currentHashRate + " KH/s");
  mm_field(184, "Shares",     data.completedShares);
}

void minerMaker154_LoadingScreen(void) {
  mm_tft.fillScreen(TFT_BLACK);
  mm_tft.setTextColor(MM_YELLOW, TFT_BLACK);
  mm_tft.setTextDatum(MC_DATUM);
  mm_tft.setFreeFont(&FreeSansBold18pt7b);
  mm_tft.drawString("MINER", 120, 100);
  mm_tft.drawString("MAKER", 120, 140);
  mm_tft.setTextDatum(TL_DATUM);
}

void minerMaker154_SetupScreen(void) {
  mm_tft.fillScreen(TFT_BLACK);
  mm_header("SETUP MODE");
  mm_tft.setTextColor(TFT_WHITE, TFT_BLACK);
  mm_tft.setFreeFont(&FreeSans9pt7b);
  mm_tft.drawString("Connect WiFi to:", 6, 60);
  mm_tft.setTextColor(MM_YELLOW, TFT_BLACK);
  mm_tft.drawString("NerdMinerAP", 6, 90);
  mm_tft.setTextColor(TFT_WHITE, TFT_BLACK);
  mm_tft.drawString("Then open:", 6, 130);
  mm_tft.setTextColor(MM_YELLOW, TFT_BLACK);
  mm_tft.drawString("192.168.4.1", 6, 160);
}

void minerMaker154_DoLedStuff(unsigned long frame) {}
void minerMaker154_AnimateCurrentScreen(unsigned long frame) {}

CyclicScreenFunction minerMaker154CyclicScreens[] = {
    minerMaker154_MinerScreen,
    minerMaker154_ClockScreen,
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
