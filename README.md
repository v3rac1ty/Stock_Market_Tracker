# Stock Market Tracker

A real-time stock market tracking application written in C++ that fetches and displays up-to-date stock information from Yahoo Finance.

## Features

- **Real-time Stock Data**: Fetch current price, change, volume and other metrics for any publicly traded stock.
- **Multi-stock Tracking**: Follow multiple stocks simultaneously in a clean, organized display.
- **Color-coded Performance**: Visual indicators show stock performance at a glance (green for up, red for down).
- **Concurrent Processing**: Background thread refreshes stock data while main thread handles display.
- **Clean Shutdown**: Proper resource management and graceful termination with signal handling.

## Technologies Used

- C++17
- libcurl for HTTP requests
- nlohmann/json for JSON parsing
- Multithreading with std::thread, std::mutex, and std::condition_variable
- ANSI terminal colors

## Prerequisites

- C++17 compatible compiler (GCC, Clang, MSVC)
- CMake (version 3.10 or higher)
- libcurl development package
- nlohmann/json library

## Installation

### Clone the repository

```bash
git clone https://github.com/yourusername/Stock_Market_Tracker.git
cd Stock_Market_Tracker
```

### Build with CMake

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

### Run the application

```bash
./stock_market_tracker
```

### Adding/Removing Stocks

To modify the list of tracked stocks, edit `main.cpp`:

```cpp
// Add stocks to track
tracker.addSymbol("AAPL");  // Apple
tracker.addSymbol("GOOGL"); // Google
tracker.addSymbol("MSFT");  // Microsoft
// Add more symbols as needed
```

### Controlling the Application

- Press `Ctrl+C` to exit the application

## Project Structure

```
Stock_Market_Tracker/
├── include/                  # Header files
│   ├── stock_data.hpp        # Stock data structures and provider interface
│   ├── stock_tracker.hpp     # Main tracking functionality
│   └── yahoo_provider.hpp    # Yahoo Finance API implementation
├── src/                      # Implementation files
│   ├── main.cpp              # Entry point
│   ├── stock_data.cpp        # Stock data implementation
│   ├── stock_tracker.cpp     # Tracker implementation
│   └── yahoo_provider.cpp    # Yahoo Finance API implementation
├── CMakeLists.txt            # CMake build configuration
└── README.md                 # This file
```

## How It Works

1. The application initializes the data provider, Yahoo Finance, and the stock tracker
2. It creates a background thread that periodically fetches fresh stock data
3. The main thread displays stock information at regular intervals
4. Stock data includes current price, change, percent change, day high/low, and volume
5. Performance indicators show whether stocks are up or down

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Author

Your Name - [rishivemulapalli@gmail.com](mailto:rishivemulapalli@gmail.com)

---

*Note: This project was created for educational purposes and should not be used for financial decision-making.*
