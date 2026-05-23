# Stock Market Tracker

A real-time stock charting desktop application written in C++17. Pulls live OHLCV data from the Yahoo Finance API, renders interactive candlestick charts with technical indicators, and manages all I/O on a dedicated background thread - keeping the render loop locked at 60 fps regardless of network latency.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue) ![OpenGL](https://img.shields.io/badge/OpenGL-3.3-green) ![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey)

---

## Features

- **Live candlestick charts** - custom OHLC renderer built directly on ImPlot's draw list API (`PlotToPixels`, `AddRectFilled`, `AddLine`), with per-bar bull/bear coloring and sub-pixel body clamping
- **Seven time ranges** - 1D · 5D · 1M · 3M · 6M · 1Y · 5Y, each with the appropriate Yahoo Finance interval (5m → 1wk)
- **Technical indicators** overlaid on the chart, toggleable per session:
  - SMA (20, 50) - sliding window, O(n)
  - EMA (12, 26) - Wilder multiplier `k = 2/(n+1)`, seeded from SMA
  - Bollinger Bands - population std dev upper/lower bands
  - RSI (14) - Wilder smoothing with proper seed averaging, plotted in a separate sub-chart with overbought/oversold zones
- **Watchlist** - add/remove tickers at runtime; color-coded by daily change percentage
- **Zero-lock render loop** - fetch results are delivered from the worker thread to the render thread through a lock-free SPSC ring buffer; the main thread never blocks on I/O

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  Render Thread (main)          60 fps GLFW loop     │
│                                                     │
│  ImGui sidebar / controls                           │
│  DrawCandlestickChart()  ◄──┐                       │
│  DrawRSIChart()             │  g_ring.pop()         │
└─────────────────────────────┼───────────────────────┘
                              │  RingBuffer<StockInfo,8>
┌─────────────────────────────┼───────────────────────┐
│  Worker Thread              │  g_ring.push()        │
│                             │                       │
│  StockTracker::workerLoop() │                       │
│    ← std::condition_variable│                       │
│  YahooProvider::fetchHistorical()                   │
│    libcurl  →  Yahoo Finance v8/finance/chart API   │
│    nlohmann/json  →  OHLCV parse                    │
└─────────────────────────────────────────────────────┘
```

### Key design decisions

**Lock-free SPSC ring buffer** (`include/ring_buffer.hpp`) - head and tail atomics are placed on separate 64-byte cache lines (`alignas(64)`) to prevent false sharing. `push` uses release ordering on the head store; `pop` uses acquire on the head load. No mutexes, no allocations on the hot path.

**Static indicator methods** - `StockTracker::computeSMA`, `computeEMA`, `computeRSI`, `computeBollinger` are all `static`, so the test suite can call them directly without spinning up a worker thread or making network requests.

**Yahoo Finance v8 JSON API** - uses the `/v8/finance/chart/{symbol}` endpoint with `range`, `interval`, and `includePrePost=false` query parameters. Null OHLCV rows (partial trading days) are silently skipped during parse. No web scraping; no crumb authentication (v8 doesn't require it).

---

## Tech Stack

| Layer | Library / Tool |
|---|---|
| Window & input | GLFW 3.4 |
| Immediate-mode UI | Dear ImGui v1.91.6 |
| Charting | ImPlot v0.17 |
| OpenGL | OpenGL 3.3 Core (ImGui embedded loader) |
| HTTP client | libcurl |
| JSON parsing | nlohmann/json v3.11.3 |
| Testing | Catch2 v3.7.1 |
| Build | CMake 3.20+ with FetchContent |
| Language | C++17 |

All dependencies except libcurl are fetched and compiled automatically by CMake at configure time.

---

## Test Suite

Eight Catch2 test cases covering the full non-UI surface:

| Test | What it checks |
|---|---|
| `YahooProvider URL building` | Correct endpoint, query params, percent-encoding of symbols with spaces |
| `YahooProvider JSON parsing` | Happy path (3 candles), null-row skipping, empty/malformed input |
| `StockTracker::computeSMA` | Sliding-window values and NaN prefix for period-1 positions |
| `StockTracker::computeEMA` | Seed value, multiplier, first step after seed |
| `StockTracker::computeRSI` | NaN prefix length, valid range [0,100] at first computed index |
| `StockTracker::computeBollinger` | Middle == SMA, band symmetry, bandwidth == 2·σ·multiplier |
| `RingBuffer` single-threaded | FIFO order, capacity enforcement, push-after-pop |
| `RingBuffer` SPSC stress | 1000 messages in-order across producer/consumer threads |

---

## Building

### Prerequisites

- CMake 3.20+
- C++17 compiler (GCC 11+, Clang 13+, MSVC 2022+)
- libcurl development files

**Windows (MinGW):** download a pre-built MinGW curl package from [curl.se/windows](https://curl.se/windows/) and extract it to `C:\curl-mingw`. The CMakeLists.txt will find it there automatically.

**Linux / macOS:**
```sh
# Ubuntu / Debian
sudo apt install libcurl4-openssl-dev

# macOS
brew install curl
```

### Build

```sh
# Windows (run in cmd with C:\mingw64\bin and C:\curl-mingw\bin in PATH)
"C:\Program Files\CMake\bin\cmake.exe" -S . -B build -G "MinGW Makefiles" ^
  -DCMAKE_C_COMPILER=C:/mingw64/bin/gcc.exe ^
  -DCMAKE_CXX_COMPILER=C:/mingw64/bin/g++.exe ^
  -DCMAKE_MAKE_PROGRAM=C:/mingw64/bin/mingw32-make.exe ^
  -DCMAKE_BUILD_TYPE=Release
"C:\Program Files\CMake\bin\cmake.exe" --build build --parallel
```

```sh
# Linux / macOS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

First build downloads all FetchContent dependencies (~500 MB, ~5-10 min). Subsequent builds are incremental.

### Run

```sh
./build/StockTracker        # Linux / macOS
build\StockTracker.exe      # Windows
```

### Tests

```sh
cd build
ctest --output-on-failure
```
