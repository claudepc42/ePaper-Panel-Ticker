#pragma once
#include "../display_hal.h"
#include "../types.h"

// ── Board B: reTerminal E1001 (XIAO ESP32-S3 internal, 7.5" mono ePaper) ─────
//
// PRD §2.2: UC8179 controller confirmed — same family as Board A's Panel A.
// We use GxEPD2_750_T7 (UC8179 driver) as the first choice, matching Board A,
// to share the MonoRenderer code path.
//
// TODO (PRD §11 item 8): Verify GxEPD2 actually has mature support for this
// specific board. If rendering is wrong or unstable, fall back to Seeed_GFX
// for Board B and accept diverged rendering code for that target.
//
// Board B pins: internal wiring — these are best-effort defaults based on the
// XIAO ESP32-S3 SPI bus. Adjust EPB_* defines in config.h / build flags if
// Seeed publishes the actual internal pin mapping for the E1001.
//
// TODO (PRD §11 item 2): Confirm wake-to-render timing on Board B hardware.

// Board B internal ePaper SPI pins — confirmed from Seeed Arduino cookbook
// for the reTerminal E1001 (wiki.seeedstudio.com/reterminal_e10xx_with_arduino)
#ifndef EPB_CS
#  define EPB_CS    10
#endif
#ifndef EPB_DC
#  define EPB_DC    11
#endif
#ifndef EPB_RST
#  define EPB_RST   12
#endif
#ifndef EPB_BUSY
#  define EPB_BUSY  13
#endif

class MonoRenderer;

class DisplayBoardB : public IDisplay {
public:
    DisplayBoardB();
    ~DisplayBoardB() override;

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
    void*         _display;    // GxEPD2_BW<GxEPD2_750_T7,...>*
    MonoRenderer* _renderer;
};
