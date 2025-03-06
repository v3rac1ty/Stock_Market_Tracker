#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <csignal>
#include "stock_tracker.hpp"
#include "yahoo_provider.hpp"

// Global tracker for signal handler
std::unique_ptr<StockTracker> g_tracker;
std::atomic<bool> g_running(true);

// Signal handler for clean exit
void signalHandler(int signal) {
    std::cout << "\nReceived signal to terminate. Cleaning up...\n";
    g_running = false;
    if (g_tracker) {
        g_tracker->stopTracking();
    }
}

int main() {
    try {
        // Setup signal handler
        std::signal(SIGINT, signalHandler);
        
        std::cout << "==== Stock Market Tracker ====\n";
        std::cout << "Press Ctrl+C to exit\n";
        
        // Create stock data provider
        auto yahooProvider = std::make_unique<YahooFinanceDataProvider>();
        
        // Initialize tracker with Yahoo Finance provider
        g_tracker = std::make_unique<StockTracker>(std::move(yahooProvider), 30);
        
        // Add stocks to track
        g_tracker->addSymbol("AAPL");
        g_tracker->addSymbol("GOOGL");
        g_tracker->addSymbol("MSFT");
        
        // Start tracking in a separate thread
        std::thread trackingThread([&]() {
            g_tracker->startTracking();
        });
        
        // Wait for termination signal
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Clean up and join thread
        g_tracker->stopTracking();
        if (trackingThread.joinable()) {
            trackingThread.join();
        }
        
        std::cout << "Stock Market Tracker stopped successfully.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
