#include <mutex>
#include "stock_tracker.hpp"

StockTracker::StockTracker(
    std::unique_ptr<StockDataProvider> provider, 
    int interval
) : 
    dataProvider(std::move(provider)), 
    refreshInterval(interval) {}

StockTracker::~StockTracker() {
    stopTracking();
}

void StockTracker::addSymbol(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(dataMutex);
    watchlist.push_back(symbol);
}

void StockTracker::removeSymbol(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(dataMutex);
    watchlist.erase(
        std::remove(watchlist.begin(), watchlist.end(), symbol), 
        watchlist.end()
    );
}

void StockTracker::refreshThread() {
    while (isRunning.load()) {
        std::vector<std::string> symbolsToRefresh;
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            symbolsToRefresh = watchlist;
        }

        for (const auto& symbol : symbolsToRefresh) {
            try {
                StockData updatedData = dataProvider->fetchStockData(symbol);
                
                std::lock_guard<std::mutex> lock(dataMutex);
                stockCache[symbol] = updatedData;
            }
            catch (const std::exception& e) {
                std::cerr << "Error updating " << symbol << ": " << e.what() << std::endl;
            }
        }

        // Sleep for refresh interval
        std::unique_lock<std::mutex> lock(dataMutex);
        refreshCondition.wait_for(lock, 
            std::chrono::seconds(refreshInterval), 
            [this]{ return !isRunning.load(); }
        );
    }
}

void StockTracker::displayStockData(const StockData& stock) {
    // ANSI color codes
    const std::string GREEN = "\033[32m";
    const std::string RED = "\033[31m";
    const std::string RESET = "\033[0m";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Stock: " << stock.symbol << std::endl;
    
    std::string changeColor = stock.performance == StockPerformance::UP ? GREEN : 
                              stock.performance == StockPerformance::DOWN ? RED : "";
    
    std::cout << changeColor 
              << "Price: $" << stock.currentPrice 
              << " | Change: $" << stock.change 
              << " (" << stock.percentChange << "%)" 
              << RESET << std::endl;
    
    std::cout << "Day High: $" << stock.dayHigh 
              << " | Day Low: $" << stock.dayLow << std::endl;
    std::cout << "Volume: " << stock.volume << std::endl;
    std::cout << std::string(40, '-') << std::endl;
}

void StockTracker::startTracking() {
    isRunning.store(true);
    // Start background refresh thread
    refresherThread = std::thread(&StockTracker::refreshThread, this);
    
    while (isRunning.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        std::lock_guard<std::mutex> lock(dataMutex);
        std::cout << "\n===== Stock Market Tracker =====" << std::endl;
        
        for (const auto& symbol : watchlist) {
            auto it = stockCache.find(symbol);
            if (it != stockCache.end() && it->second.isValid) {
                displayStockData(it->second);
            }
        }
    }

    // Handle thread cleanup in stopTracking()
}

void StockTracker::stopTracking() {
    isRunning.store(false);
    refreshCondition.notify_one();
    if (refresherThread.joinable()) {
        refresherThread.join();
    }
}
