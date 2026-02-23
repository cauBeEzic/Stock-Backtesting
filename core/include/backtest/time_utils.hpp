#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "backtest/types.hpp"

namespace stockbt {

std::optional<int64_t> parse_timestamp_utc(const std::string& text, DateFormat fmt);
std::string format_timestamp_utc_iso8601(int64_t ts);

} // namespace stockbt
