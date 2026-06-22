#pragma once
#include "data_provider.h"

// Twelve Data provider (PRD §9.2)
// Free tier: 800 calls/day, ~4-hour data delay.
// Default provider — simple single daily limit makes user quota math easy.
//
// TODO (PRD §11 item 3): Verify day_high/day_low and 52-week range are
// returned on the free tier before shipping. If not available, td.hasWeek52
// and td.hasDayHiLo will be set false and the renderer will hide those columns.
//
// TODO (PRD §11 item 4): Confirm actual observed daily limit against a real
// account dashboard before finalizing default refresh interval recommendations
// shown in the portal UI.

class TwelveDataProvider : public IDataProvider {
public:
    void setApiKey(const char* key) override;
    FetchResult fetchTicker(const char* symbol, TickerData& td) override;
    FetchResult fetchIndexSummary(IndexData idx[3]) override;
    const char* name() const override { return "Twelve Data"; }

private:
    char _apiKey[128];

    // Makes HTTPS GET to Twelve Data and returns the body, or empty string on error.
    // Sets *httpCode to the HTTP status code.
    String _get(const String& path, int* httpCode);

    // Parse a single "quote" JSON response into TickerData.
    FetchResult _parseQuote(const String& body, TickerData& td);
};
