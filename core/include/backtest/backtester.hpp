#pragma once

#include "backtest/types.hpp"

namespace stockbt {

BacktestResult run_sma_backtest(const Series& candles,
                                const SmaParams& params,
                                const BacktestSettings& settings);

} // namespace stockbt
