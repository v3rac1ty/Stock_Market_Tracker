#pragma once
#include "stock_data.hpp"
#include <string>
#include <vector>
#include <optional>

// Fetches market data from Yahoo Finance using the v8/finance/chart JSON API.
// Thread-safe: each instance owns its own CURL handle.
class YahooProvider {
public:
    YahooProvider();
    ~YahooProvider();

    YahooProvider(const YahooProvider&)            = delete;
    YahooProvider& operator=(const YahooProvider&) = delete;

    // Fetch historical OHLCV candles.
    //   range    : "1d", "5d", "1mo", "3mo", "6mo", "1y", "5y"
    //   interval : "1m", "5m", "15m", "30m", "1h", "1d", "1wk"
    // Returns an empty vector on failure; call lastError() for details.
    std::vector<CandleData> fetchHistorical(
        const std::string& symbol,
        const std::string& range,
        const std::string& interval);

    // Fetch current quote (price, change, changePercent) with minimal candles.
    std::optional<StockInfo> fetchQuote(const std::string& symbol);

    std::string lastError() const { return m_lastError; }

    // ---- Exposed for unit tests ----------------------------------------

    // Build the Yahoo Finance v8 chart request URL.
    static std::string buildChartURL(
        const std::string& symbol,
        const std::string& range,
        const std::string& interval);

    // Parse the raw JSON response string returned by the v8 chart endpoint.
    // Returns an empty vector on malformed or missing data.
    static std::vector<CandleData> parseChartJSON(const std::string& json);

private:
    std::string httpGet(const std::string& url);

    void*       m_curl{nullptr};  // opaque CURL* handle
    std::string m_lastError;
};
