#include "backtest/exporter.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

#include "backtest/time_utils.hpp"

namespace stockbt {
namespace {

const char* kDisclaimer = "Educational tool. Not investment advice. No live trading.";

} // namespace

bool export_equity_csv(const std::string& output_path,
                       const Series& candles,
                       const BacktestResult& result,
                       std::string* error) {
    std::ofstream out(output_path);
    if (!out.is_open()) {
        if (error != nullptr) {
            *error = "Failed to open equity output path: " + output_path;
        }
        return false;
    }

    out << "timestamp,equity\n";
    const std::size_t count = std::min(candles.size(), result.equity.size());
    out << std::fixed << std::setprecision(10);
    for (std::size_t i = 0; i < count; ++i) {
        out << format_timestamp_utc_iso8601(candles[i].ts) << ',' << result.equity[i] << '\n';
    }
    return true;
}

bool export_trades_csv(const std::string& output_path,
                       const BacktestResult& result,
                       std::string* error) {
    std::ofstream out(output_path);
    if (!out.is_open()) {
        if (error != nullptr) {
            *error = "Failed to open trades output path: " + output_path;
        }
        return false;
    }

    out << "entry_time,entry_price,exit_time,exit_price,qty,pnl,return_pct\n";
    out << std::fixed << std::setprecision(10);
    for (const Trade& trade : result.trades) {
        out << format_timestamp_utc_iso8601(trade.entry_time) << ',' << trade.entry_price << ','
            << format_timestamp_utc_iso8601(trade.exit_time) << ',' << trade.exit_price << ',' << trade.qty << ','
            << trade.pnl << ',' << trade.return_pct << '\n';
    }
    return true;
}

bool export_metrics_json(const std::string& output_path,
                         const DatasetMetadata& dataset,
                         const SmaParams& params,
                         const BacktestSettings& settings,
                         const Metrics& metrics,
                         std::string* error) {
    std::ofstream out(output_path);
    if (!out.is_open()) {
        if (error != nullptr) {
            *error = "Failed to open metrics output path: " + output_path;
        }
        return false;
    }

    out << std::fixed << std::setprecision(10);
    out << "{\n";
    out << "  \"schema_version\": 2,\n";
    out << "  \"dataset\": {\"rows\": " << dataset.rows << ", \"start\": \""
        << format_timestamp_utc_iso8601(dataset.start_ts) << "\", \"end\": \""
        << format_timestamp_utc_iso8601(dataset.end_ts) << "\"},\n";
    out << "  \"strategy\": {\"name\": \"SMA_CROSS\", \"fast\": " << params.fast_window
        << ", \"slow\": " << params.slow_window << "},\n";
    out << "  \"settings\": {\"starting_cash\": " << settings.starting_cash << ", \"commission_pct\": "
        << settings.commission_pct << ", \"position_size_pct\": " << settings.position_size_pct
        << ", \"stop_loss_pct\": " << settings.stop_loss_pct << ", \"take_profit_pct\": "
        << settings.take_profit_pct << "},\n";
    out << "  \"results\": {\n";
    out << "    \"total_return_pct\": " << metrics.total_return_pct << ",\n";
    out << "    \"total_pnl\": " << metrics.total_pnl << ",\n";
    out << "    \"max_drawdown_pct\": " << metrics.max_drawdown_pct << ",\n";
    out << "    \"trades\": " << metrics.trades << ",\n";
    out << "    \"win_rate_pct\": " << metrics.win_rate_pct << ",\n";
    out << "    \"avg_trade_return_pct\": " << metrics.avg_trade_return_pct << "\n";
    out << "  },\n";
    out << "  \"disclaimer\": \"" << kDisclaimer << "\"\n";
    out << "}\n";

    return true;
}

} // namespace stockbt
