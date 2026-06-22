#include "renderer_mono.h"
#include <GxEPD2_BW.h>

// Adafruit GFX fonts bundled with the GxEPD2/Adafruit-GFX library
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// 4-tone palette aliases matching design (#0d0d0d / #595959 / #a8a8a8 / #f0ece3)
static constexpr uint16_t INK   = GxEPD_BLACK;
static constexpr uint16_t DARK  = GxEPD_DARKGREY;
static constexpr uint16_t MID   = GxEPD_LIGHTGREY;
static constexpr uint16_t PAPER = GxEPD_WHITE;

MonoRenderer::MonoRenderer(GxEPD2_GFX* gfx) : _gfx(gfx) {}

// ── Format helpers ─────────────────────────────────────────────────────────────

String MonoRenderer::_fmtPrice(float v) {
    char buf[16];
    if (v >= 10000.0f)      snprintf(buf, sizeof(buf), "%.0f", v);
    else if (v >= 1000.0f)  snprintf(buf, sizeof(buf), "%.2f", v);
    else                    snprintf(buf, sizeof(buf), "%.2f", v);
    return String("$") + buf;
}

String MonoRenderer::_fmtRange(float hi, float lo) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f / %.2f", hi, lo);
    return String(buf);
}

void MonoRenderer::_drawSparkBars(int x, int y, int w, int h,
                                   const float* vals, uint8_t count, uint16_t color) {
    if (count == 0 || w <= 0 || h <= 0) return;

    float mn = vals[0], mx = vals[0];
    for (uint8_t i = 1; i < count; i++) {
        if (vals[i] < mn) mn = vals[i];
        if (vals[i] > mx) mx = vals[i];
    }
    float rng = mx - mn;
    if (rng == 0.0f) rng = 1.0f;

    // Equal bar widths with 1px gaps. Distribute remainder to leading bars.
    int barW    = (w - (int)(count - 1)) / (int)count;
    if (barW < 1) barW = 1;
    int leftover = w - (barW * (int)count + (int)(count - 1));

    int bx = x;
    for (uint8_t i = 0; i < count; i++) {
        int thisW = barW + (i < leftover ? 1 : 0);
        float pct = ((vals[i] - mn) / rng) * 87.5f + 12.5f;
        int barH  = (int)(pct / 100.0f * (float)h + 0.5f);
        if (barH < 1) barH = 1;
        if (barH > h) barH = h;
        _gfx->fillRect(bx, y + h - barH, thisW, barH, color);
        bx += thisW + 1;
    }
}

String MonoRenderer::_fmtTime(time_t t) {
    if (t == 0) return String("--:-- ET");
    struct tm* tm = localtime(&t);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d ET", tm->tm_hour, tm->tm_min);
    return String(buf);
}

