#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "backtest/backtester.hpp"
#include "backtest/csv_importer.hpp"

namespace {

struct SweepRow {
    std::size_t fast{0};
    std::size_t slow{0};
    stockbt::Metrics train;
    stockbt::Metrics test;
};

stockbt::DateFormat parse_date_format(const std::string& value) {
    if (value == "mdy") {
        return stockbt::DateFormat::Mdy;
    }
    if (value == "dmy") {
        return stockbt::DateFormat::Dmy;
    }
    return stockbt::DateFormat::Iso;
}

void print_usage(const char* argv0) {
    std::cerr << "Usage: " << argv0
              << " <csv_path> <out_csv> [date_format=iso] [train_ratio=0.7]"
              << " [fast_min=5] [fast_max=80] [slow_min=20] [slow_max=300] [step=5]"
              << " [position_size_pct=1.0] [stop_loss_pct=0.0] [take_profit_pct=0.0]\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string csv_path = argv[1];
    const std::string out_csv = argv[2];
    const std::string date_format_arg = (argc > 3) ? argv[3] : "iso";
    const double train_ratio = (argc > 4) ? std::atof(argv[4]) : 0.7;
    const std::size_t fast_min = (argc > 5) ? static_cast<std::size_t>(std::strtoull(argv[5], nullptr, 10)) : 5;
    const std::size_t fast_max = (argc > 6) ? static_cast<std::size_t>(std::strtoull(argv[6], nullptr, 10)) : 80;
    const std::size_t slow_min = (argc > 7) ? static_cast<std::size_t>(std::strtoull(argv[7], nullptr, 10)) : 20;
    const std::size_t slow_max = (argc > 8) ? static_cast<std::size_t>(std::strtoull(argv[8], nullptr, 10)) : 300;
    const std::size_t step = (argc > 9) ? static_cast<std::size_t>(std::strtoull(argv[9], nullptr, 10)) : 5;
    const double position_size_pct = (argc > 10) ? std::atof(argv[10]) : 1.0;
    const double stop_loss_pct = (argc > 11) ? std::atof(argv[11]) : 0.0;
    const double take_profit_pct = (argc > 12) ? std::atof(argv[12]) : 0.0;

    if (train_ratio <= 0.0 || train_ratio >= 1.0) {
        std::cerr << "train_ratio must be in (0,1)\n";
        return 1;
    }
    if (step == 0) {
        std::cerr << "step must be > 0\n";
        return 1;
    }

    const stockbt::ImportResult imported = stockbt::import_ohlcv_csv(csv_path, parse_date_format(date_format_arg));
    if (!imported.success) {
        std::cerr << "Import failed for: " << csv_path << "\n";
        for (const auto& e : imported.errors) {
            std::cerr << "line " << e.line << ": " << e.message << "\n";
        }
        return 1;
    }

    const std::size_t n = imported.candles.size();
    const std::size_t split_idx = static_cast<std::size_t>(static_cast<double>(n) * train_ratio);
    if (split_idx < 2 || split_idx >= n - 1) {
        std::cerr << "Dataset too short for requested split ratio\n";
        return 1;
    }

    const stockbt::Series train(imported.candles.begin(), imported.candles.begin() + static_cast<long>(split_idx));
    const stockbt::Series test(imported.candles.begin() + static_cast<long>(split_idx), imported.candles.end());

    stockbt::BacktestSettings settings;
    settings.starting_cash = 10000.0;
    settings.commission_pct = 0.001;
    settings.position_size_pct = position_size_pct;
    settings.stop_loss_pct = stop_loss_pct;
    settings.take_profit_pct = take_profit_pct;

    std::vector<SweepRow> rows;
    for (std::size_t fast = fast_min; fast <= fast_max; fast += step) {
        for (std::size_t slow = slow_min; slow <= slow_max; slow += step) {
            stockbt::SmaParams params;
            params.fast_window = fast;
            params.slow_window = slow;
            if (!params.is_valid()) {
                continue;
            }
            if (train.size() < params.slow_window || test.size() < params.slow_window) {
                continue;
            }

            const stockbt::BacktestResult train_result = stockbt::run_sma_backtest(train, params, settings);
            const stockbt::BacktestResult test_result = stockbt::run_sma_backtest(test, params, settings);
            rows.push_back({fast, slow, train_result.metrics, test_result.metrics});
        }
    }

    if (rows.empty()) {
        std::cerr << "No valid parameter combinations produced results\n";
        return 1;
    }

    std::sort(rows.begin(), rows.end(), [](const SweepRow& a, const SweepRow& b) {
        if (a.train.total_return_pct != b.train.total_return_pct) {
            return a.train.total_return_pct > b.train.total_return_pct;
        }
        return a.train.max_drawdown_pct > b.train.max_drawdown_pct;
    });

    std::ofstream out(out_csv);
    if (!out.is_open()) {
        std::cerr << "Failed to open output report path: " << out_csv << "\n";
        return 1;
    }

    out << "fast,slow,train_return_pct,train_max_drawdown_pct,train_trades,test_return_pct,test_max_drawdown_pct,test_trades\n";
    out << std::fixed << std::setprecision(6);
    for (const SweepRow& row : rows) {
        out << row.fast << ',' << row.slow << ',' << row.train.total_return_pct << ',' << row.train.max_drawdown_pct << ','
            << row.train.trades << ',' << row.test.total_return_pct << ',' << row.test.max_drawdown_pct << ','
            << row.test.trades << '\n';
    }

    const SweepRow& best = rows.front();
    std::cout << "Rows imported: " << n << "\n";
    std::cout << "Train rows: " << train.size() << ", Test rows: " << test.size() << "\n";
    std::cout << "Best (by train return): fast=" << best.fast << " slow=" << best.slow << "\n";
    std::cout << "Train return=" << best.train.total_return_pct << "% maxDD=" << best.train.max_drawdown_pct
              << "% trades=" << best.train.trades << "\n";
    std::cout << "Test return=" << best.test.total_return_pct << "% maxDD=" << best.test.max_drawdown_pct
              << "% trades=" << best.test.trades << "\n";
    std::cout << "Report written: " << out_csv << "\n";

    return 0;
}
