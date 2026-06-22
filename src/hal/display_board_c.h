#pragma once
#include "../display_hal.h"

// ── Board C stub: reTerminal E1002 (color Spectra 6 panel) ──────────────────
//
// NOT IN SCOPE for this implementation round (PRD §2.3, §6.6).
// The abstraction layer (IDisplay) and enum slot (BoardTarget::RETERMINAL_E1002)
// are in place so Board C can be added later without restructuring.
//
// When implementing Board C:
//   - Controller: ED2208 (different from Boards A/B UC8179)
//   - Library:    Seeed_GFX Colorful path (separate from the Basic/mono path used A/B)
//   - Renderer:   New ColorRenderer — NOT a recolored MonoRenderer; color semantics
//                 (red/green for gains/losses) don't exist in the grayscale path (PRD §6.6)
//   - Refresh:    ~30s full-color refresh — design all UI around full-screen redraws,
//                 no partial updates (PRD §2.3.1)
//   - Min interval: 30 min floor enforced in portal validation (PRD §2.3.1)
//
// TODO (PRD §11 item 9): Confirm actual refresh time on real hardware before
// finalising the 30-minute minimum. The ~30s figure is a community report, not
// a datasheet spec.
//
// TODO (PRD §11 item 11): Decide whether red/green color mapping is
// user-configurable (colorblind accessibility) or hardcoded.

class DisplayBoardC : public IDisplay {
public:
    void init() override {}

    void drawMarketOverview(const IndexData*, const TickerData*, uint8_t, time_t) override {
        // TODO: implement ColorRenderer for Spectra 6 panel
        Serial.println("[BoardC] Color rendering not yet implemented");
    }

    void drawWifiUnavailable(const CachedTickerSet&) override {
        Serial.println("[BoardC] Color rendering not yet implemented");
    }

    void drawRateLimited(const CachedTickerSet&) override {
        Serial.println("[BoardC] Color rendering not yet implemented");
    }

    void drawSetupRequired(const char*, const char*) override {
        Serial.println("[BoardC] Color rendering not yet implemented");
    }

    void clear() override {}
};
