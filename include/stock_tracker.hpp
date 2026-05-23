#pragma once
#include "stock_data.hpp"
#include "ring_buffer.hpp"
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <optional>

class YahooProvider;

// Manages a watchlist, dispatches async fetch tasks to a dedicated worker
// thread, and provides stateless technical-indicator computations.
class StockTracker {
public:
    StockTracker();
    ~StockTracker();

    StockTracker(const StockTracker&)            = delete;
    StockTracker& operator=(const StockTracker&) = delete;

    // ---- Watchlist --------------------------------------------------------
    void addSymbol(const std::string& symbol);
    void removeSymbol(const std::string& symbol);
    const std::vector<std::string>& watchlist() const { return m_watchlist; }

    // ---- Data fetching ----------------------------------------------------

    // Enqueue an async fetch; callback is invoked on the worker thread.
    void fetchAsync(
        const std::string& symbol,
        int rangeIdx,                          // index into TIME_RANGES[]
        std::function<void(StockInfo)> cb);

    // Blocking convenience wrapper (spins until the worker completes).
    StockInfo fetchSync(const std::string& symbol, int rangeIdx);

    // ---- Technical indicators (pure, stateless) --------------------------

    // Simple moving average.  Output[i] is NaN for i < period-1.
    static std::vector<double> computeSMA(
        const std::vector<CandleData>& candles, int period);

    // Exponential moving average (Wilder smoothing).
    static std::vector<double> computeEMA(
        const std::vector<CandleData>& candles, int period);

    // Relative Strength Index.
    static std::vector<double> computeRSI(
        const std::vector<CandleData>& candles, int period = 14);

    struct BollingerBands {
        std::vector<double> upper;   // middle + stdDevMult * stddev
        std::vector<double> middle;  // SMA(period)
        std::vector<double> lower;   // middle - stdDevMult * stddev
    };
    static BollingerBands computeBollinger(
        const std::vector<CandleData>& candles,
        int    period     = 20,
        double stdDevMult = 2.0);

private:
    struct FetchTask {
        std::string                   symbol;
        int                           rangeIdx;
        std::function<void(StockInfo)> cb;
    };

    void workerLoop();

    std::unique_ptr<YahooProvider> m_provider;
    std::vector<std::string>       m_watchlist;

    std::thread             m_worker;
    std::atomic<bool>       m_running{false};
    std::mutex              m_mu;
    std::condition_variable m_cv;
    std::queue<FetchTask>   m_taskQueue;
};
