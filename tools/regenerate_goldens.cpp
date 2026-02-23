#include <filesystem>
#include <iostream>
#include <string>

#include "backtest/backtester.hpp"
#include "backtest/csv_importer.hpp"
#include "backtest/exporter.hpp"

int main(int argc, char** argv) {
    std::filesystem::path root = std::filesystem::current_path();
    if (argc > 1) {
        root = std::filesystem::path(argv[1]);
    }

    const auto sample = root / "data" / "sample.csv";
    const auto equity_golden = root / "tests" / "golden" / "equity.csv";
    const auto trades_golden = root / "tests" / "golden" / "trades.csv";
    const auto metrics_golden = root / "tests" / "golden" / "metrics.json";

    const stockbt::ImportResult import = stockbt::import_ohlcv_csv(sample.string(), stockbt::DateFormat::Iso);
    if (!import.success) {
        std::cerr << "Import failed for sample.csv\n";
        for (const auto& e : import.errors) {
            std::cerr << "line " << e.line << ": " << e.message << "\n";
        }
        return 1;
    }

    stockbt::SmaParams params;
    params.fast_window = 2;
    params.slow_window = 3;
    stockbt::BacktestSettings settings;
    settings.starting_cash = 10000.0;
    settings.commission_pct = 0.001;

    const stockbt::BacktestResult result = stockbt::run_sma_backtest(import.candles, params, settings);

    std::string error;
    if (!stockbt::export_equity_csv(equity_golden.string(), import.candles, result, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    if (!stockbt::export_trades_csv(trades_golden.string(), result, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    stockbt::DatasetMetadata dataset;
    dataset.rows = import.candles.size();
    dataset.start_ts = import.candles.front().ts;
    dataset.end_ts = import.candles.back().ts;
    if (!stockbt::export_metrics_json(metrics_golden.string(), dataset, params, settings, result.metrics, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    std::cout << "Regenerated tests/golden/equity.csv, tests/golden/trades.csv, and tests/golden/metrics.json\n";
    return 0;
}
