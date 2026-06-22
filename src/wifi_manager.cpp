#include "wifi_manager.h"
#include <WiFi.h>
#include <ArduinoJson.h>

WiFiResult WiFiManager::connect(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            WiFi.disconnect(true);
            return WiFiResult::TIMEOUT;
        }
        delay(100);
    }
    return WiFiResult::CONNECTED;
}

void WiFiManager::disconnect() {
    WiFi.disconnect(true);
}

void WiFiManager::startAP() {
    // Build SSID from last 2 bytes of MAC for uniqueness
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[8];
    snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
    _apSsid = String(AP_SSID_BASE) + suffix;

    // AP+STA mode so we can test credentials while serving the portal
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(_apSsid.c_str(), AP_PASSWORD);
    delay(100);
}

void WiFiManager::stopAP() {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
}

String WiFiManager::getAPSsid() {
    return _apSsid;
}

String WiFiManager::getAPIP() {
    return WiFi.softAPIP().toString();
}

bool WiFiManager::testCredentials(const char* ssid, const char* password) {
    // Re-assert AP+STA mode before each test to prevent a failed associate from
    // quietly dropping the AP and disconnecting the user's phone mid-setup.
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            WiFi.disconnect(false);
            return false;
        }
        delay(100);
    }
    WiFi.disconnect(false);   // drop STA but keep AP
    return true;
}

String WiFiManager::scanNetworksJSON() {
    int n = WiFi.scanNetworks(false, false, false, 300);
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < n; i++) {
        JsonObject net = arr.add<JsonObject>();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
    }
    WiFi.scanDelete();

    String out;
    serializeJson(doc, out);
    return out;
}

uint16_t WiFiManager::backoffMinutes(uint8_t failCount, uint16_t baseIntervalMin) {
    if (failCount <= WIFI_NORMAL_RETRY_COUNT) {
        return baseIntervalMin;
    }
    // Exponential: double for each failure beyond the normal count, cap at max
    uint32_t extra = failCount - WIFI_NORMAL_RETRY_COUNT;
    uint32_t interval = (uint32_t)baseIntervalMin;
    for (uint32_t i = 0; i < extra; i++) {
        interval *= 2;
        if (interval >= WIFI_BACKOFF_CAP_MIN) {
            return WIFI_BACKOFF_CAP_MIN;
        }
    }
    return (uint16_t)interval;
}
