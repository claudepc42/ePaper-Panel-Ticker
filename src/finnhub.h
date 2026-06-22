#pragma once
#include "data_provider.h"

// Finnhub provider (PRD §9.3)
// Free tier: 60 calls/min (well-documented); daily/monthly cap unverified (PRD §9.1).
// ~20-minute data delay.
//
// TODO (PRD §11 item 7): Confirm field-set parity with Twelve Data.
// If Finnhub's free tier lacks 52-week range or day H/L, td.hasWeek52 /
// td.hasDayHiLo will be false and those columns will be hidden by the renderer.
// Do not assume parity — verify before marking this provider production-ready.
//
// TODO (PRD §11 item 4): Verify observed daily/monthly limits on a real
// account dashboard.

class FinnhubProvider : public IDataProvider {
public:
    void setApiKey(const char* key) override;
    FetchResult fetchTicker(const char* symbol, TickerData& td) override;
    FetchResult fetchIndexSummary(IndexData idx[3]) override;
    FetchResult fetchTimeSeries(const char* symbol, float* out,
                                uint8_t count, const char* interval) override;
    const char* name() const override { return "Finnhub"; }

private:
    char _apiKey[128];
    String _get(const String& path, int* httpCode);
};