String MonoRenderer::_fmtDate(time_t t) {
    if (t == 0) return String("---");
    struct tm* tm = localtime(&t);
    const char* days[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    const char* mons[] = {"JAN","FEB","MAR","APR","MAY","JUN",
                          "JUL","AUG","SEP","OCT","NOV","DEC"};
    char buf[24];
    snprintf(buf, sizeof(buf), "%s %s %d", days[tm->tm_wday], mons[tm->tm_mon], tm->tm_mday);
    return String(buf);
}

// ── Text alignment helpers ────────────────────────────────────────────────────

void MonoRenderer::_printRight(const String& s, int xRight, int y) {
    int16_t x1, y1; uint16_t tw, th;
    _gfx->getTextBounds(s.c_str(), 0, y, &x1, &y1, &tw, &th);
    _gfx->setCursor(xRight - (int)tw, y);
    _gfx->print(s);
}

void MonoRenderer::_printCenter(const String& s, int xLeft, int xRight, int y) {
    int16_t x1, y1; uint16_t tw, th;
    _gfx->getTextBounds(s.c_str(), 0, y, &x1, &y1, &tw, &th);
    _gfx->setCursor(xLeft + ((xRight - xLeft) - (int)tw) / 2, y);
    _gfx->print(s);
}

// ── Shared header ─────────────────────────────────────────────────────────────

void MonoRenderer::_drawSharedHeader(const char* title, const char* dateStr,
                                     const char* bullet, const char* statusStr) {
    _gfx->setFont(&FreeMonoBold9pt7b);

    // Title (left)
    _gfx->setTextColor(INK);
    _gfx->setCursor(MX, HDR_Y + 13);
    _gfx->print(title);

    // Date (centre)
    _gfx->setTextColor(MID);
    _printCenter(String(dateStr), 250, 550, HDR_Y + 13);

    // Bullet + status (right)
    _gfx->setTextColor(INK);
    String bStr(bullet);
    int statusX = W - MX - 200;
    _gfx->setCursor(statusX, HDR_Y + 13);
    _gfx->print(bStr);
    _gfx->print(" ");
    _gfx->print(statusStr);

    // Separator
    _gfx->drawLine(MX, HDR_LINE_Y, W - MX, HDR_LINE_Y, INK);
    _gfx->drawLine(MX, HDR_LINE_Y + 1, W - MX, HDR_LINE_Y + 1, INK);
}

// ── Index column ──────────────────────────────────────────────────────────────

void MonoRenderer::_drawIndexColumn(int colX, const IndexData& idx) {
    if (!idx.valid) {
        _gfx->setFont(&FreeMonoBold9pt7b);
        _gfx->setTextColor(MID);
        _gfx->setCursor(colX + 4, IDX_Y + 60);
        _gfx->print(idx.symbol);
        _gfx->print(" N/A");
        return;
    }

    int x = colX + 4;

    // Sub-label (e.g. "SPX")
    _gfx->setFont(&FreeMonoBold9pt7b);
    _gfx->setTextColor(MID);
    _gfx->setCursor(x, IDX_Y + 14);
    _gfx->print(idx.symbol);

    // Fund name (e.g. "S&P 500")
    _gfx->setFont(&FreeSansBold9pt7b);
    _gfx->setTextColor(DARK);
    _gfx->setCursor(x, IDX_Y + 30);
    _gfx->print(idx.name);

    // Large price
    _gfx->setFont(&FreeMonoBold24pt7b);
    _gfx->setTextColor(INK);
    char priceBuf[16];
    if (idx.price >= 10000) snprintf(priceBuf, sizeof(priceBuf), "%.0f", idx.price);
    else                    snprintf(priceBuf, sizeof(priceBuf), "%.2f", idx.price);
    _gfx->setCursor(x, IDX_Y + 72);
    _gfx->print(priceBuf);

    // Change line
    _gfx->setFont(&FreeMonoBold12pt7b);
    _gfx->setTextColor(INK);
    char chgBuf[24];
    const char* arr = (idx.change >= 0) ? "+" : "";
    snprintf(chgBuf, sizeof(chgBuf), "%s%.2f  %+.2f%%", arr, idx.change, idx.changePct);
    _gfx->setCursor(x, IDX_Y + 93);
    _gfx->print(chgBuf);

    // Rolling 6-hour bar chart: "H xxxx [bars] L xxxx  6hrs"
    if (idx.hasHistory) {
        float mn = idx.history[0], mx = idx.history[0];
        for (int j = 1; j < 6; j++) {
            if (idx.history[j] < mn) mn = idx.history[j];
            if (idx.history[j] > mx) mx = idx.history[j];
        }

        char hBuf[12], lBuf[12];
        if (mx >= 1000.0f) snprintf(hBuf, sizeof(hBuf), "H %.0f", mx);
        else                snprintf(hBuf, sizeof(hBuf), "H %.2f", mx);
        if (mn >= 1000.0f) snprintf(lBuf, sizeof(lBuf), "L %.0f", mn);
        else                snprintf(lBuf, sizeof(lBuf), "L %.2f", mn);

        _gfx->setFont(&FreeMonoBold9pt7b);
        _gfx->setTextColor(MID);

        // Bar row: 9px tall. Text baseline aligns with bar bottom.
        int baseline = IDX_Y + 110;
        int barTop   = baseline - 9;
        int cx = x;

        _gfx->setCursor(cx, baseline);
        _gfx->print(hBuf);
        int16_t tx, ty; uint16_t tw, th;
        _gfx->getTextBounds(hBuf, cx, baseline, &tx, &ty, &tw, &th);
        cx += (int)tw + 3;

        _drawSparkBars(cx, barTop, 60, 9, idx.history, 6, MID);
        cx += 60 + 3;

        _gfx->setCursor(cx, baseline);
        _gfx->print(lBuf);
        _gfx->getTextBounds(lBuf, cx, baseline, &tx, &ty, &tw, &th);
        cx += (int)tw + 3;

        _gfx->setCursor(cx, baseline);
        _gfx->print("6hrs");
    }
}

// ── Table header ──────────────────────────────────────────────────────────────

void MonoRenderer::_drawTableHeader(bool showHL) {
    _gfx->setFont(&FreeMonoBold9pt7b);
    _gfx->setTextColor(DARK);
    int y = TBL_Y + 16;

    _gfx->setCursor(COL_TICK,  y); _gfx->print("TICKER");
    _gfx->setCursor(COL_FUND,  y); _gfx->print("FUND");
    _printRight("PRICE",        COL_PRICE + 94, y);
    _printRight("DAY CHG",      COL_CHG   + 86, y);
    if (showHL) _printCenter("DAY H / L",   COL_HL,    COL_HL    + 162, y);
    _printCenter("15 DAY TREND", COL_TREND, COL_TREND + 146, y);

    _gfx->drawLine(MX, TBL_LINE_Y, W - MX, TBL_LINE_Y, INK);
    _gfx->drawLine(MX, TBL_LINE_Y + 1, W - MX, TBL_LINE_Y + 1, INK);
}

// ── Ticker data row ───────────────────────────────────────────────────────────

void MonoRenderer::_drawTickerRow(int y, const TickerData& td, bool showHL) {
    int baseline = y + ROW_H - 16;   // text baseline within row

    if (!td.valid) {
        _gfx->setFont(&FreeMonoBold9pt7b);
        _gfx->setTextColor(MID);
        _gfx->setCursor(COL_TICK, baseline);
        _gfx->print(td.symbol);
        _gfx->print(" --");
        _gfx->drawLine(MX, y + ROW_H - 1, W - MX, y + ROW_H - 1, MID);
        return;
    }

    // Ticker symbol (large)
    _gfx->setFont(&FreeMonoBold18pt7b);
    _gfx->setTextColor(INK);
    _gfx->setCursor(COL_TICK, baseline);
    _gfx->print(td.symbol);

    // Fund name
    _gfx->setFont(&FreeSansBold9pt7b);
    _gfx->setTextColor(DARK);
    // Clip name to column width with simple truncation
    String name(td.name);
    if (name.length() > 18) name = name.substring(0, 17) + ".";
    _gfx->setCursor(COL_FUND, baseline);
    _gfx->print(name);

    // Price
    _gfx->setFont(&FreeMonoBold9pt7b);
    _gfx->setTextColor(INK);
    _printRight(_fmtPrice(td.price), COL_PRICE + 94, baseline);

    // Day change
    char chgBuf[20];
    snprintf(chgBuf, sizeof(chgBuf), "%+.2f%%", td.changePct);
    _printRight(String(chgBuf), COL_CHG + 86, baseline);

    // Day H/L
    if (showHL && td.hasDayHiLo) {
        _gfx->setTextColor(DARK);
        _printCenter(_fmtRange(td.dayHigh, td.dayLow), COL_HL, COL_HL + 162, baseline);
    }

    // 15-day sparkline: 146px wide × 16px tall, vertically centered in row
    if (td.hasHistory) {
        int barY = y + (ROW_H - 16) / 2;
        _drawSparkBars(COL_TREND, barY, 146, 16, td.history, 15, INK);
    }

    _gfx->drawLine(MX, y + ROW_H - 1, W - MX, y + ROW_H - 1, MID);
}

// ── Footer ────────────────────────────────────────────────────────────────────

void MonoRenderer::_drawFooter(time_t fetchedAt, const char* note) {
    _gfx->drawLine(MX, FTR_Y, W - MX, FTR_Y, MID);
    _gfx->setFont(&FreeMonoBold9pt7b);
    _gfx->setTextColor(MID);

    String left = note ? String(note) : String("LAST UPD ") + _fmtTime(fetchedAt);
    _gfx->setCursor(MX, FTR_Y + 14);
    _gfx->print(left);

    String right = "NOT FINANCIAL ADVICE";
    _printRight(right, W - MX, FTR_Y + 14);
}

// ── Market Overview ───────────────────────────────────────────────────────────

void MonoRenderer::drawMarketOverview(const IndexData indices[3], const TickerData* tickers,
                                      uint8_t tickerCount, time_t fetchedAt) {
    bool showHL = false;
    for (uint8_t i = 0; i < tickerCount; i++) {
        if (tickers[i].hasDayHiLo) showHL = true;
    }

    _gfx->setFullWindow();
    _gfx->firstPage();
    do {
        _gfx->fillScreen(PAPER);

        // Header
        _drawSharedHeader("MARKET OVERVIEW", _fmtDate(fetchedAt).c_str(),
                          "*", (String("NYSE - ") + _fmtTime(fetchedAt)).c_str());

        // Index bar
        _drawIndexColumn(MX,            indices[0]);
        _gfx->drawLine(IDX_DIV1, IDX_Y, IDX_DIV1, IDX_LINE_Y, MID);
        _drawIndexColumn(IDX_DIV1 + 4, indices[1]);
        _gfx->drawLine(IDX_DIV2, IDX_Y, IDX_DIV2, IDX_LINE_Y, MID);
        _drawIndexColumn(IDX_DIV2 + 4, indices[2]);
        _gfx->drawLine(MX, IDX_LINE_Y, W - MX, IDX_LINE_Y, MID);

        // Table
        _drawTableHeader(showHL);

        uint8_t shown = min(tickerCount, (uint8_t)5);
        for (uint8_t i = 0; i < shown; i++) {
            _drawTickerRow(ROWS_TOP + i * ROW_H, tickers[i], showHL);
        }

        _drawFooter(fetchedAt);

    } while (_gfx->nextPage());
}

// ── WiFi Unavailable ──────────────────────────────────────────────────────────

void MonoRenderer::drawWifiUnavailable(const CachedTickerSet& cache) {
    bool hasData = (cache.fetchedAt > 0 && cache.count > 0);

    _gfx->setFullWindow();
    _gfx->firstPage();
    do {
        _gfx->fillScreen(PAPER);

        _drawSharedHeader("ePaperTicker", "---", "!", "WIFI UNAVAILABLE");

        if (!hasData) {
            // Empty state (PRD §6.2 — no cached data)
            _gfx->setFont(&FreeMonoBold18pt7b);
            _gfx->setTextColor(INK);
            _printCenter("[!] WIFI UNAVAILABLE", MX, W - MX, 200);

            _gfx->setFont(&FreeMonoBold9pt7b);
            _gfx->setTextColor(DARK);
            _printCenter("Cannot connect to saved network.", MX, W - MX, 240);
            _printCenter("Retrying automatically.", MX, W - MX, 260);
            _printCenter("Hold BOOT to reconfigure WiFi.", MX, W - MX, 280);

            _gfx->drawLine(W / 2 - 60, 310, W / 2 + 60, 310, MID);

            _gfx->setTextColor(MID);
            _printCenter("No cached data yet.", MX, W - MX, 345);
            _printCenter("Data appears after first successful fetch.", MX, W - MX, 365);

        } else {
            // Cached data state (PRD §6.2 — dimmed, "may be outdated")
            _gfx->setFont(&FreeMonoBold12pt7b);
            _gfx->setTextColor(INK);
            _gfx->setCursor(MX, IDX_Y + 20);
            _gfx->print("[!] WIFI UNAVAILABLE");

            _gfx->setFont(&FreeMonoBold9pt7b);
            _gfx->setTextColor(DARK);
            _gfx->setCursor(MX, IDX_Y + 38);
            _gfx->print("Cannot connect. Retrying automatically.");
            _gfx->setCursor(MX, IDX_Y + 53);
            _gfx->print("Hold BOOT to reconfigure WiFi.");

            _gfx->drawLine(MX, IDX_Y + 63, W - MX, IDX_Y + 63, MID);

            // "CACHED DATA · LAST UPD..." banner
            _gfx->setTextColor(MID);
            String bannerStr = String("CACHED DATA  |  LAST UPD ")
                             + _fmtTime(cache.fetchedAt) + "  |  MAY BE OUTDATED";
            _printCenter(bannerStr, MX, W - MX, IDX_Y + 80);

            _gfx->drawLine(MX, IDX_Y + 86, W - MX, IDX_Y + 86, MID);

            // Compact cached ticker table (all in MID gray — visually deprioritised)
            int y = IDX_Y + 95;
            for (uint8_t i = 0; i < cache.count && i < 5; i++) {
                const TickerData& td = cache.tickers[i];
                _gfx->setFont(&FreeMonoBold9pt7b);
                _gfx->setTextColor(MID);

                _gfx->setCursor(MX, y + 14);
                _gfx->print(td.symbol);

                _printRight(_fmtPrice(td.price), COL_PRICE + 94, y + 14);

                char chgBuf[20];
                snprintf(chgBuf, sizeof(chgBuf), "%+.2f%%", td.changePct);
                _printRight(String(chgBuf), COL_CHG + 86, y + 14);

                if (td.hasDayHiLo) {
                    _printCenter(_fmtRange(td.dayHigh, td.dayLow), COL_HL, COL_HL + 162, y + 14);
                }

                _gfx->drawLine(MX, y + 20, W - MX, y + 20, MID);
                y += 22;
            }
        }

    } while (_gfx->nextPage());
}

// ── Rate Limited ──────────────────────────────────────────────────────────────

void MonoRenderer::drawRateLimited(const CachedTickerSet& cache) {
    _gfx->setFullWindow();
    _gfx->firstPage();
    do {
        _gfx->fillScreen(PAPER);

        _drawSharedHeader("ePaperTicker", "---", "!", "DATA LIMIT REACHED");

        _gfx->setFont(&FreeMonoBold12pt7b);
        _gfx->setTextColor(INK);
        _gfx->setCursor(MX, IDX_Y + 24);
        _gfx->print("API RATE LIMIT REACHED");

        _gfx->setFont(&FreeMonoBold9pt7b);
        _gfx->setTextColor(DARK);
        _gfx->setCursor(MX, IDX_Y + 44);
        _gfx->print("Free tier limit exceeded. Data will resume when quota resets.");
        _gfx->setCursor(MX, IDX_Y + 60);
        _gfx->print("Reduce ticker count or increase refresh interval (Advanced settings).");
        _gfx->setCursor(MX, IDX_Y + 76);
        _gfx->print("Hold BOOT to reconfigure.");

        if (cache.fetchedAt > 0 && cache.count > 0) {
            _gfx->drawLine(MX, IDX_Y + 86, W - MX, IDX_Y + 86, MID);
            _gfx->setTextColor(MID);
            _printCenter(String("LAST KNOWN DATA  |  ") + _fmtTime(cache.fetchedAt),
                         MX, W - MX, IDX_Y + 103);
            _gfx->drawLine(MX, IDX_Y + 109, W - MX, IDX_Y + 109, MID);

            int y = IDX_Y + 120;
            for (uint8_t i = 0; i < cache.count && i < 5; i++) {
                const TickerData& td = cache.tickers[i];
                _gfx->setFont(&FreeMonoBold9pt7b);
                _gfx->setTextColor(MID);
                _gfx->setCursor(MX, y + 14);
                _gfx->print(td.symbol);
                _printRight(_fmtPrice(td.price), COL_PRICE + 94, y + 14);
                char chgBuf[20];
                snprintf(chgBuf, sizeof(chgBuf), "%+.2f%%", td.changePct);
                _printRight(String(chgBuf), COL_CHG + 86, y + 14);
                _gfx->drawLine(MX, y + 20, W - MX, y + 20, MID);
                y += 22;
            }
        }

    } while (_gfx->nextPage());
}

// ── Setup Required (first boot) ───────────────────────────────────────────────

void MonoRenderer::drawSetupRequired(const char* apSsid, const char* apIP) {
    _gfx->setFullWindow();
    _gfx->firstPage();
    do {
        _gfx->fillScreen(PAPER);

        // Header — no date, no status, just title + mode indicator
        _gfx->setFont(&FreeMonoBold9pt7b);
        _gfx->setTextColor(INK);
        _gfx->setCursor(MX, HDR_Y + 13);
        _gfx->print("ePaperTicker");
        _gfx->setTextColor(MID);
        _printRight("[ ] SETUP MODE", W - MX, HDR_Y + 13);
        _gfx->drawLine(MX, HDR_LINE_Y, W - MX, HDR_LINE_Y, INK);
        _gfx->drawLine(MX, HDR_LINE_Y + 1, W - MX, HDR_LINE_Y + 1, INK);

        // Content (PRD §6.3)
        _gfx->setFont(&FreeMonoBold18pt7b);
        _gfx->setTextColor(INK);
        _gfx->setCursor(MX, HDR_LINE_Y + 40);
        _gfx->print("SETUP REQUIRED");

        _gfx->setFont(&FreeMonoBold9pt7b);
        _gfx->setTextColor(DARK);
        _gfx->setCursor(MX, HDR_LINE_Y + 64);
        _gfx->print("1.");
        _gfx->print("  Connect your phone to this WiFi network:");

        // SSID box
        int boxY = HDR_LINE_Y + 76;
        _gfx->drawRect(MX, boxY, CW, 36, INK);
        _gfx->drawRect(MX + 1, boxY + 1, CW - 2, 34, INK);
        _gfx->setFont(&FreeMonoBold18pt7b);
        _gfx->setTextColor(INK);
        _gfx->setCursor(MX + 14, boxY + 26);
        _gfx->print(apSsid);

        _gfx->setFont(&FreeMonoBold9pt7b);
        _gfx->setTextColor(MID);
        _gfx->setCursor(MX, boxY + 52);
        _gfx->print("No password required.");

        _gfx->setTextColor(DARK);
        _gfx->setCursor(MX, boxY + 80);
        _gfx->print("2.");
        _gfx->print("  A setup page opens automatically on your phone. If not, visit:");

        // IP address — large
        _gfx->setFont(&FreeMonoBold24pt7b);
        _gfx->setTextColor(INK);
        _gfx->setCursor(MX, boxY + 130);
        _gfx->print(apIP);

        _gfx->setFont(&FreeMonoBold9pt7b);
        _gfx->setTextColor(MID);
        _gfx->setCursor(MX, boxY + 150);
        _gfx->print("in your phone's browser.");

        // Footer strip
        _gfx->drawLine(MX, H - 26, W - MX, H - 26, MID);
        _gfx->setTextColor(MID);
        _printCenter("[ ] ACCESS POINT ACTIVE  -  WAITING FOR SETUP CONNECTION",
                     MX, W - MX, H - 12);

    } while (_gfx->nextPage());
}
