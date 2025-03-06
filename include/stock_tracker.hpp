#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include "stock_data.hpp"

class StockTracker {
public:
    StockTracker(std::unique_ptr<StockDataProvider> provider, int interval = 60);
    ~StockTracker(); // Add destructor to ensure clean shutdown
    
    void addSymbol(const std::string& symbol);
    void removeSymbol(const std::string& symbol);
    void startTracking();
    void stopTracking(); // Add method to cleanly stop tracking

private:
    std::unique_ptr<StockDataProvider> dataProvider;
    std::vector<std::string> watchlist;
    std::unordered_map<std::string, StockData> stockCache;
    std::mutex dataMutex;
    std::condition_variable refreshCondition;
    int refreshInterval;
    std::atomic<bool> isRunning{true}; // Make isRunning atomic for thread safety
    std::thread refresherThread; // Store the thread as a member

    void refreshThread();
    void displayStockData(const StockData& stock);
};
