#pragma once
#include "types.h"
#include "config.h"

// Wraps the ESP32 Preferences library for all persistent storage.
// Three logical stores:
//   1. DeviceConfig   — settings written by the portal
//   2. CachedTickerSet — last-known-good data for the WiFi fallback screen
//   3. Runtime counters — rotation position, WiFi failure count

class NVSManager {
public:
    // Returns true if WiFi credentials + at least one ticker are present
    // (i.e. device has been configured). False on true out-of-box state.
    bool isConfigured();

    void loadConfig(DeviceConfig& cfg);
    void saveConfig(const DeviceConfig& cfg);

    void loadCache(CachedTickerSet& cache);
    void saveCache(const CachedTickerSet& cache);
    bool hasCachedData();

    uint8_t  getRotPosition();
    void     setRotPosition(uint8_t pos);

    uint8_t  getFailCount();
    void     setFailCount(uint8_t count);
    void     incrementFailCount();
    void     resetFailCount();
};
