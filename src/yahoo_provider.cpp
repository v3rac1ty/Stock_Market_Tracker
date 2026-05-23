#include "yahoo_provider.hpp"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <stdexcept>
#include <sstream>
#include <cctype>

// ---------------------------------------------------------------------------
// libcurl write callback — appends received bytes to a std::string
// ---------------------------------------------------------------------------
static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
YahooProvider::YahooProvider() {
    curl_global_init(CURL_GLOBAL_ALL);
    m_curl = curl_easy_init();
}

YahooProvider::~YahooProvider() {
    if (m_curl) {
        curl_easy_cleanup(static_cast<CURL*>(m_curl));
    }
    curl_global_cleanup();
}

// ---------------------------------------------------------------------------
// httpGet — perform a GET request and return the response body
// ---------------------------------------------------------------------------
std::string YahooProvider::httpGet(const std::string& url) {
    if (!m_curl) {
        m_lastError = "CURL handle not initialised";
        return {};
    }

    CURL* curl = static_cast<CURL*>(m_curl);
    std::string response;

    // Build custom headers list
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,1L);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    // Reset header option so it doesn't dangle on the next call
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);

    if (res != CURLE_OK) {
        m_lastError = curl_easy_strerror(res);
        return {};
    }

    m_lastError.clear();
    return response;
}

// ---------------------------------------------------------------------------
// buildChartURL — pure static helper, no libcurl calls
// ---------------------------------------------------------------------------
std::string YahooProvider::buildChartURL(
    const std::string& symbol,
    const std::string& range,
    const std::string& interval)
{
    // Simple URL-encode: percent-encode anything that is not unreserved
    // (RFC 3986 unreserved = ALPHA / DIGIT / "-" / "." / "_" / "~")
    std::ostringstream encoded;
    for (unsigned char c : symbol) {
        if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
            encoded << static_cast<char>(c);
        } else {
            encoded << '%'
                    << "0123456789ABCDEF"[(c >> 4) & 0xF]
                    << "0123456789ABCDEF"[ c       & 0xF];
        }
    }

    std::ostringstream url;
    url << "https://query1.finance.yahoo.com/v8/finance/chart/"
        << encoded.str()
        << "?range="    << range
        << "&interval=" << interval
        << "&includePrePost=false";
    return url.str();
}

// ---------------------------------------------------------------------------
// parseChartJSON — pure static helper
// ---------------------------------------------------------------------------
std::vector<CandleData> YahooProvider::parseChartJSON(const std::string& jsonStr) {
    std::vector<CandleData> candles;

    try {
        using json = nlohmann::json;
        json doc = json::parse(jsonStr);

        // chart.result must be a non-null, non-empty array
        const json& chart = doc.at("chart");
        const json& result = chart.at("result");
        if (result.is_null() || result.empty()) {
            return candles;
        }

        const json& entry      = result[0];
        const json& timestamps = entry.at("timestamp");
        const json& quote      = entry.at("indicators").at("quote")[0];

        const json& opens   = quote.at("open");
        const json& highs   = quote.at("high");
        const json& lows    = quote.at("low");
        const json& closes  = quote.at("close");
        const json& volumes = quote.at("volume");

        std::size_t count = timestamps.size();
        for (std::size_t i = 0; i < count; ++i) {
            // Skip rows that contain any null in the OHLC fields
            if (opens[i].is_null()  || highs[i].is_null() ||
                lows[i].is_null()   || closes[i].is_null()) {
                continue;
            }

            CandleData c;
            c.timestamp = static_cast<double>(timestamps[i].get<std::int64_t>());
            c.open      = opens[i].get<double>();
            c.high      = highs[i].get<double>();
            c.low       = lows[i].get<double>();
            c.close     = closes[i].get<double>();
            c.volume    = volumes[i].is_null()
                              ? 0LL
                              : volumes[i].get<long long>();
            candles.push_back(c);
        }
    } catch (...) {
        // Malformed or unexpected JSON — return whatever was collected so far
        // (or empty if the exception fired before any candle was pushed)
        candles.clear();
    }

    return candles;
}

// ---------------------------------------------------------------------------
// fetchHistorical
// ---------------------------------------------------------------------------
std::vector<CandleData> YahooProvider::fetchHistorical(
    const std::string& symbol,
    const std::string& range,
    const std::string& interval)
{
    std::string response = httpGet(buildChartURL(symbol, range, interval));
    if (response.empty()) {
        return {};
    }
    return parseChartJSON(response);
}

// ---------------------------------------------------------------------------
// fetchQuote
// ---------------------------------------------------------------------------
std::optional<StockInfo> YahooProvider::fetchQuote(const std::string& symbol) {
    std::string url      = buildChartURL(symbol, "1d", "1d");
    std::string response = httpGet(url);
    if (response.empty()) {
        return std::nullopt;
    }

    // Parse candles (reuse shared logic)
    std::vector<CandleData> candles = parseChartJSON(response);

    // Also extract meta fields for the live quote
    try {
        using json = nlohmann::json;
        json doc = json::parse(response);

        const json& result = doc.at("chart").at("result");
        if (result.is_null() || result.empty()) {
            return std::nullopt;
        }

        const json& meta          = result[0].at("meta");
        double currentPrice       = meta.value("regularMarketPrice", 0.0);
        double previousClose      = meta.value("previousClose",      0.0);

        if (currentPrice == 0.0) {
            return std::nullopt;
        }

        StockInfo info;
        info.symbol        = symbol;
        info.currentPrice  = currentPrice;
        info.change        = currentPrice - previousClose;
        info.changePercent = (previousClose != 0.0)
                                 ? (info.change / previousClose) * 100.0
                                 : 0.0;
        info.candles       = std::move(candles);
        return info;

    } catch (...) {
        return std::nullopt;
    }
}
