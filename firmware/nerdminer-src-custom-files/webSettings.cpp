#include "webSettings.h"

#ifdef MINERMAKER154

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "drivers/storage/storage.h"
#include "drivers/storage/nvMemory.h"

// Defined in wManager.cpp
extern TSettings Settings;
extern nvMemory nvMem;

// Separate port from the WiFiManager captive portal (80), which is only
// alive during initial setup anyway - avoids any doubt about clashing.
static WebServer mmServer(8080);
static bool mmServerStarted = false;

// Fixed credentials for the settings page - not configurable, same trust
// tier as the setup AP's own hardcoded password (MineYourCoins). Good
// enough to keep it off casual snooping on the home LAN, nothing more.
#define MM_WEB_USER "admin"
#define MM_WEB_PASS "MinerMaker154"

static bool mmRequireAuth() {
  if (mmServer.authenticate(MM_WEB_USER, MM_WEB_PASS)) return true;
  mmServer.requestAuthentication();
  return false;
}

static String mmEsc(const String &in) {
  String out = in;
  out.replace("&", "&amp;");
  out.replace("\"", "&quot;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  return out;
}

static void mmHandleRoot() {
  if (!mmRequireAuth()) return;
  String html;
  html.reserve(2200);
  html += "<!doctype html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>";
  html += "<title>MinerMaker154 settings</title><style>";
  html += "body{font-family:sans-serif;max-width:480px;margin:24px auto;padding:0 12px;background:#111;color:#eee}";
  html += "h1{font-size:1.2em}label{display:block;margin-top:14px;font-size:.9em;color:#aaa}";
  html += "input{width:100%;box-sizing:border-box;padding:8px;margin-top:4px;background:#222;color:#eee;border:1px solid #444;border-radius:4px}";
  html += "button{margin-top:20px;padding:10px 18px;background:#FEA0;color:#111;border:0;border-radius:4px;font-weight:bold}";
  html += "</style></head><body>";
  html += "<h1>MinerMaker154 settings</h1>";
  html += "<p style='color:#aaa'>Changes are saved to flash and the device restarts to apply them.</p>";
  html += "<form method='POST' action='/save'>";
  html += "<label>BTC receiving address (not xpub/ypub/zpub!)</label>";
  html += "<input name='btc' value='" + mmEsc(Settings.BtcWallet) + "' maxlength='79'>";
  html += "<label>Pool URL</label>";
  html += "<input name='pool' value='" + mmEsc(Settings.PoolAddress) + "' maxlength='79'>";
  html += "<label>Pool port</label>";
  html += "<input name='port' type='number' value='" + String(Settings.PoolPort) + "'>";
  html += "<label>Pool password (optional)</label>";
  html += "<input name='poolpass' value='" + mmEsc(Settings.PoolPassword) + "' maxlength='79'>";
  html += "<label>Timezone offset from UTC (-12..+12)</label>";
  html += "<input name='tz' type='number' min='-12' max='12' value='" + String(Settings.Timezone) + "'>";
  html += "<button type='submit'>Save &amp; restart</button>";
  html += "</form></body></html>";
  mmServer.send(200, "text/html", html);
}

static void mmHandleSave() {
  if (!mmRequireAuth()) return;
  if (mmServer.hasArg("btc")) {
    strncpy(Settings.BtcWallet, mmServer.arg("btc").c_str(), sizeof(Settings.BtcWallet) - 1);
    Settings.BtcWallet[sizeof(Settings.BtcWallet) - 1] = 0;
  }
  if (mmServer.hasArg("pool")) {
    Settings.PoolAddress = mmServer.arg("pool");
  }
  if (mmServer.hasArg("port")) {
    Settings.PoolPort = mmServer.arg("port").toInt();
  }
  if (mmServer.hasArg("poolpass")) {
    strncpy(Settings.PoolPassword, mmServer.arg("poolpass").c_str(), sizeof(Settings.PoolPassword) - 1);
    Settings.PoolPassword[sizeof(Settings.PoolPassword) - 1] = 0;
  }
  if (mmServer.hasArg("tz")) {
    Settings.Timezone = mmServer.arg("tz").toInt();
  }

  nvMem.saveConfig(&Settings);

  mmServer.send(200, "text/html",
                "<!doctype html><html><body style='font-family:sans-serif;background:#111;color:#eee'>"
                "<h1>Saved</h1><p>Restarting the miner now&hellip;</p></body></html>");
  mmServer.client().flush();
  delay(1500);
  ESP.restart();
}

void minerMaker154_WebSettingsInit(void) {
  // Routes can be registered before WiFi connects; the server itself is
  // only started once we're actually online (see WebSettingsLoop).
  mmServer.on("/", HTTP_GET, mmHandleRoot);
  mmServer.on("/save", HTTP_POST, mmHandleSave);
}

void minerMaker154_WebSettingsLoop(void) {
  if (!mmServerStarted) {
    if (WiFi.status() != WL_CONNECTED) return;
    mmServer.begin();
    mmServerStarted = true;

    if (MDNS.begin("minermaker")) {
      MDNS.addService("http", "tcp", 8080);
      Serial.println("MinerMaker154: settings page at http://minermaker.local:8080/");
    } else {
      Serial.println("MinerMaker154: mDNS init failed, use the IP address instead");
    }
    Serial.print("MinerMaker154: settings page at http://");
    Serial.print(WiFi.localIP());
    Serial.println(":8080/");
  }
  mmServer.handleClient();
}

#endif // MINERMAKER154
