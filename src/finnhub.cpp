#include "finnhub.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

void FinnhubProvider::setApiKey(const char* key) {
    strlcpy(_apiKey, key, sizeof(_apiKey));
}

String FinnhubProvider::_get(const String& path, int* httpCode) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String("https://") + FINNHUB_HOST + path;
    http.begin(client, url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("X-Finnhub-Token", _apiKey);

    *httpCode = http.GET();
    String body = "";
    if (*httpCode > 0) {
        body = http.getString();
    }
    http.end();
    return body;
}

FetchResult FinnhubProvider::fetchTicker(const char* symbol, TickerData& td) {
    // Finnhub /quote endpoint: c=current, d=change, dp=change%, h=high, l=low
    String path = String("/api/v1/quote?symbol=") + symbol;
    int code;
    String body = _get(path, &code);

    if (code == 429) return FetchResult::RATE_LIMITED;
    if (code == 401 || code == 403) return FetchResult::AUTH_ERROR;
    if (code <= 0 || body.isEmpty()) return FetchResult::NETWORK_ERROR;

    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        return FetchResult::PARSE_ERROR;
    }

    strlcpy(td.symbol, symbol, sizeof(td.symbol));
    td.name[0] = '\0';  // Finnhub quote doesn't include name; fetch separately if needed

    td.price     = doc["c"]  | 0.0f;
    td.change    = doc["d"]  | 0.0f;
    td.changePct = doc["dp"] | 0.0f;

    float h = doc["h"] | 0.0f;
    float l = doc["l"] | 0.0f;
    if (h != 0.0f && l != 0.0f) {
        td.dayHigh    = h;
        td.dayLow     = l;
        td.hasDayHiLo = true;
    } else {
        td.hasDayHiLo = false;
    }

    // TODO (PRD §11 item 7): Finnhub free tier does not appear to include 52w range
    // in the /quote endpoint. Leaving as not-available; verify and add a second call
    // to /stock/metric if it turns out to be accessible.
    td.hasWeek52 = false;

    td.valid = (td.price != 0.0f);
    return td.valid ? FetchResult::OK : FetchResult::PARSE_ERROR;
}

FetchResult FinnhubProvider::fetchIndexSummary(IndexData idx[3]) {
    // Finnhub uses different symbols for indices: ^GSPC, ^IXIC, ^DJI
    const char* symbols[] = { "^GSPC", "^IXIC", "^DJI" };
    const char* names[]   = { "S&P 500", "NASDAQ COMP.", "DOW JONES" };
    const char* subLabels[]= { "SPX",    "COMP",          "DJIA" };

    FetchResult worst = FetchResult::OK;

    for (int i = 0; i < 3; i++) {
        strlcpy(idx[i].symbol, subLabels[i], sizeof(idx[i].symbol));
        strlcpy(idx[i].name,   names[i],     sizeof(idx[i].name));
        idx[i].valid = false;

        String path = String("/api/v1/quote?symbol=") + symbols[i];
        int code;
        String body = _get(path, &code);

        if (code == 429) { worst = FetchResult::RATE_LIMITED; continue; }
        if (code <= 0 || body.isEmpty()) { worst = FetchResult::NETWORK_ERROR; continue; }

        JsonDocument doc;
        if (deserializeJson(doc, body) != DeserializationError::Ok) continue;

        idx[i].price     = doc["c"]  | 0.0f;
        idx[i].change    = doc["d"]  | 0.0f;
        idx[i].changePct = doc["dp"] | 0.0f;
        idx[i].dayHigh   = doc["h"]  | 0.0f;
        idx[i].dayLow    = doc["l"]  | 0.0f;
        idx[i].valid     = (idx[i].price != 0.0f);
    }

    return worst;
}

