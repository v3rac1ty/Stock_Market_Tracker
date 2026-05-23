#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "yahoo_provider.hpp"
#include "stock_tracker.hpp"
#include "ring_buffer.hpp"
#include "stock_data.hpp"
#include <cmath>
#include <thread>
#include <atomic>
#include <vector>

using Catch::Approx;

// ---------------------------------------------------------------------------
// Helper: build a candle vector from a list of close prices.
// All other OHLC fields are set equal to the close; volume is fixed.
// ---------------------------------------------------------------------------
static std::vector<CandleData> makeCandles(std::initializer_list<double> closes)
{
    std::vector<CandleData> v;
    double ts = 1700000000.0;
    for (double c : closes) {
        CandleData cd{};
        cd.timestamp = ts;  ts += 86400.0;
        cd.open  = cd.high = cd.low = cd.close = c;
        cd.volume = 1000000;
        v.push_back(cd);
    }
    return v;
}

// ===========================================================================
// 1. YahooProvider URL building
// ===========================================================================
TEST_CASE("YahooProvider URL building", "[yahoo][url]")
{
    SECTION("basic URL structure and query parameters")
    {
        const std::string url = YahooProvider::buildChartURL("AAPL", "1mo", "1d");

        REQUIRE(url.find("query1.finance.yahoo.com") != std::string::npos);
        REQUIRE(url.find("v8/finance/chart/AAPL")    != std::string::npos);
        REQUIRE(url.find("range=1mo")                != std::string::npos);
        REQUIRE(url.find("interval=1d")              != std::string::npos);
        REQUIRE(url.find("includePrePost=false")     != std::string::npos);
    }

    SECTION("symbol with a space is percent-encoded")
    {
        // "BRK B" must appear as "BRK%20B" (or similar encoding) — never raw space
        const std::string url = YahooProvider::buildChartURL("BRK B", "1mo", "1d");

        REQUIRE(url.find(' ') == std::string::npos);          // no raw space
        REQUIRE(url.find("BRK%20B") != std::string::npos);   // percent-encoded
    }
}

// ===========================================================================
// 2. YahooProvider JSON parsing
// ===========================================================================
TEST_CASE("YahooProvider JSON parsing", "[yahoo][parse]")
{
    // Minimal Yahoo Finance v8 /chart response with 3 candles.
    static const std::string kGoodJSON = R"({
  "chart": {
    "result": [{
      "meta": {
        "regularMarketPrice": 185.5,
        "previousClose": 184.0,
        "symbol": "AAPL"
      },
      "timestamp": [1704067200, 1704153600, 1704240000],
      "indicators": {
        "quote": [{
          "open":   [183.0, 184.5, 185.0],
          "high":   [186.0, 186.5, 187.0],
          "low":    [182.0, 183.5, 184.0],
          "close":  [184.5, 185.0, 185.5],
          "volume": [50000000, 52000000, 48000000]
        }]
      }
    }],
    "error": null
  }
})";

    SECTION("happy path — three candles parsed correctly")
    {
        auto candles = YahooProvider::parseChartJSON(kGoodJSON);

        REQUIRE(candles.size() == 3);

        REQUIRE(candles[0].timestamp == Approx(1704067200));
        REQUIRE(candles[0].open      == Approx(183.0));
        REQUIRE(candles[0].high      == Approx(186.0));
        REQUIRE(candles[0].low       == Approx(182.0));
        REQUIRE(candles[0].close     == Approx(184.5));
        REQUIRE(candles[0].volume    == 50000000LL);

        REQUIRE(candles[2].close     == Approx(185.5));
        REQUIRE(candles[1].volume    == 52000000LL);
    }

    SECTION("null values are skipped")
    {
        // Row index 1 has null close — that candle must be dropped.
        static const std::string kNullJSON = R"({
  "chart": {
    "result": [{
      "meta": { "regularMarketPrice": 185.5, "previousClose": 184.0, "symbol": "AAPL" },
      "timestamp": [1704067200, 1704153600, 1704240000],
      "indicators": {
        "quote": [{
          "open":   [183.0, 184.5, 185.0],
          "high":   [186.0, 186.5, 187.0],
          "low":    [182.0, 183.5, 184.0],
          "close":  [184.5, null,  185.5],
          "volume": [50000000, 52000000, 48000000]
        }]
      }
    }],
    "error": null
  }
})";
        auto candles = YahooProvider::parseChartJSON(kNullJSON);
        REQUIRE(candles.size() == 2);
    }

    SECTION("empty/garbage JSON returns empty vector")
    {
        REQUIRE(YahooProvider::parseChartJSON("{}").empty());
        REQUIRE(YahooProvider::parseChartJSON("").empty());
        REQUIRE(YahooProvider::parseChartJSON("{not valid json!!!}").empty());
    }
}

