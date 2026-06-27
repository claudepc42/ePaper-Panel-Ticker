#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "types.h"
#include "nvs_manager.h"
#include "wifi_manager.h"
#include "data_provider.h"
#include "display_factory.h"
#include "display_hal.h"
#include "portal.h"
#include "power_manager.h"

// ── Singletons ────────────────────────────────────────────────────────────────
static NVSManager  nvs;
static WiFiManager wifiMgr;

// ── BOOT-hold detection ───────────────────────────────────────────────────────
// Returns true if BOOT button (GPIO9 on C3, GPIO0 on S3) is held for
// BOOT_HOLD_MS at startup. Checked before anything else (PRD §2.4).
static bool bootHeld() {
#if defined(BOARD_BUILD_A)
    const int BOOT_PIN = 9;          // GPIO9 = BOOT on XIAO ESP32-C3
#elif defined(BOARD_BUILD_B)
    const int BOOT_PIN = BTN_GREEN;  // green button doubles as config-hold on E1001
#else
    const int BOOT_PIN = 0;
#endif
    pinMode(BOOT_PIN, INPUT_PULLUP);
    if (digitalRead(BOOT_PIN) != LOW) return false;

    unsigned long start = millis();
    while (digitalRead(BOOT_PIN) == LOW) {
        if (millis() - start >= BOOT_HOLD_MS) return true;
        delay(10);
    }
    return false;  // released before threshold
}

// ── Config Mode (captive portal) ──────────────────────────────────────────────
static void runConfigMode(DeviceConfig& cfg, bool firstBoot) {
    Serial.println("[Config] createDisplay()");
    IDisplay* display = createDisplay(cfg.boardTarget, cfg.panelVariant);
    Serial.println("[Config] display->init()");
    display->init();
    Serial.println("[Config] display init done");

    Serial.println("[Config] startAP()");
    wifiMgr.startAP();
    String apSsid = wifiMgr.getAPSsid();
    String apIP   = wifiMgr.getAPIP();

    if (firstBoot) {
        Serial.println("[Config] drawSetupRequired()");
        display->drawSetupRequired(apSsid.c_str(), apIP.c_str());
        Serial.println("[Config] drawSetupRequired done");
    }
    // (If not first-boot, the display keeps its last content while portal runs.)

    CaptivePortal portal(nvs, wifiMgr);
    portal.begin();

    Serial.printf("[Config] AP: %s  IP: %s\n", apSsid.c_str(), apIP.c_str());

    while (!portal.didSave()) {
        portal.loop();
        yield();
    }

    wifiMgr.stopAP();
    delete display;
    ESP.restart();
}

// ── Build 5-row display set ───────────────────────────────────────────────────
// Positions 1–5 (cfg index 0–4): STATIC = always show assigned ticker;
//                                 ROTATING = draw from rotation bank.
// Positions 6+ (cfg index 5+):   always ROTATING, always in the bank.
// Rotation bank cycles sequentially each refresh with no intra-refresh duplicates.
// nextRotPos is written so the caller can persist it to NVS.
static uint8_t buildDisplaySet(const DeviceConfig& cfg,
                                const CachedTickerSet& freshCache,
                                TickerData displaySet[5],
                                uint8_t rotPos,
                                uint8_t* nextRotPos)
{
    // Locate ticker data by symbol in the fresh cache
    auto findInCache = [&](const char* sym, TickerData& out) -> bool {
        for (uint8_t i = 0; i < freshCache.count; i++) {
            if (strcmp(freshCache.tickers[i].symbol, sym) == 0) {
                out = freshCache.tickers[i];
                return true;
            }
        }
        return false;
    };

    // Build bank: cfg[0-4] marked ROTATING + all cfg[5+]
    TickerData bank[MAX_TICKERS];
    uint8_t    bankSize = 0;
    for (uint8_t i = 0; i < cfg.tickerCount && bankSize < MAX_TICKERS; i++) {
        if (i >= 5 || cfg.tickers[i].mode == TickerMode::ROTATING) {
            TickerData td = {};
            strlcpy(td.symbol, cfg.tickers[i].symbol, sizeof(td.symbol));
            findInCache(cfg.tickers[i].symbol, td);
            bank[bankSize++] = td;
        }
    }

    // Fill up to 5 display rows
    uint8_t rowCount    = 0;
    uint8_t bankStart   = (bankSize > 0) ? (rotPos % bankSize) : 0;
    uint8_t bankUsed    = 0;
    uint8_t visibleSlots = min(cfg.tickerCount, (uint8_t)5);

    for (uint8_t i = 0; i < visibleSlots; i++) {
        if (cfg.tickers[i].mode == TickerMode::STATIC) {
            TickerData td = {};
            strlcpy(td.symbol, cfg.tickers[i].symbol, sizeof(td.symbol));
            findInCache(cfg.tickers[i].symbol, td);
            displaySet[rowCount++] = td;
        } else {
            // ROTATING slot: take next unique bank ticker; skip row if bank exhausted
            if (bankUsed < bankSize) {
                displaySet[rowCount++] = bank[(bankStart + bankUsed) % bankSize];
                bankUsed++;
            }
        }
    }

    // Advance by slots consumed, starting from the already-reduced bankStart so the
    // stored value stays bounded in [0, bankSize) and never overflows the uint8_t.
    *nextRotPos = (bankSize > 0)
                  ? (bankStart + (bankUsed > 0 ? bankUsed : 1)) % bankSize
                  : 0;

    return rowCount;
}

