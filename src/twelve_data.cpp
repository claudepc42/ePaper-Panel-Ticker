#include "twelve_data.h"
#include "finnhub.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

void TwelveDataProvider::setApiKey(const char* key) {
    strlcpy(_apiKey, key, sizeof(_apiKey));
}

String TwelveDataProvider::_get(const String& path, int* httpCode) {
    WiFiClientSecure client;
    client.setInsecure();   // TODO: add cert pinning for production hardening

    HTTPClient http;
    String url = String("https://") + TWELVE_DATA_HOST + path;
    http.begin(client, url);
    http.setTimeout(HTTP_TIMEOUT_MS);

    *httpCode = http.GET();
    String body = "";
    if (*httpCode > 0) {
        body = http.getString();
    }
    http.end();
    return body;
}

FetchResult TwelveDataProvider::_parseQuote(const String& body, TickerData& td) {
    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        return FetchResult::PARSE_ERROR;
    }

    // Twelve Data returns {"code":400,...} or {"status":"error",...} on errors
    if (!doc["status"].isNull() && String(doc["status"] | "").equals("error")) {
        const char* msg = doc["message"] | "";
        if (strstr(msg, "You have run out of API credits")) {
            return FetchResult::RATE_LIMITED;
        }
        return FetchResult::AUTH_ERROR;
    }

    const char* sym = doc["symbol"] | "";
    if (sym[0]) strlcpy(td.symbol, sym, sizeof(td.symbol));  // preserve caller-set symbol if field absent
    strlcpy(td.name, doc["name"] | "", sizeof(td.name));

    td.price     = doc["close"]  | 0.0f;
    td.change    = doc["change"] | 0.0f;
    td.changePct = doc["percent_change"] | 0.0f;

    // PRD §11 item 3: these fields may not be on the free tier
    if (!doc["high"].isNull() && !doc["low"].isNull()) {
        td.dayHigh   = doc["high"] | 0.0f;
        td.dayLow    = doc["low"]  | 0.0f;
        td.hasDayHiLo = true;
    } else {
        td.hasDayHiLo = false;
    }

    if (!doc["fifty_two_week"].isNull()) {
        JsonObject w52 = doc["fifty_two_week"];
        td.week52High = w52["high"] | 0.0f;
        td.week52Low  = w52["low"]  | 0.0f;
        td.hasWeek52  = true;
    } else {
        td.hasWeek52 = false;
    }

    td.valid = (td.price != 0.0f);
    return FetchResult::OK;
}

FetchResult TwelveDataProvider::fetchTicker(const char* symbol, TickerData& td) {
    // /quote returns price, change, high, low and optionally 52w range
    String path = String("/quote?symbol=") + symbol
                + "&apikey=" + _apiKey
                + "&dp=2";

    int code;
    String body = _get(path, &code);

    if (code == 429) return FetchResult::RATE_LIMITED;
    if (code == 401 || code == 403) return FetchResult::AUTH_ERROR;
    if (code <= 0 || body.isEmpty()) return FetchResult::NETWORK_ERROR;

    strlcpy(td.symbol, symbol, sizeof(td.symbol));
    return _parseQuote(body, td);
}

FetchResult TwelveDataProvider::fetchIndexSummary(IndexData idx[3]) {
    // PRD §6.1: SPX, NASDAQ (COMP), DOW (DJI)
    // On Twelve Data free tier, indices may require a specific plan.
    // We attempt it and mark invalid if unavailable.
    const char* symbols[] = { "SPX", "COMP", "DJI" };
    const char* names[]   = { "S&P 500", "NASDAQ COMP.", "DOW JONES" };

    FetchResult worstResult = FetchResult::OK;

    for (int i = 0; i < 3; i++) {
        strlcpy(idx[i].symbol, symbols[i], sizeof(idx[i].symbol));
        strlcpy(idx[i].name,   names[i],   sizeof(idx[i].name));
        idx[i].valid = false;

        String path = String("/quote?symbol=") + symbols[i]
                    + "&apikey=" + _apiKey + "&dp=2";
        int code;
        String body = _get(path, &code);

        if (code == 429) { worstResult = FetchResult::RATE_LIMITED; continue; }
        if (code <= 0 || body.isEmpty()) { worstResult = FetchResult::NETWORK_ERROR; continue; }

        JsonDocument doc;
        if (deserializeJson(doc, body) != DeserializationError::Ok) continue;
        if (!doc["status"].isNull() && !String(doc["status"] | "ok").equals("ok")) continue;

        idx[i].price     = doc["close"]           | 0.0f;
        idx[i].change    = doc["change"]           | 0.0f;
        idx[i].changePct = doc["percent_change"]   | 0.0f;
        idx[i].dayHigh   = doc["high"]             | 0.0f;
        idx[i].dayLow    = doc["low"]              | 0.0f;
        idx[i].valid     = (idx[i].price != 0.0f);
    }

    return worstResult;
}

IDataProvider* createProvider(DataProvider which) {
    switch (which) {
        default:
        case DataProvider::TWELVE_DATA: {
            return new TwelveDataProvider();
        }
        case DataProvider::FINNHUB:
            return new FinnhubProvider();
    }
}
