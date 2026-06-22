#pragma once
#include "types.h"
#include <GxEPD2_GFX.h>
#include <time.h>

// ── MonoRenderer ─────────────────────────────────────────────────────────────
// Shared 4-tone grayscale renderer for Boards A and B.
//
// Palette (PRD §6.5 — flat fills only, no gradients, no anti-aliasing):
//   INK   = GxEPD_BLACK     (#0d0d0d) — primary text, borders, strong elements
//   DARK  = GxEPD_DARKGREY  (#595959) — secondary text, fund names
//   MID   = GxEPD_LIGHTGREY (#a8a8a8) — tertiary / disabled text, separators
//   PAPER = GxEPD_WHITE     (#f0ece3) — background
//
// Font mapping (Adafruit GFX bitmapped fonts, included by GxEPD2):
//   Large price values  → FreeMonoBold24pt7b  (~30px cap height)
//   Index changes       → FreeMonoBold12pt7b  (~17px)
//   Ticker symbols      → FreeMonoBold18pt7b  (~23px)
//   Normal body text    → FreeMonoBold9pt7b   (~13px)
//   Metadata / labels   → FreeMonoBold9pt7b   (~13px)

class MonoRenderer {
public:
    explicit MonoRenderer(GxEPD2_GFX* gfx);

    void drawMarketOverview(const IndexData    indices[3],
                            const TickerData*  tickers,
                            uint8_t            tickerCount,
                            time_t             fetchedAt);

    void drawWifiUnavailable(const CachedTickerSet& cache);
    void drawRateLimited(const CachedTickerSet& cache);
    void drawSetupRequired(const char* apSsid, const char* apIP);

private:
    GxEPD2_GFX* _gfx;

    // ── Layout constants ─────────────────────────────────────────────────────
    static constexpr int MX  = 18;    // horizontal margin
    static constexpr int W   = 800;
    static constexpr int H   = 480;
    static constexpr int CW  = W - 2 * MX;   // content width = 764px

    // Header zone
    static constexpr int HDR_Y      = 13;   // top of header text baseline area
    static constexpr int HDR_LINE_Y = 34;   // separator line

    // Index bar
    static constexpr int IDX_Y      = 38;   // top of index zone
    static constexpr int IDX_LINE_Y = 150;  // bottom separator
    // 3 column dividers at x=272 and x=527
    static constexpr int IDX_DIV1 = 272;
    static constexpr int IDX_DIV2 = 527;

    // Table
    static constexpr int TBL_Y     = 152;
    static constexpr int TBL_HDR_H = 24;
    static constexpr int TBL_LINE_Y= TBL_Y + TBL_HDR_H;   // = 176; header separator
    static constexpr int ROWS_TOP  = TBL_LINE_Y;            // = 176; first row starts here
    static constexpr int ROW_H     = 56;    // 5 rows × 56 = 280px → ends at 456 (2px above FTR_Y)
    static constexpr int FTR_Y     = 458;   // footer separator; must be > ROWS_TOP + 5*ROW_H

    // Table column x-starts (based on design grid, PRD §6.1)
    static constexpr int COL_TICK  = MX;          //  18 — TICKER  (70px)
    static constexpr int COL_FUND  = MX + 74;     //  92 — FUND    (186px)
    static constexpr int COL_PRICE = MX + 264;    // 282 — PRICE   (94px)
    static constexpr int COL_CHG   = MX + 362;    // 380 — DAY CHG (86px)
    static constexpr int COL_HL    = MX + 452;    // 470 — DAY H/L (162px)
    static constexpr int COL_W52   = MX + 618;    // 636 — 52W     (146px)

    // ── Helpers ──────────────────────────────────────────────────────────────
    void _drawSharedHeader(const char* title, const char* dateStr,
                           const char* bullet, const char* statusStr);

    void _drawIndexColumn(int colX, const IndexData& idx);

    void _drawTableHeader(bool showW52, bool showHL);

    void _drawTickerRow(int y, const TickerData& td, bool showW52, bool showHL);

    void _drawFooter(time_t fetchedAt, const char* note = nullptr);

    // Format helpers
    static String _fmtPrice(float v);
    static String _fmtRange(float hi, float lo);
    static String _fmtW52Bar(float price, float hi, float lo);
    static String _fmtTime(time_t t);
    static String _fmtDate(time_t t);

    // Right-align text within a box ending at xRight
    void _printRight(const String& s, int xRight, int y);

    // Center text within [xLeft, xRight]
    void _printCenter(const String& s, int xLeft, int xRight, int y);
};
