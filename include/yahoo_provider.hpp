#pragma once
#include "stock_data.hpp"
#include <string>
#include <nlohmann/json.hpp>

class YahooFinanceDataProvider : public StockDataProvider {
public:
    StockData fetchStockData(const std::string& symbol) override;

private:
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output);
    std::string constructUrl(const std::string& symbol);
    std::string fetchUrlContent(const std::string& url);
};
