#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "backtest/backtester.hpp"
#include "backtest/csv_importer.hpp"

namespace {

std::filesystem::path write_synthetic_csv(const std::filesystem::path& out_path, std::size_t rows) {
    std::ofstream out(out_path);
    out << "Date,Open,High,Low,Close,Volume\n";

    // Deterministic pseudo-market path with bounded drift.
    double price = 100.0;
    int day = 1;
    int month = 1;
    int year = 2020;

    for (std::size_t i = 0; i < rows; ++i) {
        const double drift = ((static_cast<int>(i % 29) - 14) * 0.02);
        const double open = price;
        const double close = std::max(1.0, open + drift);
        const double high = std::max(open, close) + 0.3;
        const double low = std::min(open, close) - 0.3;

        out << std::setw(4) << std::setfill('0') << year << '-'
            << std::setw(2) << month << '-'
            << std::setw(2) << day << ','
            << std::fixed << std::setprecision(6)
            << open << ',' << high << ',' << low << ',' << close << ",1000\n";

        price = close;
        ++day;
        if (day > 28) {
            day = 1;
            ++month;
            if (month > 12) {
                month = 1;
                ++year;
            }
        }
    }

    return out_path;
}

} // namespace

int main(int argc, char** argv) {
    const std::size_t rows = (argc > 1) ? static_cast<std::size_t>(std::stoull(argv[1])) : 200000;
    const std::filesystem::path csv_path = std::filesystem::temp_directory_path() / "stockbt_benchmark_ohlcv.csv";

    write_synthetic_csv(csv_path, rows);

    const auto import_start = std::chrono::steady_clock::now();
    const auto import = stockbt::import_ohlcv_csv(csv_path.string(), stockbt::DateFormat::Iso);
    const auto import_end = std::chrono::steady_clock::now();

    if (!import.success) {
        std::cerr << "Import failed in benchmark\n";
        return 1;
    }

    stockbt::SmaParams params;
    params.fast_window = 20;
    params.slow_window = 50;
    stockbt::BacktestSettings settings;
    settings.starting_cash = 10000.0;
    settings.commission_pct = 0.001;

    const auto backtest_start = std::chrono::steady_clock::now();
    const auto result = stockbt::run_sma_backtest(import.candles, params, settings);
    const auto backtest_end = std::chrono::steady_clock::now();

    const auto import_ms = std::chrono::duration_cast<std::chrono::milliseconds>(import_end - import_start).count();
    const auto backtest_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(backtest_end - backtest_start).count();

    std::cout << "Rows: " << import.candles.size() << "\n";
    std::cout << "Import ms: " << import_ms << "\n";
    std::cout << "Backtest ms: " << backtest_ms << "\n";
    std::cout << "Trades: " << result.metrics.trades << "\n";
    std::cout << "Total return (%): " << result.metrics.total_return_pct << "\n";

    std::error_code ec;
    std::filesystem::remove(csv_path, ec);
    return 0;
}