// ===========================================================================
// 3. StockTracker::computeSMA
// ===========================================================================
TEST_CASE("StockTracker::computeSMA", "[indicator][sma]")
{
    // close = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, period = 3
    // SMA[i] = NaN for i < 2, then averages of three consecutive closes:
    //   index 2: (1+2+3)/3 = 2.0
    //   index 3: (2+3+4)/3 = 3.0  ...  index 9: (8+9+10)/3 = 9.0
    auto candles = makeCandles({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto sma     = StockTracker::computeSMA(candles, 3);

    REQUIRE(sma.size() == candles.size());

    // First two positions must be NaN.
    REQUIRE(std::isnan(sma[0]));
    REQUIRE(std::isnan(sma[1]));

    // Valid positions.
    REQUIRE(sma[2] == Approx(2.0));
    REQUIRE(sma[3] == Approx(3.0));
    REQUIRE(sma[4] == Approx(4.0));
    REQUIRE(sma[5] == Approx(5.0));
    REQUIRE(sma[6] == Approx(6.0));
    REQUIRE(sma[7] == Approx(7.0));
    REQUIRE(sma[8] == Approx(8.0));
    REQUIRE(sma[9] == Approx(9.0));
}

// ===========================================================================
// 4. StockTracker::computeEMA
// ===========================================================================
TEST_CASE("StockTracker::computeEMA", "[indicator][ema]")
{
    // close = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, period = 3
    // Seed at index 2 = SMA of first three = (1+2+3)/3 = 2.0
    // Multiplier k = 2 / (period + 1) = 2/4 = 0.5
    // EMA[3] = 4*0.5 + 2.0*0.5 = 3.0
    // EMA[4] = 5*0.5 + 3.0*0.5 = 4.0
    auto candles = makeCandles({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto ema     = StockTracker::computeEMA(candles, 3);

    REQUIRE(ema.size() == candles.size());

    // Positions before the seed must be NaN.
    REQUIRE(std::isnan(ema[0]));
    REQUIRE(std::isnan(ema[1]));

    // Seed value.
    REQUIRE(ema[2] == Approx(2.0));

    // First step after the seed.
    REQUIRE(ema[3] == Approx(3.0));
}

// ===========================================================================
// 5. StockTracker::computeRSI
// ===========================================================================
TEST_CASE("StockTracker::computeRSI", "[indicator][rsi]")
{
    // 15-element close series; RSI period = 14.
    auto candles = makeCandles({
        44.00, 44.34, 44.09, 44.15, 43.61,
        44.33, 44.83, 45.10, 45.15, 43.61,
        44.33, 44.83, 45.10, 45.15, 43.61
    });

    auto rsi = StockTracker::computeRSI(candles, 14);

    REQUIRE(rsi.size() == candles.size());

    // The first 14 positions (indices 0–13) do not have a full window yet.
    for (int i = 0; i < 14; ++i) {
        REQUIRE(std::isnan(rsi[i]));
    }

    // Index 14 is the first complete RSI value.
    REQUIRE_FALSE(std::isnan(rsi[14]));
    REQUIRE(rsi[14] >= 0.0);
    REQUIRE(rsi[14] <= 100.0);
}

// ===========================================================================
// 6. StockTracker::computeBollinger
// ===========================================================================
TEST_CASE("StockTracker::computeBollinger", "[indicator][bollinger]")
{
    // 30-element arithmetic series: close[i] = i + 1.0  (1.0 … 30.0)
    std::initializer_list<double> vals = {
         1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
        11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
        21, 22, 23, 24, 25, 26, 27, 28, 29, 30
    };
    auto candles = makeCandles(vals);
    const int    period     = 5;
    const double stdDevMult = 2.0;

    auto bb = StockTracker::computeBollinger(candles, period, stdDevMult);

    REQUIRE(bb.upper.size()  == candles.size());
    REQUIRE(bb.middle.size() == candles.size());
    REQUIRE(bb.lower.size()  == candles.size());

    // Positions before the first full window must be NaN.
    for (int i = 0; i < period - 1; ++i) {
        REQUIRE(std::isnan(bb.middle[i]));
        REQUIRE(std::isnan(bb.upper[i]));
        REQUIRE(std::isnan(bb.lower[i]));
    }

    // For each valid position verify middle = SMA, bands are symmetric.
    for (int i = period - 1; i < static_cast<int>(candles.size()); ++i) {
        // Compute expected middle (SMA) and population stddev from raw closes.
        double sum = 0.0;
        for (int j = i - period + 1; j <= i; ++j)
            sum += candles[j].close;
        const double mean = sum / period;

        double sumSq = 0.0;
        for (int j = i - period + 1; j <= i; ++j) {
            double diff = candles[j].close - mean;
            sumSq += diff * diff;
        }
        const double stddev = std::sqrt(sumSq / period);

        REQUIRE(bb.middle[i] == Approx(mean).epsilon(1e-9));
        REQUIRE(bb.upper[i]  >  bb.middle[i]);
        REQUIRE(bb.lower[i]  <  bb.middle[i]);

        // Band width == 2 * stdDevMult * stddev  (within floating-point tolerance).
        const double bandwidth = bb.upper[i] - bb.lower[i];
        REQUIRE(bandwidth == Approx(2.0 * stdDevMult * stddev).epsilon(1e-9));
    }
}

// ===========================================================================
// 7. RingBuffer — single-threaded correctness
// ===========================================================================
TEST_CASE("RingBuffer single-threaded", "[ringbuffer][st]")
{
    // Capacity = 4 means 3 usable slots (one slot is always reserved).
    RingBuffer<int, 4> rb;

    REQUIRE(rb.empty());
    REQUIRE(rb.capacity() == 3);

    // Fill to capacity.
    REQUIRE(rb.push(10));
    REQUIRE(rb.push(20));
    REQUIRE(rb.push(30));

    // Buffer is full; a fourth push must fail.
    REQUIRE_FALSE(rb.push(40));

    // Pop should return values in FIFO order.
    auto v1 = rb.pop();
    REQUIRE(v1.has_value());
    REQUIRE(*v1 == 10);

    // After one pop there is room again.
    REQUIRE(rb.push(40));

    // Drain and verify remaining order.
    auto v2 = rb.pop();
    REQUIRE(v2.has_value());
    REQUIRE(*v2 == 20);

    auto v3 = rb.pop();
    REQUIRE(v3.has_value());
    REQUIRE(*v3 == 30);

    auto v4 = rb.pop();
    REQUIRE(v4.has_value());
    REQUIRE(*v4 == 40);

    // Buffer is now empty.
    REQUIRE(rb.empty());
    REQUIRE_FALSE(rb.pop().has_value());
}

// ===========================================================================
// 8. RingBuffer — multi-threaded SPSC stress test
// ===========================================================================
TEST_CASE("RingBuffer multi-threaded SPSC", "[ringbuffer][mt]")
{
    constexpr int kMessages = 1000;
    RingBuffer<int, 1024> rb;

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    std::thread producer([&] {
        for (int i = 0; i < kMessages; ++i) {
            while (!rb.push(i))
                std::this_thread::yield();
            produced.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread consumer([&] {
        int expected = 0;
        while (expected < kMessages) {
            if (auto v = rb.pop()) {
                REQUIRE(*v == expected);
                ++expected;
                consumed.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    REQUIRE(produced.load() == kMessages);
    REQUIRE(consumed.load() == kMessages);
}
