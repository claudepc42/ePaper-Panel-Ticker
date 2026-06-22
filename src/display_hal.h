#pragma once
#include "types.h"
#include <Arduino.h>

// ── IDisplay — hardware abstraction layer ─────────────────────────────────────
// All rendering entry points for the firmware.
// Board A and B use the same MonoRenderer implementation.
// Board C would add a ColorRenderer — see hal/display_board_c.h stub.
//
// All methods block until the e-paper refresh is complete before returning.

class IDisplay {
public:
    virtual ~IDisplay() = default;

    // Called once at startup. Initialises the SPI bus and display controller.
    virtual void init() = 0;

    // Render the full Market Overview layout.
    // indices[3]: SPX / NASDAQ / DOW; can be .valid=false if unavailable.
    // tickers: pre-selected display set (up to 5 rows, built by buildDisplaySet).
    // fetchedAt: timestamp of last successful data fetch (0 = never).
    virtual void drawMarketOverview(
        const IndexData    indices[3],
        const TickerData*  tickers,
        uint8_t            tickerCount,
        time_t             fetchedAt) = 0;

    // Render the WiFi Unavailable screen.
    // If cache.fetchedAt == 0 the screen shows the "no cached data" empty state.
    virtual void drawWifiUnavailable(const CachedTickerSet& cache) = 0;

    // Render the rate-limit degraded state (extends WiFi Unavailable pattern,
    // PRD §9.5).
    virtual void drawRateLimited(const CachedTickerSet& cache) = 0;

    // Render the first-boot Setup Required screen with AP SSID + IP.
    virtual void drawSetupRequired(const char* apSsid, const char* apIP) = 0;

    // Clears the display to white. Used before deep sleep if desired.
    virtual void clear() = 0;
};
