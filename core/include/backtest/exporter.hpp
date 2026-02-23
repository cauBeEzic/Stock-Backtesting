#pragma once

#include <string>

#include "backtest/types.hpp"

namespace stockbt {

bool export_equity_csv(const std::string& output_path,
                       const Series& candles,
                       const BacktestResult& result,
                       std::string* error);

bool export_trades_csv(const std::string& output_path,
                       const BacktestResult& result,
                       std::string* error);

bool export_metrics_json(const std::string& output_path,
                         const DatasetMetadata& dataset,
                         const SmaParams& params,
                         const BacktestSettings& settings,
                         const Metrics& metrics,
                         std::string* error);

} // namespace stockbt
