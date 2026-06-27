#pragma once
#include "types.h"
#include "config.h"
#include <esp_sleep.h>
#include <WiFi.h>
#ifdef BOARD_BUILD_B
#  include <driver/gpio.h>
#endif

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
#ifdef BOARD_BUILD_B
        // Allow any of the three physical buttons to wake from deep sleep.
        // Buttons pull the line low when pressed; normal state is high (INPUT_PULLUP).
        const uint64_t btnMask = (1ULL << BTN_GREEN) | (1ULL << BTN_LEFT) | (1ULL << BTN_RIGHT);
        esp_sleep_enable_ext1_wakeup(btnMask, ESP_EXT1_WAKEUP_ANY_LOW);
#endif
        esp_deep_sleep_start();
        // Does not return.

    } else {
        // Quick Wake: light sleep with WiFi modem sleep enabled.
        // Configuring modem sleep is required to actually save power (PRD §7).
#ifdef BOARD_BUILD_B
        gpio_wakeup_enable((gpio_num_t)BTN_GREEN, GPIO_INTR_LOW_LEVEL);
        gpio_wakeup_enable((gpio_num_t)BTN_LEFT,  GPIO_INTR_LOW_LEVEL);
        gpio_wakeup_enable((gpio_num_t)BTN_RIGHT, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
#endif
        WiFi.setSleep(WIFI_PS_MAX_MODEM);    // modem sleep between wakes
        esp_sleep_enable_timer_wakeup(us);
        esp_light_sleep_start();
        // Returns here after wakeup.
        WiFi.setSleep(WIFI_PS_NONE);         // restore full WiFi on wake
    }
}
