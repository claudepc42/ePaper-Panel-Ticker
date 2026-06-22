#pragma once

// ── NVS ──────────────────────────────────────────────────────────────────────
#define NVS_NAMESPACE         "ticker"

#define NVS_WIFI_SSID         "wifi_ssid"
#define NVS_WIFI_PASS         "wifi_pass"
#define NVS_BOARD_TARGET      "board_target"
#define NVS_PANEL_VARIANT     "panel_variant"
#define NVS_PROVIDER          "provider"
#define NVS_API_KEY           "api_key"
#define NVS_ROT_INTERVAL      "rot_interval"   // stored as total minutes
#define NVS_REF_INTERVAL      "ref_interval"   // stored as total minutes
#define NVS_POWER_MODE        "power_mode"
#define NVS_TICKER_COUNT      "ticker_count"
#define NVS_TICKER_PREFIX     "tk_"            // "tk_0" … "tk_N" → JSON blobs
#define NVS_CACHE_PREFIX      "c_"             // "c_VOO" → JSON blobs (prefix kept short: NVS keys max 15 chars)
#define NVS_CACHE_TIMESTAMP   "cache_ts"       // epoch of last successful fetch
#define NVS_ROT_POSITION      "rot_pos"        // which rotating ticker is current
#define NVS_FAIL_COUNT        "fail_count"     // consecutive WiFi failures

// ── WiFi / AP ─────────────────────────────────────────────────────────────────
#define WIFI_CONNECT_TIMEOUT_MS   18000
#define AP_SSID_BASE              "ePaperTicker-"
#define AP_PASSWORD               ""              // open network for ease of setup
#define AP_IP_STR                 "192.168.4.1"
#define DNS_PORT                  53

// ── Buttons ───────────────────────────────────────────────────────────────────
// PRD §2.4 / §11 item 5 — confirmed 4 s
#define BOOT_HOLD_MS              4000

// ── Intervals & limits ────────────────────────────────────────────────────────
#define DEFAULT_ROT_INTERVAL_MIN  30
#define DEFAULT_REF_INTERVAL_MIN  60
#define MIN_INTERVAL_AB_MIN       15     // boards A & B (PRD §5.2)
#define MIN_INTERVAL_C_MIN        30     // board C reserved (PRD §2.3.1)

// WiFi retry backoff (PRD §4.3) — confirmed: 3 normal → exponential cap 240 min
#define WIFI_NORMAL_RETRY_COUNT   3
#define WIFI_BACKOFF_CAP_MIN      240

// ── Display ───────────────────────────────────────────────────────────────────
#define DISPLAY_W   800
#define DISPLAY_H   480

// ── SPI / ePaper pin defaults (Board A — external panel wiring) ──────────────
// Adjust here to match your physical wiring. Board B uses internal connections
// defined in hal/display_board_b.h.
//
// NOTE: On the XIAO ESP32-C3, D-label numbers do NOT equal GPIO numbers.
// Mapping: D0=GPIO2, D1=GPIO3, D2=GPIO4, D3=GPIO5, D4=GPIO6, D5=GPIO7,
//          D6=GPIO21, D7=GPIO20, D8=GPIO8, D9=GPIO9, D10=GPIO10.
// These defines use GPIO numbers directly.
#ifndef EPAPER_CS
#  define EPAPER_CS    3    // GPIO3 = D1 on XIAO C3 — verified against Seeed ESPHome wiki
#endif
#ifndef EPAPER_DC
#  define EPAPER_DC    5    // GPIO5 = D3
#endif
#ifndef EPAPER_RST
#  define EPAPER_RST   2    // GPIO2 = D0
#endif
#ifndef EPAPER_BUSY
#  define EPAPER_BUSY  4    // GPIO4 = D2 (active low — GxEPD2 handles polarity internally)
#endif

// ── Data providers ────────────────────────────────────────────────────────────
#define TWELVE_DATA_HOST  "api.twelvedata.com"
#define FINNHUB_HOST      "finnhub.io"
#define HTTPS_PORT        443

// Request timeout for API calls
#define HTTP_TIMEOUT_MS   10000

// ── Portal ────────────────────────────────────────────────────────────────────
#define PORTAL_PORT       80
