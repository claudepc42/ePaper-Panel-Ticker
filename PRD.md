# PRD: ePaper Stock Ticker (Multi-Board)

## 1. Overview

A standalone, battery-powered stock ticker, designed to run on **three Seeed Studio ePaper hardware targets** from one shared codebase:

1. **XIAO 7.5" ePaper Panel** — bare panel + XIAO ESP32-C3
2. **reTerminal E1001** — enclosed device, monochrome 4-gray panel, XIAO ESP32-S3 under the hood
3. **reTerminal E1002** — enclosed device, full-color (Spectra 6) panel, XIAO ESP32-S3 under the hood

The device fetches stock/ETF data directly from a cloud API over HTTPS — no intermediary server, no Home Assistant, no cloud dashboard service required. All configuration (WiFi credentials, tracked tickers, refresh timing, power mode, board/display selection) is done via a phone-based captive portal hosted directly on the device, entered by holding the BOOT button.

This is a from-scratch Arduino firmware project — not built on ESPHome or TRMNL firmware, since neither supports custom standalone operation against an arbitrary stock API with this level of on-device configurability. (Note: all three boards do natively support TRMNL/ESPHome/SenseCraft HMI as alternative no-code paths — out of scope here, but worth knowing if scope ever needs to shrink.)

### 1.1 Board Selection Strategy
One shared codebase, board/display target selected via a setting (NVS-stored, exposed in the captive portal's Advanced settings, same pattern as the panel-variant toggle) — **not** three separate firmware builds. Shared logic (modes, portal, NVS schema, data fetching/caching, WiFi handling) is board-agnostic. Only the **hardware abstraction layer** (Section 2) and **rendering layer** (Section 6) branch by board.

## 2. Hardware

### 2.1 Board A: XIAO 7.5" ePaper Panel
- **MCU:** XIAO ESP32-C3 (single-core RISC-V @ 160MHz, 400KB SRAM, 4MB flash, WiFi 802.11 b/g/n, BLE 5.0)
- **Display:** 7.5" e-paper, 800×480, UC8179-family SPI controller, native 4-level grayscale hardware mode
- **Power:** 2000mAh Li-ion, USB-C charging, physical on/off switch
- **Buttons:** BOOT + RESET behind the stand
- **Display library:** GxEPD2 (two candidate driver classes — see Section 6.4)

### 2.2 Board B: reTerminal E1001
- **MCU:** XIAO ESP32-S3 (dual-core, 8MB PSRAM, 416KB RAM on-target) — confirmed Seeed's own PlatformIO config builds this board as `board = seeed-xiao-esp32-s3-sense`, i.e. it is a XIAO ESP32-S3 under the enclosure, not a bespoke chip
- **Display:** 7.5" monochrome e-paper, 800×480, **UC8179 controller — same controller family as Board A's panel**, native 4-level grayscale
- **Power:** 2000mAh Li-ion, ~3-month battery life at low refresh, USB-C
- **Extras (not used by this project but present):** microSD slot, temp/humidity sensor, microphone, buzzer, 8-pin GPIO header, LEDs
- **Display library:** Seeed_GFX (a TFT_eSPI fork) is Seeed's primary documented path, exposing 4-gray mode via `epaper.initGrayMode(GRAY_LEVEL4)`. GxEPD2 is also confirmed as a supported alternative for this board. **Recommend using GxEPD2 for this board too**, given Board A already requires it — minimizes the number of display libraries the codebase depends on, at the cost of not using Seeed's "primary" path. Flag this choice explicitly to Claude Code as a deliberate tradeoff, not an oversight, in case GxEPD2 support for this exact board turns out to be less mature in practice than Seeed_GFX once implementation starts.

### 2.3 Board C: reTerminal E1002
- **MCU:** XIAO ESP32-S3 (same as Board B)
- **Display:** 7.3" full-color e-paper, 800×480, **ED2208 controller** (different from Boards A/B) — E Ink Spectra 6 technology, 6 pigment colors (black/white/red/yellow/green/blue) blended at the subpixel level for up to 4,096 displayable colors (12-bit depth)
- **Power:** 2000mAh Li-ion, ~3-month battery life **specifically at a 6-hour refresh interval** (this is a meaningfully longer reference interval than Boards A/B's marketing claims, reflecting the slower/costlier refresh — see 2.3.1)
- **Display library:** Seeed_GFX, via the library's separate Colorful/HelloWorld example path (distinct from the Basic/HelloWorld monochrome path used by Boards A/B) — confirms the library already branches cleanly along the same color/mono line this PRD's rendering split follows (Section 6)

#### 2.3.1 Refresh Time — Critical Constraint
Full-color refresh on this panel has been observed taking up to ~30 seconds, dramatically longer than the ~1.5-3s typical of Boards A/B's grayscale refresh. This is a hardware/electrophoretic characteristic of multi-pigment e-ink, not a software inefficiency — **do not attempt to optimize this away**, just design around it:
- The 15-minute minimum refresh/rotation interval (Section 5.2) should be reconsidered for Board C specifically — a 30s refresh eating into a 15-min cycle is a much bigger relative cost than the same 30s would be against a 60-min cycle. Recommend a higher minimum floor when Board C is the selected target (e.g. 30 min), enforced in the portal's validation logic based on the currently-selected board.
- Battery life claims/estimates shown anywhere in the portal UI must be board-specific, not a single shared number, given the 6-hour-reference-interval difference called out above.

### 2.4 Buttons (all boards)
RESET restarts firmware as-is. BOOT-hold (confirmed: 4s) is the **sole** trigger for entering Config Mode, on all three boards. All boards have a physical on/off power switch — no soft-power/dual-duty button design needed.

### 2.5 Hardware Abstraction Layer (Required Architecture)
Given three boards share a codebase, the firmware needs a clean separation:
- **Board-select setting** (NVS, portal-exposed) determines which of three concrete implementations is active
- A single internal display interface (e.g. `initDisplay()`, `drawMarketOverview(data)`, `drawWifiUnavailable(cached)`, `drawSetupRequired(ssid, ip)`, `clearDisplay()`) that all three board implementations satisfy
- Boards A and B share the grayscale rendering path (Section 6, 4-tone constraint) — likely literally the same rendering code, since both use UC8179 and (per 2.2's recommendation) GxEPD2; only the GPIO/board-init specifics differ
- Board C uses a **separate color rendering path** (Section 6.6) — not a recolored version of the same draw calls, since color semantics (e.g. red/green for gains/losses) don't exist in the grayscale path at all
- This mirrors the pattern already established for the GxEPD2 panel-variant toggle (Section 6.4) — same idea, one more layer up the stack

## 3. Operating Modes

### 3.1 Normal Mode (default)
On wake (timer or fresh boot with saved config):
1. Connect to saved WiFi (timeout: suggest 15-20s)
2. If connected: fetch ticker data from the configured provider for all tracked symbols
3. Cache the fetched data to persistent storage (see Section 5.3)
4. Render to e-paper (Market Overview layout — static tickers + rotating tickers per current rotation position)
5. Enter sleep per configured Power Mode (see Section 7) for the configured Data Refresh Interval
6. If WiFi connection fails: render the "WiFi Unavailable" fallback screen (Section 6.2) using cached data if available, then sleep and retry per backoff schedule (Section 4.3) — does **not** auto-enter Config Mode

### 3.2 Config Mode
Entered only by holding BOOT for the confirmed duration. Device:
1. Starts a SoftAP (SSID `ePaperTicker-XXXX` where XXXX is a MAC-derived suffix)
2. Runs a captive portal web server serving the settings UI (Section 8)
3. On save: writes all settings to NVS, then restarts into Normal Mode

### 3.3 First-Boot Mode (automatic, no button press)
If the device boots with **no saved WiFi credentials and no saved tickers at all** (true out-of-box state), it automatically enters Config Mode's AP/portal behavior without requiring a BOOT hold, and renders the "Setup Required" e-paper screen (Section 6.3) with on-screen connection instructions, since the user has no other way to know what to do at this point.

## 4. WiFi & Connectivity Behavior

### 4.1 Connection attempt
On every wake in Normal Mode, attempt to connect to saved credentials with a bounded timeout (~15-20s). Do not block indefinitely.

### 4.2 On failure
- Render WiFi Unavailable fallback (Section 6.2)
- Do NOT automatically drop into Config/AP mode — that remains a deliberate physical action only. An automatic fallback to AP mode would mean a transient router blip silently takes the device out of its intended job and broadcasts an open setup network unattended.

### 4.3 Retry schedule
Use a backoff schedule rather than retrying every single wake at full frequency if failures persist (exact backoff curve to be defined during implementation — e.g. retry at normal refresh interval for the first few failures, then back off to a longer interval if failures continue, to conserve battery during extended outages).

## 5. Data Model / Persistent Storage (NVS)

Three logical categories of persisted data:

### 5.1 WiFi Credentials
- SSID, password

### 5.2 Device Configuration
- **Board/Display Target:** enum `xiao_panel | reterminal_e1001 | reterminal_e1002` — selects which hardware abstraction implementation (Section 2.5) is active; exposed in portal Advanced settings (Section 8.7). This is the top-level branch; the existing Display Panel Variant setting (below) remains a sub-choice that only applies when Board/Display Target = `xiao_panel`
- Ticker list: array of `{symbol, mode: static|rotating, order: int}`
- Rotation Interval: stored as total minutes (hours + minutes entered separately in UI, stored combined), minimum enforced value 15 minutes **for Boards A/B; 30 minutes when Board/Display Target = reterminal_e1002, per Section 2.3.1**
- Data Refresh Interval: same hours+minutes pattern, same board-dependent minimum
- Power Mode: enum `battery_saver | quick_wake`
- Display Panel Variant: enum, two values corresponding to the two candidate GxEPD2 driver classes (Section 6.4) — generic labels, not part numbers; **only relevant/shown when Board/Display Target = xiao_panel**
- Data Provider: enum `twelve_data | finnhub` (Section 9) — **exposed as a portal setting in v1** (confirmed), shown as a dropdown/choice on the Ticker Management page (Section 8.2) alongside the API key field, so the user can pick a provider and enter its corresponding key directly from their phone, with no firmware change required to switch
- API key for the currently-selected provider

### 5.3 Cached Ticker Data
- Last successfully fetched value set per symbol (price, change, etc. — see Section 6.1 for exact fields) plus a timestamp of last successful fetch
- Must survive reboot/deep-sleep cycles (i.e., NVS or equivalent flash-backed storage, not just RAM) — this is what powers the WiFi Unavailable fallback screen's "last known data" display
- Overwritten on every successful fetch; not a history/log, just the most recent snapshot

## 6. Display Rendering

### 6.1 Market Overview (normal operation)
Per the existing Claude Design mockup (treated as the approved layout reference): header bar with date/time/market status, a 3-column index summary (e.g. S&P 500 / NASDAQ / DOW placeholders — confirm whether user wants real index data or just ETF/ticker rows; mockup includes both), and a data table of tracked tickers showing symbol, name, price, day change, day high/low, and 52-week range indicator. User has confirmed satisfaction with this mockup's field set — implement matching fields, sourced from whichever provider fields are available (see Section 9 — confirm field availability per provider before finalizing, as not all fields may exist on both Twelve Data and Finnhub free tiers).

Rotating tickers cycle through additional symbols in the configured order, each shown for the configured Rotation Interval, in whatever screen region is designated for rotation (vs. the static region) — exact layout split between static/rotating to be finalized during implementation in collaboration with the design.

### 6.2 WiFi Unavailable (fallback state)
Shown when saved credentials exist but connection fails. Calm, non-alarming tone. Shows:
- Status message + instruction to hold BOOT to reconfigure (framed as an option, not a demand, since the device retries automatically)
- Last cached ticker data if available, viscerally deprioritized (lighter gray tone) with a "last updated [timestamp], may be outdated" label
- If no cached data exists yet (e.g., immediately post-setup before first successful fetch): simpler empty state, no fake placeholder data

### 6.3 Setup Required (first-boot fallback)
Shown only during the automatic First-Boot Mode (Section 3.3). Shows:
- Friendly headline ("Setup Required" / "Let's Get Started")
- The AP's SSID, rendered large/clear/monospace
- Instruction to connect from a phone, with fallback IP address if the captive portal doesn't auto-open

### 6.4 Display Hardware / Driver (Boards A & B — grayscale tier)
Both boards confirmed using `GxEPD2_750_T7` (GDEY075T7 / UC8179 controller). Board A first-boot confirmed: setup screen rendered correctly with `GxEPD2_BW<GxEPD2_750_T7, ...>` as the active driver class. The panel variant toggle (Section 5.2) remains in the portal as a fallback escape hatch — if a user's Board A panel turns out to be the EK79655 variant (GDEW075T7, `GxEPD2_750`), they can switch without reflashing. Default stays `GxEPD2_750_T7`.

SPI pins confirmed for Board A (verified against Seeed ESPHome wiki): CS=GPIO3, DC=GPIO5, RST=GPIO2, BUSY=GPIO4.

### 6.5 Color Constraints (Boards A & B — grayscale tier only)
All e-paper rendering on Boards A and B must use flat fills only across exactly 4 tones (pure black, dark gray, light gray, pure white) — no gradients, no antialiasing, no soft shadows. The display cannot blend between levels; crisp edges between tone regions only. This applies even though the existing design mockup (built in a phone/web context) uses soft antialiased grays and subtle shadows for preview purposes — those need to be flattened to the 4-tone constraint when translated into actual rendering code. **This constraint does not apply to Board C** — see Section 6.6.

### 6.6 Color Rendering (Board C — reTerminal E1002 only)
Board C's Spectra 6 panel (Section 2.3) supports real color, not just grayscale. This is a genuine opportunity, not just a constraint to work around — a stock ticker is one of the better use cases for actual color (red for losses, green for gains, rather than relying on +/- symbols or arrows alone).

- Use real semantic colors where they add information: red for negative change, green for positive change, consistent with conventional stock UI design
- Still respect the underlying pigment-mixing model (Section 2.3) — the 6 base pigments mix at the subpixel level, but don't assume smooth gradients render cleanly; treat this more like a limited/spot-color palette (think early color e-readers, not a phone screen) rather than full photographic color. Flat, deliberate color blocks and text, not gradients or photographic imagery.
- Given the ~30s refresh cost (Section 2.3.1), Board C should NOT attempt partial-region updates or animations — every refresh is a full-screen redraw by design, and the UI/UX (and rotation/refresh interval defaults) should be built around that being normal and expected for this board, not an error condition
- Layout can otherwise follow the same Market Overview structure as Boards A/B (Section 6.1), just with color applied to the change/gain-loss indicators specifically — no need for a fundamentally different layout, just a fundamentally different color-rendering implementation underneath

## 7. Power Management

Two selectable modes (Section 5.2, exposed in portal Advanced settings):

- **Battery Saver** (`esp_deep_sleep_start()`): Maximum battery life. Device fully powers down between wakes (RAM cleared, full reboot + WiFi reconnect on every wake). ESP32-C3 wakes from deep sleep notably faster than many comparable chips, so reconnect overhead, while present, may be smaller in practice than typical deep-sleep horror stories — confirm actual wake-to-render duration on real hardware before finalizing any user-facing time estimate.
- **Quick Wake** (light sleep): Sub-1ms wake latency, peripheral/CPU state preserved across sleep. Requires explicitly configuring WiFi modem sleep + automatic light sleep to get real power savings — simply calling light-sleep APIs without this additional config will not meaningfully reduce power draw relative to staying awake. Implementation must include this WiFi modem-sleep configuration, not just the sleep mode call, or the mode will not deliver its intended benefit.

This selection is a runtime branch (different function calls), not a compile-time/architectural fork — no firmware-variant complexity here, just an if/else based on the stored setting.

## 8. Captive Portal (Phone-Facing Setup UI)

All pages are normal responsive mobile-web pages served from the ESP32's AP, built from the Claude Design output (see design handoff notes, Section 10). Lightweight HTML/CSS/minimal vanilla JS — no heavy frameworks, due to embedded HTTP server constraints.

### 8.1 WiFi Setup
SSID + password entry (scan-assisted if feasible, manual entry as fallback), proceeds to Ticker Management.

### 8.2 Ticker Management
Add/remove ticker symbols, static vs. rotating designation per ticker, drag-reorder for rotation order, a Data Provider selector (dropdown/choice between Twelve Data and Finnhub — Section 9) with its corresponding API key entry field directly below it (the key field should update its label/helper text based on which provider is selected, e.g. "Enter your Twelve Data API key" vs. "Enter your Finnhub API key"), Rotation Interval (hours+minutes, 15-min floor, inline validation).

### 8.3 Review & Confirm
Read-only summary (WiFi SSID only, not password; full ticker list with mode/order). Edit links back to relevant sections. "Save & Restart" CTA.

### 8.4 Saving / Restarting
Full-screen confirmation while the device writes settings and reboots.

### 8.5 Error States
- **WiFi connection test failure**: the portal performs a live connection test against the entered SSID/password *before* allowing Save — i.e., the device attempts to actually join the network from within Config Mode, and reports success/failure back to the portal UI immediately. A bad password or unreachable network is caught at this point, on the user's phone, rather than discovered later via the WiFi Unavailable e-paper fallback screen after a restart. This requires the firmware to support attempting a WiFi client connection while the SoftAP/portal is still active (ESP32 supports AP+STA concurrent mode for this), then reporting the result to the in-progress portal session before committing anything to NVS.
- Empty ticker list (blocks proceeding)
- Invalid/missing API key format

### 8.6 Reconfigure Entry
Same flow, fields pre-populated from current NVS values, adjusted copy ("Update Settings" vs. "First-Time Setup").

### 8.7 Advanced / Device Settings
Reachable via a link from Review & Confirm, not part of the main flow:
- **Board/Display Target selector** (Section 5.2) — three options, labeled by actual product name ("XIAO Panel" / "reTerminal E1001" / "reTerminal E1002") since these are physically distinct products the user is choosing between, not an ambiguous troubleshooting toggle like the panel variant below. Changing this should dynamically show/hide the Display Panel Variant sub-setting (only relevant for XIAO Panel) and adjust the enforced minimum for Rotation/Refresh Interval fields (30 min floor instead of 15 min when reTerminal E1002 is selected — Section 2.3.1)
- Display Panel Variant toggle (Section 6.4) — generic labels ("Panel A"/"Panel B"), plain-language troubleshooting helper text; **only shown when Board/Display Target = XIAO Panel**
- Data Refresh Interval (hours+minutes, board-dependent floor)
- Power Mode selection (Section 7) — plain-language labels and tradeoff descriptions, not technical jargon

## 9. Data Provider

### 9.1 Decision
Build a **provider-abstraction layer** rather than hard-coupling to one API. Rationale: free-tier limits across providers are inconsistently documented and sometimes contradicted between official docs and real-world testing (confirmed directly during scoping — Finnhub's per-minute limit of 60 calls/min is well-corroborated, but its daily/monthly cap is unverified and disputed across sources). Abstracting the fetch layer means a provider swap later is a contained change, not a rewrite.

### 9.2 Default Provider: Twelve Data
- Free tier: 800 calls/day, no per-minute throttling pressure reported
- ~4-hour data delay — acceptable given this device's refresh interval floor is 15 minutes but realistic configured intervals will likely be 30-60+ minutes; a device polling this infrequently isn't meaningfully served by lower-latency data anyway
- Chosen as default specifically because its documented limit is a single unambiguous daily number, simplifying the "will my settings exceed the free tier" math for the end user

### 9.3 Alternative Provider: Finnhub
- Free tier: 60 calls/min (well-documented), daily/monthly cap unverified
- ~20-minute data delay
- Documented as a supported alternative; user can switch by changing the Data Provider setting and entering the corresponding API key

### 9.4 Abstraction Requirements
- A single internal interface: given a symbol, return a normalized struct (price, change, change%, day high/low, etc. — exact field set per Section 6.1, intersected with what's actually available across both providers' free-tier responses)
- Provider-specific HTTP request construction, response parsing, and field-mapping isolated behind this interface
- Adding a third provider in the future should require only a new implementation of this interface, no changes to rendering/caching/portal code

### 9.5 Rate-Limit Defense
Regardless of provider, implement defensive handling for 429/rate-limit responses:
- Do not silently fail or render stale data as if it were fresh
- Surface a visible "data limit reached" type state (extend the WiFi Unavailable-style fallback screen pattern, Section 6.2, to also cover this case) so a misconfigured combination of ticker count × refresh interval × provider limit degrades visibly rather than silently
- Before finalizing default settings (default ticker count, default refresh interval), verify actual observed limits against a real account dashboard for the chosen provider, since published/third-party-reported numbers have proven unreliable during scoping for at least one candidate provider

## 10. Design Handoff

Visual design (both the e-paper screen states and the captive portal pages) is being produced in Claude Design as a separate prototyping pass, then handed to Claude Code via the design-to-code MCP connector alongside this PRD. Claude Design's output should be treated as:
- **Authoritative for:** page layout, copy/tone, field arrangement, navigation flow between portal pages, the e-paper screen states (Market Overview, WiFi Unavailable, Setup Required, and the Advanced settings page including the new Board/Display Target selector)
- **Not authoritative for:** exact color values on the Board A/B e-paper screens (must be corrected to the 4-tone flat-fill constraint per Section 6.5 regardless of what the mockup shows), underlying markup complexity (must be simplified to fit embedded server constraints per Section 8's intro), and obviously none of the firmware logic in this document
- **Note:** the existing Claude Design mockups were built before Board C (color) was in scope. They remain the layout reference for all three boards' Market Overview screen, but Board C's implementation additionally applies real color per Section 6.6 — this is an addition to the design intent (more information conveyed via color), not a deviation from it, and shouldn't require new mockups unless the team wants to preview it specifically

## 11. Open Items for Implementation

These were identified during scoping as needing verification on real hardware/accounts rather than further research, and should not block starting implementation — they're flagged so they're addressed deliberately rather than discovered as surprises:

1. Confirm exact GxEPD2 class names for both Board A display panel candidates
2. Confirm actual wake-to-render timing for both Power Modes on real hardware, **for all three boards separately** — the ESP32-S3 boards (B, C) may behave differently from the ESP32-C3 (A) here, not yet verified
3. Confirm Twelve Data's actual free-tier field availability matches the Section 6.1 field set (day high/low, 52-week range) — if not fully available, decide whether to drop fields, supplement with a second provider call, or adjust the layout
4. Verify real observed rate limits on an actual Twelve Data (and/or Finnhub) account dashboard before finalizing default Refresh Interval / ticker count recommendations shown anywhere in the portal UI or documentation
5. Finalize BOOT-hold duration for entering Config Mode
6. Finalize exact backoff curve for WiFi retry after failures (Section 4.3)
7. Confirm field-set parity between Twelve Data and Finnhub responses is close enough that switching providers via the portal doesn't silently break parts of the Market Overview layout (e.g. if one provider lacks 52-week range on its free tier) — may need graceful per-field fallback (hide field) rather than assuming both providers return an identical shape
8. **Verify GxEPD2 actually has mature/working support for the reTerminal E1001 (Board B) specifically**, not just for the standalone XIAO panel — Section 2.2 recommends GxEPD2 for code-sharing reasons, but Seeed's own primary documented path for this board is Seeed_GFX, not GxEPD2. If GxEPD2 support proves thin or buggy on this exact board during implementation, fall back to Seeed_GFX for Board B and accept a small amount of duplicated/non-shared rendering code between Board A and Board B as the cost
9. **Confirm Board C's actual achievable refresh time on real hardware** before finalizing the 30-minute recommended floor (Section 2.3.1) — the ~30s figure was a third-party-reported anecdote (a forum/community post), not a datasheet spec; could be better or worse in practice, and may also depend on how much of the screen actually changes between refreshes
10. **Confirm whether the same captive portal/WiFi/NVS code truly runs unmodified across ESP32-C3 (Board A) and ESP32-S3 (Boards B/C)** — both are part of the same Arduino-ESP32 core and this is generally true, but worth a smoke test early rather than assuming, since it's foundational to the "one shared codebase" goal
11. Decide whether Board C's color semantic mapping (red/green for loss/gain) should be user-configurable (e.g. for colorblind accessibility, some users might prefer different color pairs) or hardcoded — not raised before since it didn't exist before Board C; flagging now rather than assuming
