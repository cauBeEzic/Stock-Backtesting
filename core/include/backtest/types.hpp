#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace stockbt {

enum class DateFormat {
    Iso,
    Mdy,
    Dmy,
};

struct Candle {
    int64_t ts{0};
    double o{0.0};
    double h{0.0};
    double l{0.0};
    double c{0.0};
    double v{0.0};
};

using Series = std::vector<Candle>;

struct Trade {
    int64_t entry_time{0};
    double entry_price{0.0};
    int64_t exit_time{0};
    double exit_price{0.0};
    int qty{0};
    double pnl{0.0};
    double return_pct{0.0};
};

struct Metrics {
    double total_return_pct{0.0};
    double total_pnl{0.0};
    int trades{0};
    double win_rate_pct{0.0};
    double avg_trade_return_pct{0.0};
    double max_drawdown_pct{0.0};
};

struct BacktestSettings {
    double starting_cash{10000.0};
    double commission_pct{0.001};
    double position_size_pct{1.0}; // fraction of available cash used per entry [0,1]
    double stop_loss_pct{0.0};     // e.g. 0.02 = 2% stop from entry, 0 disables
    double take_profit_pct{0.0};   // e.g. 0.03 = 3% target from entry, 0 disables
};

struct SmaParams {
    std::size_t fast_window{20};
    std::size_t slow_window{50};

    bool is_valid() const {
        return fast_window > 0 && slow_window > 0 && fast_window < slow_window;
    }
};

struct ImportIssue {
    std::size_t line{0};
    std::string message;
};

struct ImportResult {
    bool success{false};
    bool partial_success{false};
    std::size_t dropped_rows{0};
    Series candles;
    std::vector<ImportIssue> warnings;
    std::vector<ImportIssue> errors;
};

struct BacktestResult {
    std::vector<double> equity;
    std::vector<double> drawdown;
    std::vector<Trade> trades;
    Metrics metrics;
    std::vector<std::string> warnings;
};

struct DatasetMetadata {
    std::size_t rows{0};
    int64_t start_ts{0};
    int64_t end_ts{0};
};

} // namespace stockbt
