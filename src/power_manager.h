#pragma once
#include "types.h"
#include <esp_sleep.h>
#include <WiFi.h>

// ── enterSleep ────────────────────────────────────────────────────────────────
// Enters the selected power mode for sleepMins minutes.
// Does not return in BATTERY_SAVER mode (deep sleep triggers a full reset).
// Returns after sleepMins in QUICK_WAKE mode (light sleep).
//
// PRD §7 — Quick Wake MUST have WiFi modem sleep configured first or it will
// not meaningfully reduce power draw. That configuration is done here, not
// assumed to exist elsewhere.
//
// TODO (PRD §11 item 2): Confirm actual wake-to-render timing on real hardware
// for both modes, on both Board A (ESP32-C3) and Board B (ESP32-S3).
// ESP32-C3 deep-sleep wake is reportedly faster than many ESP32 variants.

inline void enterSleep(PowerMode mode, uint16_t sleepMins) {
    uint64_t us = (uint64_t)sleepMins * 60ULL * 1000000ULL;

    if (mode == PowerMode::BATTERY_SAVER) {
        esp_sleep_enable_timer_wakeup(us);
        esp_deep_sleep_start();
        // Does not return.

    } else {
        // Quick Wake: light sleep with WiFi modem sleep enabled.
        // Configuring modem sleep is required to actually save power (PRD §7).
        WiFi.setSleep(WIFI_PS_MAX_MODEM);    // modem sleep between wakes
        esp_sleep_enable_timer_wakeup(us);
        esp_light_sleep_start();
        // Returns here after wakeup.
        WiFi.setSleep(WIFI_PS_NONE);         // restore full WiFi on wake
    }
}
