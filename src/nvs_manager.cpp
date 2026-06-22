#include "nvs_manager.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences prefs;

// ── isConfigured ─────────────────────────────────────────────────────────────

bool NVSManager::isConfigured() {
    prefs.begin(NVS_NAMESPACE, true);
    bool hasSsid    = prefs.isKey(NVS_WIFI_SSID) && prefs.getString(NVS_WIFI_SSID, "").length() > 0;
    bool hasTickers = prefs.isKey(NVS_TICKER_COUNT) && prefs.getUChar(NVS_TICKER_COUNT, 0) > 0;
    prefs.end();
    return hasSsid && hasTickers;
}

// ── loadConfig ────────────────────────────────────────────────────────────────

void NVSManager::loadConfig(DeviceConfig& cfg) {
    prefs.begin(NVS_NAMESPACE, true);

    // WiFi
    strlcpy(cfg.wifiSSID, prefs.getString(NVS_WIFI_SSID, "").c_str(), sizeof(cfg.wifiSSID));
    strlcpy(cfg.wifiPass, prefs.getString(NVS_WIFI_PASS, "").c_str(), sizeof(cfg.wifiPass));

    // Hardware — default board target matches the compile-time build flag so
    // first-boot config is correct even before the portal is completed.
#if defined(BOARD_BUILD_B)
    constexpr uint8_t DEFAULT_BOARD = static_cast<uint8_t>(BoardTarget::RETERMINAL_E1001);
#elif defined(BOARD_BUILD_C)
    constexpr uint8_t DEFAULT_BOARD = static_cast<uint8_t>(BoardTarget::RETERMINAL_E1002);
#else
    constexpr uint8_t DEFAULT_BOARD = static_cast<uint8_t>(BoardTarget::XIAO_PANEL);
#endif
    cfg.boardTarget  = static_cast<BoardTarget>(prefs.getUChar(NVS_BOARD_TARGET, DEFAULT_BOARD));
    cfg.panelVariant = static_cast<PanelVariant>(prefs.getUChar(NVS_PANEL_VARIANT, 0));

    // Data
    cfg.dataProvider = static_cast<DataProvider>(prefs.getUChar(NVS_PROVIDER, 0));
    strlcpy(cfg.apiKey, prefs.getString(NVS_API_KEY, "").c_str(), sizeof(cfg.apiKey));

    // Timing
    cfg.rotIntervalMin = prefs.getUShort(NVS_ROT_INTERVAL, DEFAULT_ROT_INTERVAL_MIN);
    cfg.refIntervalMin = prefs.getUShort(NVS_REF_INTERVAL, DEFAULT_REF_INTERVAL_MIN);

    // Power
    cfg.powerMode = static_cast<PowerMode>(prefs.getUChar(NVS_POWER_MODE, 0));

    // Tickers
    cfg.tickerCount = prefs.getUChar(NVS_TICKER_COUNT, 0);
    for (uint8_t i = 0; i < cfg.tickerCount && i < MAX_TICKERS; i++) {
        String key = String(NVS_TICKER_PREFIX) + i;
        String json = prefs.getString(key.c_str(), "{}");

        JsonDocument doc;
        if (deserializeJson(doc, json) == DeserializationError::Ok) {
            strlcpy(cfg.tickers[i].symbol, doc["sym"] | "", sizeof(cfg.tickers[i].symbol));
            cfg.tickers[i].mode = static_cast<TickerMode>(doc["mode"] | 0);
        }
    }

    prefs.end();
}

// ── saveConfig ────────────────────────────────────────────────────────────────

void NVSManager::saveConfig(const DeviceConfig& cfg) {
    prefs.begin(NVS_NAMESPACE, false);

    // Delete cache entries for any tickers currently in NVS before overwriting the
    // config — prevents orphaned keys accumulating when the ticker list changes.
    uint8_t oldCount = prefs.getUChar(NVS_TICKER_COUNT, 0);
    for (uint8_t i = 0; i < oldCount && i < MAX_TICKERS; i++) {
        String oldJson = prefs.getString((String(NVS_TICKER_PREFIX) + i).c_str(), "");
        JsonDocument oldDoc;
        if (!oldJson.isEmpty() && deserializeJson(oldDoc, oldJson) == DeserializationError::Ok) {
            const char* sym = oldDoc["sym"] | "";
            if (sym[0]) prefs.remove((String(NVS_CACHE_PREFIX) + sym).c_str());
        }
    }

    prefs.putString(NVS_WIFI_SSID,    cfg.wifiSSID);
    prefs.putString(NVS_WIFI_PASS,    cfg.wifiPass);
    prefs.putUChar(NVS_BOARD_TARGET,  static_cast<uint8_t>(cfg.boardTarget));
    prefs.putUChar(NVS_PANEL_VARIANT, static_cast<uint8_t>(cfg.panelVariant));
    prefs.putUChar(NVS_PROVIDER,      static_cast<uint8_t>(cfg.dataProvider));
    prefs.putString(NVS_API_KEY,      cfg.apiKey);
    prefs.putUShort(NVS_ROT_INTERVAL, cfg.rotIntervalMin);
    prefs.putUShort(NVS_REF_INTERVAL, cfg.refIntervalMin);
    prefs.putUChar(NVS_POWER_MODE,    static_cast<uint8_t>(cfg.powerMode));
    prefs.putUChar(NVS_TICKER_COUNT,  cfg.tickerCount);

    for (uint8_t i = 0; i < cfg.tickerCount && i < MAX_TICKERS; i++) {
        JsonDocument doc;
        doc["sym"]  = cfg.tickers[i].symbol;
        doc["mode"] = static_cast<uint8_t>(cfg.tickers[i].mode);

        String json;
        serializeJson(doc, json);
        String key = String(NVS_TICKER_PREFIX) + i;
        prefs.putString(key.c_str(), json);
    }

    prefs.end();
}

