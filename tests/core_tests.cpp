#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "backtest/backtester.hpp"
#include "backtest/csv_importer.hpp"
#include "backtest/exporter.hpp"
#include "backtest/time_utils.hpp"

namespace {

int g_failures = 0;

void check_true(bool condition, const std::string& message) {
    if (!condition) {
        ++g_failures;
        std::cerr << "[FAIL] " << message << '\n';
    }
}

void check_near(double a, double b, double tol, const std::string& message) {
    if (std::fabs(a - b) > tol) {
        ++g_failures;
        std::cerr << "[FAIL] " << message << " expected=" << b << " actual=" << a << '\n';
    }
}

std::string read_all(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool contains_warning(const std::vector<std::string>& warnings, const std::string& needle) {
    return std::any_of(warnings.begin(), warnings.end(), [&](const std::string& w) {
        return w.find(needle) != std::string::npos;
    });
}

void test_timestamp_format() {
    const auto ts = stockbt::parse_timestamp_utc("2024-01-05", stockbt::DateFormat::Iso);
    check_true(ts.has_value(), "ISO timestamp should parse");
    if (!ts.has_value()) {
        return;
    }
    check_true(stockbt::format_timestamp_utc_iso8601(*ts) == "2024-01-05T00:00:00Z",
               "UTC timestamp formatting should include Z suffix");

    check_true(stockbt::parse_timestamp_utc("01/05/2024", stockbt::DateFormat::Mdy).has_value(),
               "MDY parsing should succeed");
    check_true(stockbt::parse_timestamp_utc("05/01/2024", stockbt::DateFormat::Dmy).has_value(),
               "DMY parsing should succeed");
}

void test_import_filtering_sort_and_duplicates() {
    const auto import = stockbt::import_ohlcv_csv("data/sample_mixed_invalid.csv", stockbt::DateFormat::Iso);
    check_true(import.success, "mixed-invalid sample should import successfully");
    check_true(import.partial_success, "mixed-invalid sample should be partial success");
    check_true(import.dropped_rows == 2, "mixed-invalid sample should drop two rows");
    check_true(import.candles.size() == 4, "mixed-invalid sample should keep four rows");

    check_true(import.candles.front().ts <= import.candles.back().ts, "import output should be sorted ascending");

    bool has_unsorted_warning = false;
    bool has_duplicate_warning = false;
    for (const auto& w : import.warnings) {
        if (w.message.find("unsorted") != std::string::npos) {
            has_unsorted_warning = true;
        }
        if (w.message.find("Duplicate timestamps") != std::string::npos) {
            has_duplicate_warning = true;
        }
    }
    check_true(has_unsorted_warning, "unsorted warning should be present");
    check_true(has_duplicate_warning, "duplicate timestamp warning should be present");
}

void test_force_close_end_long() {
    const auto import = stockbt::import_ohlcv_csv("data/sample_end_long.csv", stockbt::DateFormat::Iso);
    check_true(import.success, "sample_end_long should import");

    stockbt::SmaParams params;
    params.fast_window = 2;
    params.slow_window = 3;

    const auto result = stockbt::run_sma_backtest(import.candles, params, stockbt::BacktestSettings{});
    check_true(!result.trades.empty(), "sample_end_long should produce at least one trade");
    if (!result.trades.empty()) {
        const auto& last = result.trades.back();
        check_true(last.exit_time == import.candles.back().ts, "force close should exit on final bar timestamp");
    }
    check_true(contains_warning(result.warnings, "force-closed"), "force-close warning should be present");
}

void test_short_dataset_behavior() {
    stockbt::Series s;
    s.push_back({stockbt::parse_timestamp_utc("2024-01-01", stockbt::DateFormat::Iso).value_or(0), 1, 1, 1, 1, 1});
    s.push_back({stockbt::parse_timestamp_utc("2024-01-02", stockbt::DateFormat::Iso).value_or(0), 1, 1, 1, 1, 1});

    stockbt::SmaParams params;
    params.fast_window = 2;
    params.slow_window = 5;

    const auto result = stockbt::run_sma_backtest(s, params, stockbt::BacktestSettings{});
    check_true(result.trades.empty(), "short dataset should produce zero trades");
    check_true(contains_warning(result.warnings, "below slow_window"), "short dataset warning should be emitted");
}

void test_drawdown_units() {
    const auto import = stockbt::import_ohlcv_csv("data/sample_end_long.csv", stockbt::DateFormat::Iso);
    check_true(import.success, "sample_end_long should import for drawdown test");

    stockbt::SmaParams params;
    params.fast_window = 2;
    params.slow_window = 3;
    const auto result = stockbt::run_sma_backtest(import.candles, params, stockbt::BacktestSettings{});

    double min_dd = 0.0;
    for (double dd : result.drawdown) {
        check_true(dd <= 1e-12, "drawdown should be <= 0 fraction");
        check_true(dd >= -1.0 - 1e-12, "drawdown should be >= -1 fraction");
        min_dd = std::min(min_dd, dd);
    }
    check_near(result.metrics.max_drawdown_pct, min_dd * 100.0, 1e-6, "max_drawdown_pct conversion should match");
}

void test_regression_goldens() {
    const auto import = stockbt::import_ohlcv_csv("data/sample.csv", stockbt::DateFormat::Iso);
    check_true(import.success, "sample should import for regression test");
    if (!import.success) {
        return;
    }

    stockbt::SmaParams params;
    params.fast_window = 2;
    params.slow_window = 3;
    stockbt::BacktestSettings settings;

    const auto result = stockbt::run_sma_backtest(import.candles, params, settings);

    std::filesystem::create_directories("tests/tmp");
    std::string error;
    check_true(stockbt::export_trades_csv("tests/tmp/trades.csv", result, &error), "trades export should succeed");

    stockbt::DatasetMetadata dataset;
    dataset.rows = import.candles.size();
    dataset.start_ts = import.candles.front().ts;
    dataset.end_ts = import.candles.back().ts;

    check_true(stockbt::export_metrics_json("tests/tmp/metrics.json", dataset, params, settings, result.metrics, &error),
               "metrics export should succeed");

    const std::string expected_trades = read_all("tests/golden/trades.csv");
    const std::string actual_trades = read_all("tests/tmp/trades.csv");
    check_true(expected_trades == actual_trades, "trades.csv should match golden output");

    const std::string expected_metrics = read_all("tests/golden/metrics.json");
    const std::string actual_metrics = read_all("tests/tmp/metrics.json");
    check_true(expected_metrics == actual_metrics, "metrics.json should match golden output");
}

} // namespace

int main() {
    test_timestamp_format();
    test_import_filtering_sort_and_duplicates();
    test_force_close_end_long();
    test_short_dataset_behavior();
    test_drawdown_units();
    test_regression_goldens();

    if (g_failures == 0) {
        std::cout << "All tests passed\n";
        return 0;
    }

    std::cerr << g_failures << " test(s) failed\n";
    return 1;
}
