#include "portal.h"
#include "portal_html.h"
#include "config.h"
#include "data_provider.h"
#include <ArduinoJson.h>
#include <WiFi.h>

CaptivePortal::CaptivePortal(NVSManager& nvs, WiFiManager& wifi)
    : _nvs(nvs), _wifi(wifi), _server(PORTAL_PORT), _saved(false)
{}

void CaptivePortal::begin() {
    // DNS: resolve everything to the AP IP so any URL opens the portal
    _dns.start(DNS_PORT, "*", WiFi.softAPIP());

    // Routes
    _server.on("/",            [this](){ _handleRoot(); });
    _server.on("/config",      [this](){ _handleConfig(); });
    _server.on("/scan",        [this](){ _handleScan(); });
    _server.on("/wifi-test",   HTTP_POST, [this](){ _handleWifiTest(); });
    _server.on("/api-test",    HTTP_POST, [this](){ _handleApiTest(); });
    _server.on("/save",        HTTP_POST, [this](){ _handleSave(); });

    // Captive portal detection endpoints (Apple, Android, Windows)
    _server.on("/hotspot-detect.html", [this](){ _handleRedirect(); });
    _server.on("/generate_204",        [this](){ _handleRedirect(); });
    _server.on("/ncsi.txt",            [this](){ _handleRedirect(); });
    _server.on("/connecttest.txt",     [this](){ _handleRedirect(); });
    _server.onNotFound(               [this](){ _handleRedirect(); });

    _server.begin();
}

void CaptivePortal::loop() {
    _dns.processNextRequest();
    _server.handleClient();
}

// ── Route handlers ────────────────────────────────────────────────────────────

void CaptivePortal::_handleRoot() {
    _server.sendHeader("Cache-Control", "no-cache");
    _server.send_P(200, "text/html", PORTAL_HTML);
}

void CaptivePortal::_handleConfig() {
    // Return current NVS config so the portal can pre-populate fields (PRD §8.6)
    DeviceConfig cfg;
    _nvs.loadConfig(cfg);

    JsonDocument doc;
    doc["ssid"]    = cfg.wifiSSID;
    // Do NOT return the password — portal re-enters it on change
    doc["provider"]     = (cfg.dataProvider == DataProvider::TWELVE_DATA) ? "twelve-data" : "finnhub";
    doc["apiKey"]       = cfg.apiKey;
    doc["board"]        = (cfg.boardTarget == BoardTarget::XIAO_PANEL) ? "xiao-panel" : "reterminal-e1001";
    doc["panelVariant"] = (cfg.panelVariant == PanelVariant::PANEL_A) ? "A" : "B";
    doc["powerMode"]    = (cfg.powerMode == PowerMode::BATTERY_SAVER) ? "battery-saver" : "quick-wake";
    doc["refH"]         = cfg.refIntervalMin / 60;
    doc["refM"]         = cfg.refIntervalMin % 60;
    doc["rotH"]         = cfg.rotIntervalMin / 60;
    doc["rotM"]         = cfg.rotIntervalMin % 60;

    JsonArray tickers = doc["tickers"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.tickerCount; i++) {
        JsonObject t = tickers.add<JsonObject>();
        t["symbol"] = cfg.tickers[i].symbol;
        t["mode"]   = (cfg.tickers[i].mode == TickerMode::STATIC) ? "static" : "rotating";
    }

    String body;
    serializeJson(doc, body);
    _server.send(200, "application/json", body);
}

void CaptivePortal::_handleScan() {
    // Block for 3-4s while scanning (single-user portal, acceptable)
    String json = _wifi.scanNetworksJSON();
    _server.send(200, "application/json", json);
}

void CaptivePortal::_handleWifiTest() {
    // Live WiFi test while AP stays running (AP+STA concurrent, PRD §8.5)
    if (!_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"No body\"}");
        return;
    }

    JsonDocument req;
    if (deserializeJson(req, _server.arg("plain")) != DeserializationError::Ok) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"Bad JSON\"}");
        return;
    }

    const char* ssid = req["ssid"] | "";
    const char* pass = req["pass"] | "";

    if (strlen(ssid) == 0) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"SSID required\"}");
        return;
    }

    bool ok = _wifi.testCredentials(ssid, pass);

    JsonDocument res;
    res["ok"] = ok;
    if (!ok) {
        char errBuf[120];
        snprintf(errBuf, sizeof(errBuf),
                 "Could not connect to \"%s\". Check the network name and password, then try again.", ssid);
        res["error"] = errBuf;
    }

    String body;
    serializeJson(res, body);
    _server.send(200, "application/json", body);
}

