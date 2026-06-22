# ePaperTicker

An ESP32 firmware that turns a 7.5" ePaper display into an always-on stock ticker. Fetches live market data over WiFi, renders it to the display, then deep-sleeps until the next refresh — idle power draw is near zero.

![Market overview screen](preview.png)
*800×480 B/W simulator — market overview with index bar, ticker table, and 52-week range bars*

## Features

- **Captive portal setup** — connect to the device's WiFi hotspot, configure your network credentials, data provider API key, and tickers through a browser. No app required.
- **Two data providers** — Finnhub or Twelve Data (switchable in the portal)
- **API key validation** — the portal tests your key against a real fetch before letting you save
- **Rotation bank** — up to 5 ticker rows on screen at once; individual rows can rotate through a bank of additional tickers each refresh cycle
- **NVS persistence** — config and data cache survive power cycles
- **Graceful degradation** — shows cached data if WiFi or the API is unavailable

## Hardware

| | Board A | Board B |
|---|---|---|
| MCU | Seeed XIAO ESP32-C3 | Seeed reTerminal E1001 |
| Display | External 7.5" ePaper panel | Internal 7.5" mono ePaper |
| Status | Abandoned (USB flashing issues on Windows) | **Active target** |

Board B (reTerminal E1001) is the actively developed and tested target.

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
# Board B (reTerminal E1001)
pio run -e board-b

# Flash to COM5
pio run -e board-b -t upload --upload-port COM5

# Serial monitor
pio device monitor -p COM5 -b 115200
```

## First boot

On first boot the device starts in Config Mode — it broadcasts a WiFi hotspot named `ePaperTicker-XXXX`. Connect to it and navigate to `192.168.4.1` to complete setup. You'll need:

1. Your WiFi network credentials
2. A free API key from [Finnhub](https://finnhub.io) or [Twelve Data](https://twelvedata.com)
3. The ticker symbols you want to display

## Dependencies

- [GxEPD2](https://github.com/ZinggJM/GxEPD2) — ePaper display driver
- [ArduinoJson](https://arduinojson.org/) — JSON parsing
