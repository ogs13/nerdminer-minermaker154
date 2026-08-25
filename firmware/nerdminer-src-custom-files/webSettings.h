#ifndef _MM_WEBSETTINGS_H_
#define _MM_WEBSETTINGS_H_

// Small always-on settings web page for the MinerMaker154 board, reachable
// over Wi-Fi once the device has joined the home network (no full
// Wi-Fi/wallet reset required to change the pool/BTC address/timezone).
// Registers its routes lazily, the first time WiFi is seen connected.
void minerMaker154_WebSettingsInit(void);

// Call periodically (drives WebServer::handleClient()).
void minerMaker154_WebSettingsLoop(void);

#endif // _MM_WEBSETTINGS_H_