void CaptivePortal::_handleApiTest() {
    if (!_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"No body\"}");
        return;
    }
    JsonDocument req;
    if (deserializeJson(req, _server.arg("plain")) != DeserializationError::Ok) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"Bad JSON\"}");
        return;
    }

    const char* ssid     = req["ssid"]     | "";
    const char* pass     = req["pass"]     | "";
    const char* provStr  = req["provider"] | "twelve-data";
    const char* apiKey   = req["apiKey"]   | "";

    if (strlen(ssid) == 0 || strlen(apiKey) == 0) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"SSID and API key required\"}");
        return;
    }

    // Bring up STA alongside the AP (same pattern as WiFi test).
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid, pass);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            WiFi.disconnect(false);
            _server.send(200, "application/json",
                "{\"ok\":false,\"error\":\"Could not reach your WiFi — re-test WiFi first.\"}");
            return;
        }
        delay(100);
    }

    DataProvider which = (strcmp(provStr, "finnhub") == 0)
                         ? DataProvider::FINNHUB : DataProvider::TWELVE_DATA;
    IDataProvider* provider = createProvider(which);
    provider->setApiKey(apiKey);

    TickerData td = {};
    FetchResult r = provider->fetchTicker("AAPL", td);
    delete provider;

    WiFi.disconnect(false);  // keep AP up

    JsonDocument res;
    const char* err = nullptr;
    switch (r) {
        case FetchResult::OK:            res["ok"] = true; break;
        case FetchResult::AUTH_ERROR:    err = "API key rejected. Check the key and try again."; break;
        case FetchResult::RATE_LIMITED:  err = "Rate limit reached on this key. Try again in a minute."; break;
        case FetchResult::PARSE_ERROR:   err = "Unexpected response from provider."; break;
        case FetchResult::NETWORK_ERROR: err = "Could not reach the provider."; break;
    }
    if (err) { res["ok"] = false; res["error"] = err; }

    String body;
    serializeJson(res, body);
    _server.send(200, "application/json", body);
}

void CaptivePortal::_handleSave() {
    if (!_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"No body\"}");
        return;
    }

    JsonDocument req;
    if (deserializeJson(req, _server.arg("plain")) != DeserializationError::Ok) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"Bad JSON\"}");
        return;
    }

    DeviceConfig cfg = {};

    // WiFi
    strlcpy(cfg.wifiSSID, req["ssid"] | "", sizeof(cfg.wifiSSID));
    strlcpy(cfg.wifiPass, req["pass"] | "", sizeof(cfg.wifiPass));

    // Provider
    const char* provider = req["provider"] | "twelve-data";
    cfg.dataProvider = (strcmp(provider, "finnhub") == 0)
                       ? DataProvider::FINNHUB : DataProvider::TWELVE_DATA;
    strlcpy(cfg.apiKey, req["apiKey"] | "", sizeof(cfg.apiKey));

    // Tickers
    JsonArray tickers = req["tickers"].as<JsonArray>();
    cfg.tickerCount = 0;
    for (JsonObject t : tickers) {
        if (cfg.tickerCount >= MAX_TICKERS) break;
        strlcpy(cfg.tickers[cfg.tickerCount].symbol, t["symbol"] | "", 12);
        // Positions 6+ (index >= 5) are always ROTATING regardless of what the portal sent
        if (cfg.tickerCount >= 5) {
            cfg.tickers[cfg.tickerCount].mode = TickerMode::ROTATING;
        } else {
            const char* mode = t["mode"] | "rotating";
            cfg.tickers[cfg.tickerCount].mode = (strcmp(mode, "static") == 0)
                                                ? TickerMode::STATIC : TickerMode::ROTATING;
        }
        cfg.tickerCount++;
    }

    // Timing
    uint16_t rotH = req["rotH"] | 0;
    uint16_t rotM = req["rotM"] | 30;
    uint16_t refH = req["refH"] | 1;
    uint16_t refM = req["refM"] | 0;
    uint16_t minInterval = (cfg.boardTarget == BoardTarget::RETERMINAL_E1002)
                           ? MIN_INTERVAL_C_MIN : MIN_INTERVAL_AB_MIN;
    cfg.rotIntervalMin = max((uint16_t)(rotH * 60 + rotM), minInterval);
    cfg.refIntervalMin = max((uint16_t)(refH * 60 + refM), minInterval);

    // Hardware
    const char* board = req["board"] | "xiao-panel";
    cfg.boardTarget = (strcmp(board, "reterminal-e1001") == 0)
                      ? BoardTarget::RETERMINAL_E1001 : BoardTarget::XIAO_PANEL;

    const char* pv = req["panelVariant"] | "A";
    cfg.panelVariant = (strcmp(pv, "B") == 0) ? PanelVariant::PANEL_B : PanelVariant::PANEL_A;

    // Power mode
    const char* pm = req["powerMode"] | "battery-saver";
    cfg.powerMode = (strcmp(pm, "quick-wake") == 0) ? PowerMode::QUICK_WAKE : PowerMode::BATTERY_SAVER;

    // Validate minimums
    if (cfg.tickerCount == 0 || strlen(cfg.wifiSSID) == 0 || strlen(cfg.apiKey) == 0) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing required fields\"}");
        return;
    }

    // Commit to NVS
    _nvs.saveConfig(cfg);
    _nvs.resetFailCount();
    _nvs.setRotPosition(0);

    // Respond before restarting so the browser gets the response
    _server.send(200, "application/json", "{\"ok\":true}");
    delay(200);

    _saved = true;
    // main.cpp will call ESP.restart() when didSave() is true
}

void CaptivePortal::_handleRedirect() {
    _server.sendHeader("Location", String("http://") + AP_IP_STR + "/");
    _server.send(302, "text/plain", "");
}
