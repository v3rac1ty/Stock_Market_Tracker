#pragma once
#include <string>
#include <ctime>

enum class StockPerformance {
    UP,
    DOWN,
    NEUTRAL
};

struct StockData {
    std::string symbol;
    double currentPrice = 0.0;
    double previousClose = 0.0;
    double change = 0.0;
    double percentChange = 0.0;
    double dayHigh = 0.0;
    double dayLow = 0.0;
    double volume = 0.0;
    StockPerformance performance = StockPerformance::NEUTRAL;
    std::time_t lastUpdated = 0;
    bool isValid = false;

    void updatePerformance();
};

class StockDataProvider {
public:
    virtual StockData fetchStockData(const std::string& symbol) = 0;
    virtual ~StockDataProvider() = default;
};