// ── Button navigation helpers (Board B) ──────────────────────────────────────
#ifdef BOARD_BUILD_B

// Total tickers in the rotation bank and how many rotating slots appear in the
// top-5 visible positions (bankUsed). Needed to compute step sizes for LEFT/RIGHT.
static void calcBankParams(const DeviceConfig& cfg, uint8_t* bankSize, uint8_t* bankUsed) {
    *bankSize = 0;
    for (uint8_t i = 0; i < cfg.tickerCount; i++) {
        if (i >= 5 || cfg.tickers[i].mode == TickerMode::ROTATING) (*bankSize)++;
    }
    *bankUsed = 0;
    uint8_t slots = min(cfg.tickerCount, (uint8_t)5);
    for (uint8_t i = 0; i < slots; i++) {
        if (cfg.tickers[i].mode == TickerMode::ROTATING) (*bankUsed)++;
    }
}

// Render the ticker set starting at renderPos from the NVS cache (no WiFi fetch).
// Advances and persists rotPos via NVS. Returns false if the cache is empty.
static bool renderCachedAtPos(IDisplay* display, const DeviceConfig& cfg,
                               uint8_t renderPos) {
    CachedTickerSet cache;
    nvs.loadCache(cache);
    if (cache.count == 0) return false;

    TickerData displaySet[5] = {};
    uint8_t    nextRotPos    = 0;
    uint8_t    count = buildDisplaySet(cfg, cache, displaySet, renderPos, &nextRotPos);

    IndexData indices[3] = {};  // no live index data available from cache
    display->drawMarketOverview(indices, displaySet, count, cache.fetchedAt);
    nvs.setRotPosition(nextRotPos);
    return true;
}

#endif  // BOARD_BUILD_B

// ── Fetch all tickers ─────────────────────────────────────────────────────────
// Returns true if at least one ticker fetched successfully.
// On 429, sets *rateLimited = true.
static bool fetchAllTickers(IDataProvider* provider, const DeviceConfig& cfg,
                             CachedTickerSet& cache, bool* rateLimited) {
    *rateLimited = false;
    bool anyOk = false;
    cache.count = 0;

    for (uint8_t i = 0; i < cfg.tickerCount && i < MAX_TICKERS; i++) {
        TickerData td = {};
        strlcpy(td.symbol, cfg.tickers[i].symbol, sizeof(td.symbol));

        FetchResult r = provider->fetchTicker(cfg.tickers[i].symbol, td);

        if (r == FetchResult::RATE_LIMITED) {
            *rateLimited = true;
            Serial.printf("[Fetch] %s: rate limited\n", cfg.tickers[i].symbol);
            continue;
        }
        if (r != FetchResult::OK) {
            Serial.printf("[Fetch] %s: error %d\n", cfg.tickers[i].symbol, (int)r);
            continue;
        }

        FetchResult hr = provider->fetchTimeSeries(cfg.tickers[i].symbol,
                                                   td.history, 15, "2h");
        td.hasHistory = (hr == FetchResult::OK);
        if (!td.hasHistory) {
            Serial.printf("[Fetch] %s: history error %d\n", cfg.tickers[i].symbol, (int)hr);
        }

        cache.tickers[cache.count++] = td;
        anyOk = true;
        Serial.printf("[Fetch] %s: $%.2f  %+.2f%%  hist:%s\n",
                      td.symbol, td.price, td.changePct, td.hasHistory ? "ok" : "no");
    }

    return anyOk;
}

