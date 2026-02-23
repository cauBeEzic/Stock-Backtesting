#pragma once

#include <string>

#include "backtest/types.hpp"

namespace stockbt {

ImportResult import_ohlcv_csv(const std::string& csv_path, DateFormat date_format);

} // namespace stockbt
