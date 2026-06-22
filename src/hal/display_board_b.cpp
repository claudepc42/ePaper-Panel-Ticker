#include "display_board_b.h"
#include "../renderer_mono.h"
#include <SPI.h>
#include <GxEPD2_BW.h>
// GxEPD2_750_T7 pulled in by GxEPD2_BW.h via __has_include

static constexpr int PAGE_H_B = GxEPD2_750_T7::HEIGHT / 4;
using DisplayTypeB1 = GxEPD2_BW<GxEPD2_750_T7, PAGE_H_B>;

DisplayBoardB::DisplayBoardB()
    : _display(nullptr), _renderer(nullptr)
{}

DisplayBoardB::~DisplayBoardB() {
    delete _renderer;
    delete static_cast<DisplayTypeB1*>(_display);
}

void DisplayBoardB::init() {
    if (_renderer != nullptr) {
        Serial.println("[BoardB] re-init (quick wake)");
        static_cast<DisplayTypeB1*>(_display)->init(115200, true, 2, false);
        return;
    }
    Serial.printf("[BoardB] new GxEPD2 CS=%d DC=%d RST=%d BUSY=%d\n",
                  EPB_CS, EPB_DC, EPB_RST, EPB_BUSY);
    auto* d = new DisplayTypeB1(
        GxEPD2_750_T7(EPB_CS, EPB_DC, EPB_RST, EPB_BUSY));
    _display = d;
    Serial.println("[BoardB] GxEPD2 init() — waits on BUSY pin");
    d->init(115200, true, 2, false);
    Serial.println("[BoardB] GxEPD2 init() returned");
    _renderer = new MonoRenderer(d);
    Serial.println("[BoardB] renderer ready");
}

void DisplayBoardB::drawMarketOverview(const IndexData indices[3], const TickerData* tickers,
                                       uint8_t tickerCount, time_t fetchedAt) {
    _renderer->drawMarketOverview(indices, tickers, tickerCount, fetchedAt);
}

void DisplayBoardB::drawWifiUnavailable(const CachedTickerSet& cache) {
    _renderer->drawWifiUnavailable(cache);
}

void DisplayBoardB::drawRateLimited(const CachedTickerSet& cache) {
    _renderer->drawRateLimited(cache);
}

void DisplayBoardB::drawSetupRequired(const char* apSsid, const char* apIP) {
    _renderer->drawSetupRequired(apSsid, apIP);
}

void DisplayBoardB::clear() {
    auto* d = static_cast<DisplayTypeB1*>(_display);
    d->setFullWindow();
    d->firstPage();
    do { d->fillScreen(GxEPD_WHITE); } while (d->nextPage());
}