// ── Normal Mode ───────────────────────────────────────────────────────────────
// BATTERY_SAVER: enterSleep() calls esp_deep_sleep_start() and never returns.
//   The next cycle begins via a full reset into setup().
// QUICK_WAKE:    enterSleep() calls esp_light_sleep_start() which returns after
//   the interval. We loop so the next fetch cycle runs without re-entering setup()
//   (RAM state preserved, display HAL kept alive across cycles).
static void runNormalMode(DeviceConfig& cfg) {
    IDisplay* display = createDisplay(cfg.boardTarget, cfg.panelVariant);
    display->init();

#ifdef BOARD_BUILD_B
    // Enable pull-ups on all three physical buttons.
    pinMode(BTN_GREEN, INPUT_PULLUP);
    pinMode(BTN_LEFT,  INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);

    // Handle deep-sleep wakeup from a navigation button (left or right).
    // Render the cached data at the requested position and go back to sleep;
    // skip WiFi and fetching entirely so the response feels instant.
    // A green-button wakeup falls through and triggers a normal fetch cycle.
    if (cfg.powerMode == PowerMode::BATTERY_SAVER &&
        esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) {
        uint64_t pins = esp_sleep_get_ext1_wakeup_status();
        bool leftWoke  = (pins & (1ULL << BTN_LEFT))  != 0;
        bool rightWoke = (pins & (1ULL << BTN_RIGHT)) != 0;

        if (leftWoke || rightWoke) {
            uint8_t bSize = 0, bUsed = 0;
            calcBankParams(cfg, &bSize, &bUsed);
            if (bSize > 0 && bUsed > 0) {
                uint8_t curPos = nvs.getRotPosition();
                uint8_t renderPos;
                if (rightWoke) {
                    // curPos is the position queued for the next fetch cycle —
                    // that is exactly the set to show for a "next" navigation.
                    renderPos = curPos;
                } else {
                    // curPos is 1 set ahead of what is currently on screen.
                    // To show the previous set, step back 2 sets from curPos.
                    int offset = (2 * (int)bUsed) % (int)bSize;
                    renderPos  = (uint8_t)(((int)curPos - offset + (int)bSize * 2) % (int)bSize);
                }
                renderCachedAtPos(display, cfg, renderPos);
            }
            Serial.printf("[Button] %s — rendered from cache, re-sleeping %d min\n",
                          rightWoke ? "RIGHT" : "LEFT", cfg.refIntervalMin);
            enterSleep(cfg.powerMode, cfg.refIntervalMin);
            // Does not return (deep sleep).
        }
        // Green button: fall through to the normal fetch cycle below.
        Serial.println("[Button] GREEN — forcing refresh cycle");
    }
#endif

    while (true) {
        uint16_t sleepMins = cfg.refIntervalMin;

        // ── Connect WiFi ─────────────────────────────────────────────────────
        Serial.printf("[WiFi] Connecting to %s...\n", cfg.wifiSSID);
        WiFiResult wifiResult = wifiMgr.connect(cfg.wifiSSID, cfg.wifiPass);

        if (wifiResult != WiFiResult::CONNECTED) {
            Serial.println("[WiFi] Connection failed");
            nvs.incrementFailCount();

            CachedTickerSet cache;
            nvs.loadCache(cache);
            display->drawWifiUnavailable(cache);

            uint8_t failCount = nvs.getFailCount();
            sleepMins = WiFiManager::backoffMinutes(failCount, cfg.refIntervalMin);
            Serial.printf("[WiFi] Fail #%d — sleeping %d min\n", failCount, sleepMins);

        } else {
            Serial.println("[WiFi] Connected");
            nvs.resetFailCount();

            // Sync time via NTP, set US Eastern timezone (EST5EDT with DST rules)
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
            setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
            tzset();
            struct tm tmInfo;
            int ntpTries = 0;
            while (!getLocalTime(&tmInfo) && ntpTries++ < 10) delay(200);

            // Fetch
            IDataProvider* provider = createProvider(cfg.dataProvider);
            provider->setApiKey(cfg.apiKey);

            IndexData indices[3] = {};
            provider->fetchIndexSummary(indices);

            CachedTickerSet freshCache = {};
            bool rateLimited = false;
            bool anyOk = fetchAllTickers(provider, cfg, freshCache, &rateLimited);

            if (anyOk) {
                freshCache.fetchedAt = time(nullptr);
                nvs.saveCache(freshCache);
            }

            delete provider;
            // Quick Wake: soft-disconnect so the radio stays alive for modem sleep
            // during light sleep (enterSleep configures WIFI_PS_MAX_MODEM).
            // Battery Saver: full radio off — deep sleep powers everything down anyway.
            if (cfg.powerMode == PowerMode::QUICK_WAKE) {
                WiFi.disconnect(false);  // drop association, keep radio on
            } else {
                wifiMgr.disconnect();    // full radio off
            }

            // Render
            if (rateLimited && !anyOk) {
                CachedTickerSet oldCache;
                nvs.loadCache(oldCache);
                display->drawRateLimited(oldCache);
            } else {
                uint8_t nextRotPos = 0;
                TickerData displaySet[5] = {};
                uint8_t displayCount = buildDisplaySet(
                    cfg, freshCache, displaySet, nvs.getRotPosition(), &nextRotPos);
                nvs.setRotPosition(nextRotPos);

                display->drawMarketOverview(indices, displaySet, displayCount,
                                            freshCache.fetchedAt);
            }
        }

        // ── Sleep ─────────────────────────────────────────────────────────────
        Serial.printf("[Sleep] %d min\n", sleepMins);
        enterSleep(cfg.powerMode, sleepMins);
        // BATTERY_SAVER: never reaches here (deep sleep = full reset).
        // QUICK_WAKE:    continues here after light sleep, loops for next cycle.

#ifdef BOARD_BUILD_B
        // If a navigation button woke us from light sleep, render from cache
        // and go back to sleep without doing a WiFi fetch.
        if (cfg.powerMode == PowerMode::QUICK_WAKE &&
            esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
            delay(30);  // debounce
            bool leftPressed  = (digitalRead(BTN_LEFT)  == LOW);
            bool rightPressed = (digitalRead(BTN_RIGHT) == LOW);
            if (leftPressed || rightPressed) {
                uint8_t bSize = 0, bUsed = 0;
                calcBankParams(cfg, &bSize, &bUsed);
                if (bSize > 0 && bUsed > 0) {
                    uint8_t curPos = nvs.getRotPosition();
                    uint8_t renderPos;
                    if (rightPressed) {
                        renderPos = curPos;
                    } else {
                        int offset = (2 * (int)bUsed) % (int)bSize;
                        renderPos  = (uint8_t)(((int)curPos - offset + (int)bSize * 2) % (int)bSize);
                    }
                    renderCachedAtPos(display, cfg, renderPos);
                }
                Serial.printf("[Button] %s — rendered from cache, re-sleeping %d min\n",
                              rightPressed ? "RIGHT" : "LEFT", cfg.refIntervalMin);
                enterSleep(cfg.powerMode, cfg.refIntervalMin);
                // Returns here after the follow-up light sleep; continues to display->init() below.
            }
            // Green button or unrecognised GPIO: fall through to normal fetch cycle.
        }
#endif

        // Reinit display and reload config for next Quick Wake cycle.
        // Note: boardTarget/panelVariant changes from a mid-session reconfigure won't
        // take effect here — those require a restart (which reconfigure always triggers).
        display->init();
        nvs.loadConfig(cfg);
    }
    // Unreachable, but keeps the compiler happy about display lifetime
    delete display;
}

