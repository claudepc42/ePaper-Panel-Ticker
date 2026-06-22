#pragma once
#include "../display_hal.h"
#include "../types.h"
#include "../config.h"

// ── Board A: XIAO ESP32-C3 + external 7.5" ePaper panel ─────────────────────
//
// Two candidate driver classes compiled in; active one selected at runtime
// from the PanelVariant NVS setting (PRD §6.4, §5.2).
//
// TODO (PRD §11 item 1): Verify actual GxEPD2 class names before first flash.
//   Candidate A (default): GxEPD2_750_T7  → GDEY075T7 / UC8179 controller
//   Candidate B:           GxEPD2_750     → GDEW075T7 / EK79655 (GD7965) controller
// Names used here are from the GxEPD2 library headers; confirm they match the
// installed library version.

class MonoRenderer;  // forward decl

class DisplayBoardA : public IDisplay {
public:
    explicit DisplayBoardA(PanelVariant variant);
    ~DisplayBoardA() override;

    void init() override;

    void drawMarketOverview(const IndexData    indices[3],
                            const TickerData*  tickers,
                            uint8_t            tickerCount,
                            time_t             fetchedAt) override;

    void drawWifiUnavailable(const CachedTickerSet& cache) override;
    void drawRateLimited(const CachedTickerSet& cache) override;
    void drawSetupRequired(const char* apSsid, const char* apIP) override;
    void clear() override;

private:
    PanelVariant _variant;
    void*        _displayA;  // GxEPD2_BW<GxEPD2_750_T7,...>* — stored as void* to avoid
    void*        _displayB;  // GxEPD2_BW<GxEPD2_750,...>*    — including GxEPD2 headers here
    MonoRenderer* _renderer;
};
