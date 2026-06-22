#pragma once
#include <Arduino.h>

// ── Enums ─────────────────────────────────────────────────────────────────────

enum class BoardTarget : uint8_t {
    XIAO_PANEL       = 0,   // Board A: XIAO ESP32-C3 + external 7.5" panel
    RETERMINAL_E1001 = 1,   // Board B: reTerminal E1001 (XIAO ESP32-S3 internal)
    RETERMINAL_E1002 = 2,   // Board C: reTerminal E1002 (color) — reserved, not in scope this round
};

enum class PanelVariant : uint8_t {
    PANEL_A = 0,  // GDEY075T7 / UC8179 — default guess (PRD §6.4)
    PANEL_B = 1,  // GDEW075T7 / EK79655 (GD7965)
};

enum class DataProvider : uint8_t {
    TWELVE_DATA = 0,
    FINNHUB     = 1,
};

enum class PowerMode : uint8_t {
    BATTERY_SAVER = 0,  // esp_deep_sleep_start()
    QUICK_WAKE    = 1,  // light sleep with WiFi modem sleep
};

enum class TickerMode : uint8_t {
    STATIC   = 0,
    ROTATING = 1,
};

// ── Stored ticker entry (from NVS config) ─────────────────────────────────────

struct TickerEntry {
    char       symbol[12];
    TickerMode mode;
};

// ── Live/cached ticker data ───────────────────────────────────────────────────

struct TickerData {
    char    symbol[12];
    char    name[48];
    float   price;
    float   change;
    float   changePct;
    float   dayHigh;
    float   dayLow;
    float   history[15];    // 2h-interval closes, oldest→newest (~5 trading days)
    bool    valid;
    bool    hasDayHiLo;
    bool    hasHistory;
};

// ── Index data (SPX / NASDAQ / DOW) ──────────────────────────────────────────

struct IndexData {
    char    symbol[8];
    char    name[32];
    float   price;
    float   change;
    float   changePct;
    float   history[6];     // 1h-interval closes, oldest→newest (rolling 6hrs)
    bool    valid;
    bool    hasHistory;
};

// ── Device configuration (NVS-persisted) ─────────────────────────────────────

static constexpr int MAX_TICKERS = 20;

struct DeviceConfig {
    // WiFi
    char        wifiSSID[64];
    char        wifiPass[64];

    // Hardware
    BoardTarget  boardTarget;
    PanelVariant panelVariant;   // only meaningful when boardTarget == XIAO_PANEL

    // Data
    DataProvider dataProvider;
    char         apiKey[128];

    // Timing (total minutes)
    uint16_t     rotIntervalMin;
    uint16_t     refIntervalMin;

    // Power
    PowerMode    powerMode;

    // Tickers
    TickerEntry  tickers[MAX_TICKERS];
    uint8_t      tickerCount;
};

// ── Runtime state passed to renderer ─────────────────────────────────────────

struct CachedTickerSet {
    TickerData   tickers[MAX_TICKERS];
    uint8_t      count;
    time_t       fetchedAt;   // 0 = never fetched
};

enum class DisplayState {
    MARKET_OVERVIEW,       // Normal: connected + data
    WIFI_UNAVAILABLE,      // WiFi fail: show cached data (may be empty)
    RATE_LIMITED,          // 429 from provider — extend WiFi unavailable pattern
    SETUP_REQUIRED,        // First boot with no saved credentials
};