// ── Arduino entry points ──────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n[Boot] ePaper Stock Ticker starting");

    // ── BOOT-hold check (PRD §2.4) ────────────────────────────────────────
    // On Board B, skip the hold check when waking from a deep-sleep button press.
    // The green button hold for config is only meaningful on a fresh power-on, not
    // on a navigation wake where the user may still have the button briefly depressed.
#ifdef BOARD_BUILD_B
    bool forceConfig = (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT1)
                       && bootHeld();
#else
    bool forceConfig = bootHeld();
#endif

    // ── Load config ───────────────────────────────────────────────────────
    DeviceConfig cfg = {};
    // Set safe defaults before loading
    cfg.boardTarget    = BoardTarget::XIAO_PANEL;
    cfg.panelVariant   = PanelVariant::PANEL_A;
    cfg.dataProvider   = DataProvider::TWELVE_DATA;
    cfg.rotIntervalMin = DEFAULT_ROT_INTERVAL_MIN;
    cfg.refIntervalMin = DEFAULT_REF_INTERVAL_MIN;
    cfg.powerMode      = PowerMode::BATTERY_SAVER;
    nvs.loadConfig(cfg);

    // ── Determine operating mode (PRD §3) ─────────────────────────────────
    bool configured = nvs.isConfigured();

    if (!configured) {
        // PRD §3.3: First-Boot Mode — auto-enter portal without button press
        Serial.println("[Boot] First boot — entering Config Mode automatically");
        runConfigMode(cfg, /*firstBoot=*/true);

    } else if (forceConfig) {
        // PRD §3.2: Manual Config Mode via BOOT hold
        Serial.println("[Boot] BOOT held — entering Config Mode");
        runConfigMode(cfg, /*firstBoot=*/false);

    } else {
        // PRD §3.1: Normal Mode
        Serial.println("[Boot] Normal Mode");
        runNormalMode(cfg);
    }
}

void loop() {
    // enterSleep() in runNormalMode() deep-sleeps and never returns to loop().
    // Quick-wake light sleep also enters via enterSleep() and re-runs setup().
    // Config mode loops inside runConfigMode() until saved, then restarts.
    // This loop body is never reached in normal operation.
    delay(1000);
}