// ── Cache ─────────────────────────────────────────────────────────────────────

void NVSManager::loadCache(CachedTickerSet& cache) {
    prefs.begin(NVS_NAMESPACE, true);

    cache.fetchedAt = (time_t)prefs.getLong64(NVS_CACHE_TIMESTAMP, 0);
    cache.count = 0;

    uint8_t n = prefs.getUChar(NVS_TICKER_COUNT, 0);
    for (uint8_t i = 0; i < n && i < MAX_TICKERS; i++) {
        // Derive the ticker symbol from config to look up its cached data
        String cfgKey = String(NVS_TICKER_PREFIX) + i;
        String cfgJson = prefs.getString(cfgKey.c_str(), "{}");
        JsonDocument cfgDoc;
        if (deserializeJson(cfgDoc, cfgJson) != DeserializationError::Ok) continue;

        const char* sym = cfgDoc["sym"] | "";
        if (sym[0] == '\0') continue;

        String cacheKey = String(NVS_CACHE_PREFIX) + sym;
        String cacheJson = prefs.getString(cacheKey.c_str(), "");
        if (cacheJson.isEmpty()) continue;

        JsonDocument cDoc;
        if (deserializeJson(cDoc, cacheJson) != DeserializationError::Ok) continue;

        TickerData& td = cache.tickers[cache.count++];
        strlcpy(td.symbol,   cDoc["sym"]  | sym,  sizeof(td.symbol));
        strlcpy(td.name,     cDoc["name"] | "",    sizeof(td.name));
        td.price      = cDoc["price"]   | 0.0f;
        td.change     = cDoc["chg"]     | 0.0f;
        td.changePct  = cDoc["chgPct"]  | 0.0f;
        td.dayHigh    = cDoc["dayH"]    | 0.0f;
        td.dayLow     = cDoc["dayL"]    | 0.0f;
        td.week52High = cDoc["w52H"]    | 0.0f;
        td.week52Low  = cDoc["w52L"]    | 0.0f;
        td.hasWeek52  = cDoc["hasW52"]  | false;
        td.hasDayHiLo = cDoc["hasDHL"]  | false;
        td.valid      = true;
    }

    prefs.end();
}

void NVSManager::saveCache(const CachedTickerSet& cache) {
    prefs.begin(NVS_NAMESPACE, false);

    prefs.putLong64(NVS_CACHE_TIMESTAMP, (int64_t)cache.fetchedAt);

    for (uint8_t i = 0; i < cache.count; i++) {
        const TickerData& td = cache.tickers[i];
        if (!td.valid) continue;

        JsonDocument doc;
        doc["sym"]    = td.symbol;
        doc["name"]   = td.name;
        doc["price"]  = td.price;
        doc["chg"]    = td.change;
        doc["chgPct"] = td.changePct;
        doc["dayH"]   = td.dayHigh;
        doc["dayL"]   = td.dayLow;
        doc["w52H"]   = td.week52High;
        doc["w52L"]   = td.week52Low;
        doc["hasW52"] = td.hasWeek52;
        doc["hasDHL"] = td.hasDayHiLo;

        String json;
        serializeJson(doc, json);
        String key = String(NVS_CACHE_PREFIX) + td.symbol;
        prefs.putString(key.c_str(), json);
    }

    prefs.end();
}

bool NVSManager::hasCachedData() {
    prefs.begin(NVS_NAMESPACE, true);
    time_t ts = (time_t)prefs.getLong64(NVS_CACHE_TIMESTAMP, 0);
    prefs.end();
    return ts > 0;
}

// ── Runtime counters ──────────────────────────────────────────────────────────

uint8_t NVSManager::getRotPosition() {
    prefs.begin(NVS_NAMESPACE, true);
    uint8_t v = prefs.getUChar(NVS_ROT_POSITION, 0);
    prefs.end();
    return v;
}

void NVSManager::setRotPosition(uint8_t pos) {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUChar(NVS_ROT_POSITION, pos);
    prefs.end();
}

uint8_t NVSManager::getFailCount() {
    prefs.begin(NVS_NAMESPACE, true);
    uint8_t v = prefs.getUChar(NVS_FAIL_COUNT, 0);
    prefs.end();
    return v;
}

void NVSManager::setFailCount(uint8_t count) {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUChar(NVS_FAIL_COUNT, count);
    prefs.end();
}

void NVSManager::incrementFailCount() {
    setFailCount(getFailCount() + 1);
}

void NVSManager::resetFailCount() {
    setFailCount(0);
}
