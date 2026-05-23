#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct CandleData {
    double    timestamp;  // Unix epoch seconds
    double    open;
    double    high;
    double    low;
    double    close;
    long long volume;
};

struct StockInfo {
    std::string symbol;
    double      currentPrice{0.0};
    double      change{0.0};        // absolute change from previous close
    double      changePercent{0.0};
    std::vector<CandleData> candles;
};

struct TimeRange {
    const char* label;
    const char* range;    // Yahoo Finance range param: "1d","5d","1mo","3mo","6mo","1y","5y"
    const char* interval; // Yahoo Finance interval:   "1m","5m","15m","30m","1h","1d","1wk"
};

inline constexpr TimeRange TIME_RANGES[] = {
    {"1D",  "1d",  "5m"},
    {"5D",  "5d",  "30m"},
    {"1M",  "1mo", "1d"},
    {"3M",  "3mo", "1d"},
    {"6M",  "6mo", "1d"},
    {"1Y",  "1y",  "1wk"},
    {"5Y",  "5y",  "1wk"},
};
inline constexpr int NUM_TIME_RANGES = 7;
