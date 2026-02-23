# Stock-Backtesting

C++17 backtesting engine with a Qt desktop shell for importing OHLCV CSV data, running deterministic SMA crossover backtests, visualizing outputs, and exporting reproducible results.

## Implemented MVP scope

- `core` static library (no Qt dependency)
- CSV import with validation and deterministic normalization
- SMA crossover backtester (long-only, next-open execution)
- Portfolio accounting with commission and integer position sizing
- Metrics (`total_return_pct`, `total_pnl`, `trades`, `win_rate_pct`, `avg_trade_return_pct`, `max_drawdown_pct`)
- Exporters:
  - `equity.csv`
  - `trades.csv`
  - `metrics.json`
- Deterministic UTC timestamp parsing/formatting (`ISO-8601 ...Z`)
- Downsampling utility (bucket min/max)
- Unit + regression tests (`core_tests`)
- Golden regeneration utility (`tools/regenerate_goldens`)

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

- `tests/golden/trades.csv`
- `tests/golden/metrics.json`

Review diffs before committing.

## Notes on determinism

- Naive input timestamps are interpreted as UTC.
- Identical inputs and settings produce identical outputs.
- Drawdown is stored internally as fraction in `[-1, 0]`; `max_drawdown_pct` is `min(drawdown) * 100`.

## Disclaimer

Educational tool. Not investment advice. No live trading.
