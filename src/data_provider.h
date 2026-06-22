#pragma once
#include "types.h"
#include "config.h"
#include <Arduino.h>

// ── FetchResult ───────────────────────────────────────────────────────────────

enum class FetchResult {
    OK,
    RATE_LIMITED,    // 429 — surfaces as a visible state per PRD §9.5
    NETWORK_ERROR,
    PARSE_ERROR,
    AUTH_ERROR,      // 401/403
};

// ── IDataProvider ─────────────────────────────────────────────────────────────
// Abstract interface. Each concrete provider implements fetch() and the index
// fetch for SPX/NASDAQ/DOW summary data.
//
// Adding a third provider later: implement this interface only; no changes
// needed to rendering, caching, or portal code (PRD §9.4).

class IDataProvider {
public:
    virtual ~IDataProvider() = default;

    virtual void setApiKey(const char* key) = 0;

    // Fetch data for a single ticker symbol. Populates td on success.
    // Returns FetchResult::OK or an error code.
    virtual FetchResult fetchTicker(const char* symbol, TickerData& td) = 0;

    // Fetch the three major index summaries (SPX, NASDAQ, DOW).
    // These are displayed in the top panel of Market Overview.
    // Implementations that can't supply these should set idx[n].valid = false.
    virtual FetchResult fetchIndexSummary(IndexData idx[3]) = 0;

    // Human-readable provider name for display in portal/logs.
    virtual const char* name() const = 0;
};

// ── Factory ───────────────────────────────────────────────────────────────────

IDataProvider* createProvider(DataProvider which);
