#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "backtest/types.hpp"

namespace stockbt {

std::optional<int64_t> parse_timestamp_utc(const std::string& text, DateFormat fmt);
std::optional<int64_t> parse_date_time_utc_yyyymmdd_hhmmss(const std::string& date_text,
                                                           const std::string& time_text);
std::string format_timestamp_utc_iso8601(int64_t ts);

} // namespace stockbt
