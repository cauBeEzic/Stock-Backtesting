# Stock-Backtesting

C++17 backtesting engine with a Qt desktop shell for importing OHLCV CSV data, running deterministic SMA crossover backtests, visualizing outputs, and exporting reproducible results.

## Implemented MVP scope

- `core` static library (no Qt dependency)
- CSV import with validation and deterministic normalization
- SMA crossover backtester (long-only, next-open execution)
- Portfolio accounting with commission and integer position sizing
- Risk controls:
  - position size percentage
  - stop-loss percentage
  - take-profit percentage
- Metrics (`total_return_pct`, `total_pnl`, `trades`, `win_rate_pct`, `avg_trade_return_pct`, `max_drawdown_pct`)
- Exporters:
  - `equity.csv`
  - `trades.csv`
  - `metrics.json`
- Deterministic UTC timestamp parsing/formatting (`ISO-8601 ...Z`)
- Downsampling utility (bucket min/max)
- Unit + regression tests (`core_tests`)
- Golden regeneration utility (`tools/regenerate_goldens`)
- Performance benchmark utility (`tools/benchmark_mvp`)
- Parameter sweep utility with out-of-sample report (`tools/parameter_sweep`)

## Project structure

- `core/include/backtest`: public headers
- `core/src`: importer, engine, exporters, utilities
- `app/src`: Qt desktop UI shell (`QMainWindow`, background workers)
- `tests`: unit + golden regression tests
- `data`: sample CSV fixtures
- `tests/golden`: committed regression artifacts
- `tools/regenerate_goldens`: script to regenerate golden files

## Build

### CMake (recommended)

```bash
cmake -S . -B build -DBUILD_APP=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

Set `-DBUILD_APP=ON` (default) when Qt6 (`Widgets`, `Concurrent`, `Charts`) is available.

### Direct g++ fallback (used in this environment)

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -Icore/include \
  core/src/csv_importer.cpp core/src/backtester.cpp core/src/exporter.cpp \
  core/src/downsampling.cpp core/src/time_utils.cpp tests/core_tests.cpp \
  -o /tmp/core_tests

/tmp/core_tests
```

## Golden workflow

Regenerate committed golden files after intentional strategy/export logic changes:

```bash
tools/regenerate_goldens
```

This command updates:

- `tests/golden/equity.csv`
- `tests/golden/trades.csv`
- `tests/golden/metrics.json`

Review diffs before committing.

## Performance check (200k rows)

Run the benchmark utility to validate import/backtest performance targets:

```bash
cmake --build build --target benchmark_mvp
./build/tools/benchmark_mvp 200000
```

Example output includes:

- rows imported
- import duration (ms)
- backtest duration (ms)

## Parameter sweep (train/test report)

Generate an out-of-sample report across SMA parameter ranges:

```bash
tools/parameter_sweep data/USDCAD.csv data/sweep_report.csv iso 0.7 5 80 20 300 5 0.5 0.01 0.02
```

Arguments:

- `csv_path`
- `out_csv`
- `date_format` (`iso`, `mdy`, `dmy`)
- `train_ratio` (e.g. `0.7`)
- `fast_min`, `fast_max`, `slow_min`, `slow_max`, `step`
- `position_size_pct`, `stop_loss_pct`, `take_profit_pct`

Output columns:

- `fast,slow`
- train: `return_pct,max_drawdown_pct,trades`
- test: `return_pct,max_drawdown_pct,trades`

## Notes on determinism

- Naive input timestamps are interpreted as UTC.
- Identical inputs and settings produce identical outputs.
- Drawdown is stored internally as fraction in `[-1, 0]`; `max_drawdown_pct` is `min(drawdown) * 100`.

## Disclaimer

Educational tool. Not investment advice. No live trading.
