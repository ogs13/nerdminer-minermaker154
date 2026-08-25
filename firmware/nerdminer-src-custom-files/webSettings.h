#ifndef _MM_WEBSETTINGS_H_
#define _MM_WEBSETTINGS_H_

// Fixed credentials for the settings page - not configurable, same trust
// tier as the setup AP's own hardcoded password (MineYourCoins). Exposed
// here (not just in webSettings.cpp) so the display driver can show them
// on-screen without duplicating the literal strings.
#define MM_WEB_USER "admin"
#define MM_WEB_PASS "MinerMaker154"
#define MM_WEB_HOST "minermaker.local"
#define MM_WEB_PORT 8080
#define MM_WEB_PORT_S "8080" // string form of MM_WEB_PORT, for literal concatenation

// Small always-on settings web page for the MinerMaker154 board, reachable
// over Wi-Fi once the device has joined the home network (no full
// Wi-Fi/wallet reset required to change the pool/BTC address/timezone).
// Registers its routes lazily, the first time WiFi is seen connected.
void minerMaker154_WebSettingsInit(void);

// Call periodically (drives WebServer::handleClient()).
void minerMaker154_WebSettingsLoop(void);

#endif // _MM_WEBSETTINGS_H_
