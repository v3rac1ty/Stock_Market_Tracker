#include "stock_data.hpp"

void StockData::updatePerformance() {
    if (change > 0) performance = StockPerformance::UP;
    else if (change < 0) performance = StockPerformance::DOWN;
    else performance = StockPerformance::NEUTRAL;
}
