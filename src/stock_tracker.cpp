#include "stock_tracker.hpp"
#include "yahoo_provider.hpp"
#include "stock_data.hpp"
#include <algorithm>
#include <cmath>
#include <future>
#include <limits>
#include <stdexcept>

// ---- Constructor / Destructor -----------------------------------------------

StockTracker::StockTracker() {
    m_provider = std::make_unique<YahooProvider>();
    m_running  = true;
    m_worker   = std::thread(&StockTracker::workerLoop, this);
}

StockTracker::~StockTracker() {
    m_running = false;
    m_cv.notify_all();
    if (m_worker.joinable())
        m_worker.join();
}

// ---- Watchlist --------------------------------------------------------------

void StockTracker::addSymbol(const std::string& symbol) {
    std::string upper = symbol;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    for (const auto& s : m_watchlist) {
        std::string existing = s;
        std::transform(existing.begin(), existing.end(), existing.begin(), ::toupper);
        if (existing == upper)
            return;
    }
    m_watchlist.push_back(upper);
}

void StockTracker::removeSymbol(const std::string& symbol) {
    std::string upper = symbol;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    m_watchlist.erase(
        std::remove_if(m_watchlist.begin(), m_watchlist.end(),
            [&upper](const std::string& s) {
                std::string existing = s;
                std::transform(existing.begin(), existing.end(), existing.begin(), ::toupper);
                return existing == upper;
            }),
        m_watchlist.end());
}

// ---- Worker thread ----------------------------------------------------------

void StockTracker::workerLoop() {
    while (m_running) {
        FetchTask task;
        {
            std::unique_lock<std::mutex> lock(m_mu);
            m_cv.wait(lock, [&] { return !m_taskQueue.empty() || !m_running; });
            if (!m_running && m_taskQueue.empty())
                break;
            task = std::move(m_taskQueue.front());
            m_taskQueue.pop();
        }

        const auto& tr     = TIME_RANGES[task.rangeIdx];
        auto        candles = m_provider->fetchHistorical(task.symbol, tr.range, tr.interval);

        StockInfo info;
        info.symbol  = task.symbol;
        info.candles = std::move(candles);
        if (!info.candles.empty()) {
            info.currentPrice  = info.candles.back().close;
            info.change        = info.candles.back().close - info.candles.front().open;
            info.changePercent = (info.change / info.candles.front().open) * 100.0;
        }
        task.cb(std::move(info));
    }
}

// ---- Data fetching ----------------------------------------------------------

void StockTracker::fetchAsync(
    const std::string& symbol,
    int rangeIdx,
    std::function<void(StockInfo)> cb)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_taskQueue.push(FetchTask{symbol, rangeIdx, std::move(cb)});
    m_cv.notify_one();
}

StockInfo StockTracker::fetchSync(const std::string& symbol, int rangeIdx) {
    std::promise<StockInfo> promise;
    std::future<StockInfo>  future = promise.get_future();

    fetchAsync(symbol, rangeIdx, [&promise](StockInfo info) {
        promise.set_value(std::move(info));
    });

    return future.get();
}

// ---- Technical indicators ---------------------------------------------------

std::vector<double> StockTracker::computeSMA(
    const std::vector<CandleData>& candles, int period)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const int    n   = static_cast<int>(candles.size());
    std::vector<double> result(n, nan);

    if (period <= 0 || period > n)
        return result;

    double windowSum = 0.0;
    for (int i = 0; i < period; ++i)
        windowSum += candles[i].close;
    result[period - 1] = windowSum / period;

    for (int i = period; i < n; ++i) {
        windowSum += candles[i].close - candles[i - period].close;
        result[i]  = windowSum / period;
    }
    return result;
}

std::vector<double> StockTracker::computeEMA(
    const std::vector<CandleData>& candles, int period)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const int    n   = static_cast<int>(candles.size());
    std::vector<double> result(n, nan);

    if (period <= 0 || period > n)
        return result;

    // Seed: average of the first `period` closes
    double seed = 0.0;
    for (int i = 0; i < period; ++i)
        seed += candles[i].close;
    seed /= period;
    result[period - 1] = seed;

    const double k = 2.0 / (period + 1);
    for (int i = period; i < n; ++i)
        result[i] = candles[i].close * k + result[i - 1] * (1.0 - k);

    return result;
}

std::vector<double> StockTracker::computeRSI(
    const std::vector<CandleData>& candles, int period)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const int    n   = static_cast<int>(candles.size());
    std::vector<double> result(n, nan);

    if (period <= 0 || n <= period)
        return result;

    // Compute seed averages over the first `period` day-over-day changes
    double avgGain = 0.0;
    double avgLoss = 0.0;
    for (int i = 1; i <= period; ++i) {
        double diff = candles[i].close - candles[i - 1].close;
        if (diff > 0.0)
            avgGain += diff;
        else
            avgLoss += -diff;
    }
    avgGain /= period;
    avgLoss /= period;

    // First RSI value at index `period`
    if (avgLoss == 0.0)
        result[period] = 100.0;
    else
        result[period] = 100.0 - 100.0 / (1.0 + avgGain / avgLoss);

    // Wilder smoothing for the rest
    for (int i = period + 1; i < n; ++i) {
        double diff = candles[i].close - candles[i - 1].close;
        double gain = (diff > 0.0) ? diff : 0.0;
        double loss = (diff < 0.0) ? -diff : 0.0;

        avgGain = (avgGain * (period - 1) + gain) / period;
        avgLoss = (avgLoss * (period - 1) + loss) / period;

        if (avgLoss == 0.0)
            result[i] = 100.0;
        else
            result[i] = 100.0 - 100.0 / (1.0 + avgGain / avgLoss);
    }
    return result;
}

StockTracker::BollingerBands StockTracker::computeBollinger(
    const std::vector<CandleData>& candles,
    int    period,
    double stdDevMult)
{
    const int n = static_cast<int>(candles.size());
    BollingerBands bands;
    bands.middle.resize(n);
    bands.upper.resize(n);
    bands.lower.resize(n);

    // Compute the SMA (middle band) using computeSMA
    bands.middle = computeSMA(candles, period);

    const double nan = std::numeric_limits<double>::quiet_NaN();

    for (int i = 0; i < n; ++i) {
        if (std::isnan(bands.middle[i])) {
            bands.upper[i] = nan;
            bands.lower[i] = nan;
            continue;
        }

        // Population std dev of the last `period` closes
        double mean = bands.middle[i];
        double variance = 0.0;
        for (int j = i - period + 1; j <= i; ++j) {
            double diff = candles[j].close - mean;
            variance += diff * diff;
        }
        variance /= period;
        double stddev = std::sqrt(variance);

        bands.upper[i] = mean + stdDevMult * stddev;
        bands.lower[i] = mean - stdDevMult * stddev;
    }
    return bands;
}
