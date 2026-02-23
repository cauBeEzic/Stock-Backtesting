#include "backtest/csv_importer.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "backtest/time_utils.hpp"

namespace stockbt {
namespace {

std::string trim(const std::string& input) {
    std::size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
        ++start;
    }
    std::size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
        --end;
    }
    return input.substr(start, end - start);
}

std::string lowercase(std::string input) {
    for (char& ch : input) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return input;
}

std::vector<std::string> parse_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool in_quotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (ch == ',' && !in_quotes) {
            fields.push_back(trim(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    fields.push_back(trim(current));
    return fields;
}

bool parse_double_strict(const std::string& text, double* out) {
    const std::string t = trim(text);
    if (t.empty()) {
        return false;
    }
    char* end_ptr = nullptr;
    errno = 0;
    const double value = std::strtod(t.c_str(), &end_ptr);
    if (errno != 0 || end_ptr == t.c_str() || *end_ptr != '\0') {
        return false;
    }
    *out = value;
    return true;
}

void append_issue(std::vector<ImportIssue>* issues, std::size_t line, const std::string& message) {
    issues->push_back({line, message});
}

} // namespace

ImportResult import_ohlcv_csv(const std::string& csv_path, DateFormat date_format) {
    ImportResult result;

    std::ifstream in(csv_path);
    if (!in.is_open()) {
        append_issue(&result.errors, 0, "Unable to open CSV file: " + csv_path);
        return result;
    }

    std::string header_line;
    if (!std::getline(in, header_line)) {
        append_issue(&result.errors, 1, "CSV is empty");
        return result;
    }

    const std::vector<std::string> headers_raw = parse_csv_line(header_line);
    std::unordered_map<std::string, std::size_t> header_index;
    for (std::size_t i = 0; i < headers_raw.size(); ++i) {
        header_index[lowercase(trim(headers_raw[i]))] = i;
    }

    const auto date_it = header_index.find("date");
    const auto timestamp_it = header_index.find("timestamp");
    const auto open_it = header_index.find("open");
    const auto high_it = header_index.find("high");
    const auto low_it = header_index.find("low");
    const auto close_it = header_index.find("close");
    const auto volume_it = header_index.find("volume");

    const bool has_time = (date_it != header_index.end()) || (timestamp_it != header_index.end());
    if (!has_time || open_it == header_index.end() || high_it == header_index.end() || low_it == header_index.end() ||
        close_it == header_index.end() || volume_it == header_index.end()) {
        append_issue(&result.errors,
                     1,
                     "Missing required columns. Required: Date/Timestamp, Open, High, Low, Close, Volume");
        return result;
    }

    const std::size_t ts_col = (timestamp_it != header_index.end()) ? timestamp_it->second : date_it->second;
    const std::size_t o_col = open_it->second;
    const std::size_t h_col = high_it->second;
    const std::size_t l_col = low_it->second;
    const std::size_t c_col = close_it->second;
    const std::size_t v_col = volume_it->second;

    std::vector<ImportIssue> row_issues;
    std::vector<Candle> valid_rows;

    std::string line;
    std::size_t line_number = 1;
    while (std::getline(in, line)) {
        ++line_number;
        if (trim(line).empty()) {
            continue;
        }

        const std::vector<std::string> fields = parse_csv_line(line);
        const std::size_t max_index = std::max({ts_col, o_col, h_col, l_col, c_col, v_col});
        if (fields.size() <= max_index) {
            ++result.dropped_rows;
            append_issue(&row_issues, line_number, "Dropped row: missing one or more required field values");
            continue;
        }

        const auto ts = parse_timestamp_utc(fields[ts_col], date_format);
        if (!ts.has_value()) {
            ++result.dropped_rows;
            append_issue(&row_issues, line_number, "Dropped row: invalid timestamp format");
            continue;
        }

        double o = 0.0;
        double h = 0.0;
        double l = 0.0;
        double c = 0.0;
        double v = 0.0;
        if (!parse_double_strict(fields[o_col], &o) || !parse_double_strict(fields[h_col], &h) ||
            !parse_double_strict(fields[l_col], &l) || !parse_double_strict(fields[c_col], &c) ||
            !parse_double_strict(fields[v_col], &v)) {
            ++result.dropped_rows;
            append_issue(&row_issues, line_number, "Dropped row: invalid numeric value");
            continue;
        }

        if (o <= 0.0 || h <= 0.0 || l <= 0.0 || c <= 0.0) {
            ++result.dropped_rows;
            append_issue(&row_issues, line_number, "Dropped row: prices must be > 0");
            continue;
        }
        if (v < 0.0) {
            ++result.dropped_rows;
            append_issue(&row_issues, line_number, "Dropped row: volume must be >= 0");
            continue;
        }

        valid_rows.push_back({*ts, o, h, l, c, v});
    }

    if (valid_rows.empty()) {
        append_issue(&result.errors, 0, "Import failed: zero valid rows remain after filtering");
        result.errors.insert(result.errors.end(), row_issues.begin(), row_issues.end());
        return result;
    }

    bool unordered = false;
    for (std::size_t i = 1; i < valid_rows.size(); ++i) {
        if (valid_rows[i].ts < valid_rows[i - 1].ts) {
            unordered = true;
            break;
        }
    }

    if (unordered) {
        append_issue(&result.warnings, 0, "Timestamps were unsorted. Data was sorted ascending.");
    }

    std::stable_sort(valid_rows.begin(), valid_rows.end(), [](const Candle& a, const Candle& b) { return a.ts < b.ts; });

    std::vector<Candle> deduped;
    deduped.reserve(valid_rows.size());
    std::size_t duplicate_count = 0;
    for (const Candle& c : valid_rows) {
        if (!deduped.empty() && deduped.back().ts == c.ts) {
            deduped.back() = c;
            ++duplicate_count;
        } else {
            deduped.push_back(c);
        }
    }
    if (duplicate_count > 0) {
        std::ostringstream oss;
        oss << "Duplicate timestamps detected. Kept last occurrence for " << duplicate_count << " row(s).";
        append_issue(&result.warnings, 0, oss.str());
    }

    result.candles = std::move(deduped);
    result.success = true;
    result.partial_success = result.dropped_rows > 0;

    if (result.partial_success) {
        result.warnings.insert(result.warnings.end(), row_issues.begin(), row_issues.end());
    }

    return result;
}

} // namespace stockbt
