#include "yahoo_provider.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <iostream>

size_t YahooFinanceDataProvider::writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string YahooFinanceDataProvider::constructUrl(const std::string& symbol) {
    return "https://query1.finance.yahoo.com/v10/finance/quoteSummary/" + 
           symbol + "?modules=price,summaryDetail";
}

std::string YahooFinanceDataProvider::fetchUrlContent(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string responseBuffer;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }

    return responseBuffer;
}

StockData YahooFinanceDataProvider::fetchStockData(const std::string& symbol) {
    StockData stockData;
    stockData.symbol = symbol;
    stockData.lastUpdated = std::time(nullptr);

    try {
        std::string url = constructUrl(symbol);
        std::string response = fetchUrlContent(url);
        
        auto jsonResponse = nlohmann::json::parse(response);
        auto result = jsonResponse["quoteSummary"]["result"][0];
        
        auto price = result["price"];
        auto summaryDetail = result["summaryDetail"];

        // Populate stock data
        stockData.currentPrice = price["regularMarketPrice"]["raw"].get<double>();
        stockData.previousClose = price["regularMarketPreviousClose"]["raw"].get<double>();
        stockData.change = price["regularMarketChange"]["raw"].get<double>();
        stockData.percentChange = price["regularMarketChangePercent"]["raw"].get<double>();
        stockData.dayHigh = price["regularMarketDayHigh"]["raw"].get<double>();
        stockData.dayLow = price["regularMarketDayLow"]["raw"].get<double>();
        stockData.volume = summaryDetail["volume"]["raw"].get<double>();

        stockData.updatePerformance();
        stockData.isValid = true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error fetching stock data for " << symbol 
                  << ": " << e.what() << std::endl;
        stockData.isValid = false;
    }

    return stockData;
}
