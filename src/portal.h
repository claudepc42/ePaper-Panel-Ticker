#pragma once
#include "types.h"
#include "nvs_manager.h"
#include "wifi_manager.h"
#include <WebServer.h>
#include <DNSServer.h>

// ── CaptivePortal ─────────────────────────────────────────────────────────────
// Runs the phone-facing setup UI (PRD §8).
// Uses the synchronous ESP32 WebServer (no AsyncWebServer dependency) so that
// the WiFi test handler can block inline without task-management complexity.
//
// Routes:
//   GET  /               → portal HTML SPA (portal_html.h)
//   GET  /config         → current NVS settings as JSON (pre-populates fields on reconfigure)
//   GET  /scan           → WiFi network scan results as JSON array
//   POST /wifi-test      → {ssid, pass} → {ok, error} — tests credentials live (PRD §8.5)
//   POST /api-test       → {ssid, pass, provider, apiKey} → {ok, error} — probes provider with a test fetch
//   POST /save           → all settings as JSON → write NVS + restart
//   GET  /hotspot-detect.html, /generate_204, /ncsi.txt, * → redirect to /

class CaptivePortal {
public:
    CaptivePortal(NVSManager& nvs, WiFiManager& wifi);

    // Starts DNS + HTTP server. Call once after WiFiManager::startAP().
    void begin();

    // Call from loop() to service pending requests.
    void loop();

    // Returns true after a successful /save has been committed and device is
    // about to restart (main.cpp uses this to stop the loop cleanly).
    bool didSave() const { return _saved; }

private:
    NVSManager&  _nvs;
    WiFiManager& _wifi;
    WebServer    _server;
    DNSServer    _dns;
    bool         _saved;

    void _handleRoot();
    void _handleConfig();
    void _handleScan();
    void _handleWifiTest();
    void _handleApiTest();
    void _handleSave();
    void _handleRedirect();
};
