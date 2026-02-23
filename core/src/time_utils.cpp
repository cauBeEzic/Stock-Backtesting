#include "backtest/time_utils.hpp"

#include <cstdio>
#include <ctime>

namespace stockbt {
namespace {

bool parse_iso(const std::string& text, std::tm* out_tm) {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    int parsed = std::sscanf(text.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
    if (parsed < 3) {
        parsed = std::sscanf(text.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second);
    }
    if (parsed < 3) {
        return false;
    }

    out_tm->tm_year = year - 1900;
    out_tm->tm_mon = month - 1;
    out_tm->tm_mday = day;
    out_tm->tm_hour = (parsed >= 6) ? hour : 0;
    out_tm->tm_min = (parsed >= 6) ? minute : 0;
    out_tm->tm_sec = (parsed >= 6) ? second : 0;
    return true;
}

bool parse_slash(const std::string& text, bool month_first, std::tm* out_tm) {
    int a = 0;
    int b = 0;
    int year = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    const int parsed = std::sscanf(text.c_str(), "%d/%d/%d %d:%d:%d", &a, &b, &year, &hour, &minute, &second);
    if (parsed < 3) {
        return false;
    }

    const int month = month_first ? a : b;
    const int day = month_first ? b : a;

    out_tm->tm_year = year - 1900;
    out_tm->tm_mon = month - 1;
    out_tm->tm_mday = day;
    out_tm->tm_hour = (parsed >= 6) ? hour : 0;
    out_tm->tm_min = (parsed >= 6) ? minute : 0;
    out_tm->tm_sec = (parsed >= 6) ? second : 0;
    return true;
}

bool is_tm_in_range(const std::tm& tm_value) {
    return tm_value.tm_mon >= 0 && tm_value.tm_mon <= 11 && tm_value.tm_mday >= 1 && tm_value.tm_mday <= 31 &&
           tm_value.tm_hour >= 0 && tm_value.tm_hour <= 23 && tm_value.tm_min >= 0 && tm_value.tm_min <= 59 &&
           tm_value.tm_sec >= 0 && tm_value.tm_sec <= 60;
}

std::optional<int64_t> tm_to_epoch_utc(std::tm tm_value) {
    if (!is_tm_in_range(tm_value)) {
        return std::nullopt;
    }
    tm_value.tm_isdst = -1;

#if defined(_WIN32)
    const std::time_t t = _mkgmtime(&tm_value);
#else
    const std::time_t t = timegm(&tm_value);
#endif
    if (t == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }
    return static_cast<int64_t>(t);
}

} // namespace

std::optional<int64_t> parse_timestamp_utc(const std::string& text, DateFormat fmt) {
    std::tm tm_value{};
    bool ok = false;
    switch (fmt) {
        case DateFormat::Iso:
            ok = parse_iso(text, &tm_value);
            break;
        case DateFormat::Mdy:
            ok = parse_slash(text, true, &tm_value);
            break;
        case DateFormat::Dmy:
            ok = parse_slash(text, false, &tm_value);
            break;
    }
    if (!ok) {
        return std::nullopt;
    }
    return tm_to_epoch_utc(tm_value);
}

std::string format_timestamp_utc_iso8601(int64_t ts) {
    const std::time_t t = static_cast<std::time_t>(ts);

    std::tm out_tm{};
#if defined(_WIN32)
    gmtime_s(&out_tm, &t);
#else
    gmtime_r(&t, &out_tm);
#endif

    char buffer[64];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  out_tm.tm_year + 1900,
                  out_tm.tm_mon + 1,
                  out_tm.tm_mday,
                  out_tm.tm_hour,
                  out_tm.tm_min,
                  out_tm.tm_sec);
    return std::string(buffer);
}

} // namespace stockbt
