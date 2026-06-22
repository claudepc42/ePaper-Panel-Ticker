#include "display_board_a.h"
#include "../renderer_mono.h"
#include <SPI.h>

// ── GxEPD2 includes ───────────────────────────────────────────────────────────
// TODO (PRD §11 item 1): Confirm both class names in the installed library.
// GxEPD2_750_T7 → GDEY075T7/UC8179 (Panel A, default per PRD §6.4)
// GxEPD2_750    → GDEW075T7/EK79655 (Panel B)
//
// If either class name is wrong you will get a compile error naming the missing
// type — switch the #include path and class name accordingly.
#include <GxEPD2_BW.h>
// GxEPD2_750_T7 and GxEPD2_750 are pulled in by GxEPD2_BW.h via __has_include

// Display buffer height fraction (controls RAM usage per page)
// HEIGHT/4 = 120 px per page for 480-px panel
static constexpr int PAGE_H = GxEPD2_750_T7::HEIGHT / 4;

using DisplayTypeA = GxEPD2_BW<GxEPD2_750_T7, PAGE_H>;
using DisplayTypeB = GxEPD2_BW<GxEPD2_750,    PAGE_H>;

// ── Accessors — thin wrappers returning GxEPD2_GFX* ─────────────────────────

static GxEPD2_GFX* castA(void* p) { return static_cast<DisplayTypeA*>(p); }
static GxEPD2_GFX* castB(void* p) { return static_cast<DisplayTypeB*>(p); }

// ── DisplayBoardA ─────────────────────────────────────────────────────────────

DisplayBoardA::DisplayBoardA(PanelVariant variant)
    : _variant(variant), _displayA(nullptr), _displayB(nullptr), _renderer(nullptr)
{}

DisplayBoardA::~DisplayBoardA() {
    delete _renderer;
    if (_variant == PanelVariant::PANEL_A) {
        delete static_cast<DisplayTypeA*>(_displayA);
    } else {
        delete static_cast<DisplayTypeB*>(_displayB);
    }
}

void DisplayBoardA::init() {
    GxEPD2_GFX* gfx;

    if (_renderer != nullptr) {
        // Re-init after Quick Wake light sleep — hardware reinit only, no new heap allocs
        gfx = (_variant == PanelVariant::PANEL_A) ? castA(_displayA) : castB(_displayB);
        gfx->init(115200, true, 2, false);
        return;
    }

    if (_variant == PanelVariant::PANEL_A) {
        auto* d = new DisplayTypeA(
            GxEPD2_750_T7(EPAPER_CS, EPAPER_DC, EPAPER_RST, EPAPER_BUSY));
        _displayA = d;
        gfx = d;
    } else {
        auto* d = new DisplayTypeB(
            GxEPD2_750(EPAPER_CS, EPAPER_DC, EPAPER_RST, EPAPER_BUSY));
        _displayB = d;
        gfx = d;
    }

    gfx->init(115200, true, 2, false);
    _renderer = new MonoRenderer(gfx);
}

void DisplayBoardA::drawMarketOverview(const IndexData indices[3], const TickerData* tickers,
                                       uint8_t tickerCount, time_t fetchedAt) {
    _renderer->drawMarketOverview(indices, tickers, tickerCount, fetchedAt);
}

void DisplayBoardA::drawWifiUnavailable(const CachedTickerSet& cache) {
    _renderer->drawWifiUnavailable(cache);
}

void DisplayBoardA::drawRateLimited(const CachedTickerSet& cache) {
    _renderer->drawRateLimited(cache);
}

void DisplayBoardA::drawSetupRequired(const char* apSsid, const char* apIP) {
    _renderer->drawSetupRequired(apSsid, apIP);
}

void DisplayBoardA::clear() {
    GxEPD2_GFX* gfx = (_variant == PanelVariant::PANEL_A) ? castA(_displayA) : castB(_displayB);
    gfx->setFullWindow();
    gfx->firstPage();
    do {
        gfx->fillScreen(GxEPD_WHITE);
    } while (gfx->nextPage());
}
