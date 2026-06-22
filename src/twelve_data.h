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
    FetchResult fetchTimeSeries(const char* symbol, float* out,
                                uint8_t count, const char* interval) override;
    const char* name() const override { return "Twelve Data"; }

private:
    char _apiKey[128];

    String _get(const String& path, int* httpCode);
    FetchResult _parseQuote(const String& body, TickerData& td);
    FetchResult _parseTimeSeries(const String& body, float* out, uint8_t count);
};
