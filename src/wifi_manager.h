#pragma once
#include <Arduino.h>
#include "config.h"

enum class WiFiResult {
    CONNECTED,
    FAILED,
    TIMEOUT,
};

class WiFiManager {
public:
    // Attempt to connect to saved credentials.
    // Blocks for up to WIFI_CONNECT_TIMEOUT_MS ms.
    WiFiResult connect(const char* ssid, const char* password);

    void disconnect();

    // Start the SoftAP for Config Mode. Generates a unique SSID suffix
    // from the MAC address (e.g. "ePaperTicker-A1B2").
    // Leaves STA mode enabled concurrently for WiFi testing during portal use.
    void startAP();
    void stopAP();

    String getAPSsid();
    String getAPIP();

    // Test a specific SSID+password while AP is running (AP+STA concurrent mode).
    // Returns true if the network was reached. Does NOT commit credentials.
    bool testCredentials(const char* ssid, const char* password);

    // Scan nearby networks, returns JSON array string:
    // [{"ssid":"Name","rssi":-65}, ...]
    String scanNetworksJSON();

    // Backoff-aware: given a consecutive fail count, returns how many minutes
    // to sleep before retrying. Implements: first WIFI_NORMAL_RETRY_COUNT failures
    // use the configured refresh interval; after that, double per failure up to cap.
    static uint16_t backoffMinutes(uint8_t failCount, uint16_t baseIntervalMin);

private:
    String _apSsid;
};
