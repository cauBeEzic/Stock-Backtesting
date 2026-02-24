#include "backtest/backtester.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace stockbt {
namespace {

enum class PendingAction {
    None,
    Buy,
    Sell,
};

double clamp01(double value) {
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

} // namespace

BacktestResult run_sma_backtest(const Series& candles,
                                const SmaParams& params,
                                const BacktestSettings& settings) {
    BacktestResult result;
    if (candles.empty()) {
        result.warnings.push_back("Backtest skipped: empty dataset.");
        return result;
    }
    if (!params.is_valid()) {
        result.warnings.push_back("Backtest skipped: invalid SMA parameters (require fast < slow and > 0).");
        return result;
    }

    const std::size_t n = candles.size();
    result.equity.assign(n, settings.starting_cash);
    result.drawdown.assign(n, 0.0);

    if (n < params.slow_window) {
        result.warnings.push_back("Dataset length is below slow_window. No signals/trades generated.");
    }

    double cash = settings.starting_cash;
    int qty = 0;
    const double position_size_pct = clamp01(settings.position_size_pct);
    const bool stop_loss_enabled = settings.stop_loss_pct > 0.0;
    const bool take_profit_enabled = settings.take_profit_pct > 0.0;

    int64_t open_entry_time = 0;
    double open_entry_price = 0.0;

    PendingAction pending = PendingAction::None;

    double fast_sum = 0.0;
    double slow_sum = 0.0;
    double prev_fast = 0.0;
    double prev_slow = 0.0;
    bool prev_valid = false;

    for (std::size_t i = 0; i < n; ++i) {
        const Candle& bar = candles[i];

        if (pending == PendingAction::Buy) {
            const double entry_price = bar.o;
            const double denom = entry_price * (1.0 + settings.commission_pct);
            const double budget = cash * position_size_pct;
            const int buy_qty = (denom > 0.0) ? static_cast<int>(std::floor(budget / denom)) : 0;
            if (buy_qty > 0) {
                const double cost = static_cast<double>(buy_qty) * entry_price;
                const double commission = cost * settings.commission_pct;
                cash -= (cost + commission);
                qty = buy_qty;
                open_entry_time = bar.ts;
                open_entry_price = entry_price;
            }
            pending = PendingAction::None;
        } else if (pending == PendingAction::Sell) {
            if (qty > 0) {
                const double exit_price = bar.o;
                const double proceeds = static_cast<double>(qty) * exit_price;
                const double commission = proceeds * settings.commission_pct;
                cash += (proceeds - commission);

                Trade trade;
                trade.entry_time = open_entry_time;
                trade.entry_price = open_entry_price;
                trade.exit_time = bar.ts;
                trade.exit_price = exit_price;
                trade.qty = qty;
                trade.pnl = (exit_price - open_entry_price) * static_cast<double>(qty) -
                            (open_entry_price * static_cast<double>(qty) * settings.commission_pct) -
                            (exit_price * static_cast<double>(qty) * settings.commission_pct);
                trade.return_pct = (open_entry_price > 0.0) ? ((exit_price - open_entry_price) / open_entry_price) : 0.0;
                result.trades.push_back(trade);
                qty = 0;
                open_entry_time = 0;
                open_entry_price = 0.0;
            }
            pending = PendingAction::None;
        }

        fast_sum += bar.c;
        slow_sum += bar.c;
        if (i >= params.fast_window) {
            fast_sum -= candles[i - params.fast_window].c;
        }
        if (i >= params.slow_window) {
            slow_sum -= candles[i - params.slow_window].c;
        }

        const bool fast_valid = i + 1 >= params.fast_window;
        const bool slow_valid = i + 1 >= params.slow_window;

        if (fast_valid && slow_valid) {
            const double fast = fast_sum / static_cast<double>(params.fast_window);
            const double slow = slow_sum / static_cast<double>(params.slow_window);

            if (prev_valid) {
                const bool cross_up = (prev_fast <= prev_slow) && (fast > slow);
                const bool cross_down = (prev_fast >= prev_slow) && (fast < slow);

                if (cross_up && qty == 0 && pending == PendingAction::None) {
                    if (i + 1 < n) {
                        pending = PendingAction::Buy;
                    } else {
                        result.warnings.push_back("Last bar signal discarded (no next bar for execution).");
                    }
                } else if (cross_down && qty > 0 && pending == PendingAction::None) {
                    if (i + 1 < n) {
                        pending = PendingAction::Sell;
                    } else {
                        result.warnings.push_back("Last bar signal discarded (no next bar for execution).");
                    }
                }
            }

            prev_fast = fast;
            prev_slow = slow;
            prev_valid = true;
        }

        if (qty > 0 && pending == PendingAction::None) {
            const double bar_return = (open_entry_price > 0.0) ? ((bar.c - open_entry_price) / open_entry_price) : 0.0;
            if (stop_loss_enabled && bar_return <= -settings.stop_loss_pct) {
                if (i + 1 < n) {
                    pending = PendingAction::Sell;
                    result.warnings.push_back("Stop-loss triggered; exit scheduled on next bar open.");
                } else {
                    result.warnings.push_back("Stop-loss triggered on last bar; exiting at final close.");
                }
            } else if (take_profit_enabled && bar_return >= settings.take_profit_pct) {
                if (i + 1 < n) {
                    pending = PendingAction::Sell;
                    result.warnings.push_back("Take-profit triggered; exit scheduled on next bar open.");
                } else {
                    result.warnings.push_back("Take-profit triggered on last bar; exiting at final close.");
                }
            }
        }

        result.equity[i] = cash + static_cast<double>(qty) * bar.c;
    }

    if (qty > 0) {
        const Candle& last = candles.back();
        const double exit_price = last.c;
        const double proceeds = static_cast<double>(qty) * exit_price;
        const double commission = proceeds * settings.commission_pct;
        cash += (proceeds - commission);

        Trade trade;
        trade.entry_time = open_entry_time;
        trade.entry_price = open_entry_price;
        trade.exit_time = last.ts;
        trade.exit_price = exit_price;
        trade.qty = qty;
        trade.pnl = (exit_price - open_entry_price) * static_cast<double>(qty) -
                    (open_entry_price * static_cast<double>(qty) * settings.commission_pct) -
                    (exit_price * static_cast<double>(qty) * settings.commission_pct);
        trade.return_pct = (open_entry_price > 0.0) ? ((exit_price - open_entry_price) / open_entry_price) : 0.0;
        result.trades.push_back(trade);

        qty = 0;
        result.equity.back() = cash;
        result.warnings.push_back("Open position force-closed at last bar close.");
    }

    double peak = -std::numeric_limits<double>::infinity();
    double min_dd = 0.0;
    for (std::size_t i = 0; i < result.equity.size(); ++i) {
        peak = std::max(peak, result.equity[i]);
        const double dd = (peak > 0.0) ? (result.equity[i] - peak) / peak : 0.0;
        result.drawdown[i] = dd;
        min_dd = std::min(min_dd, dd);
    }

    const double final_equity = result.equity.empty() ? settings.starting_cash : result.equity.back();
    result.metrics.total_pnl = final_equity - settings.starting_cash;
    result.metrics.total_return_pct = (settings.starting_cash != 0.0)
                                          ? (result.metrics.total_pnl / settings.starting_cash) * 100.0
                                          : 0.0;
    result.metrics.trades = static_cast<int>(result.trades.size());

    int wins = 0;
    double sum_returns = 0.0;
    for (const Trade& trade : result.trades) {
        if (trade.pnl > 0.0) {
            ++wins;
        }
        sum_returns += trade.return_pct;
    }
    result.metrics.win_rate_pct = result.trades.empty()
                                      ? 0.0
                                      : (static_cast<double>(wins) / static_cast<double>(result.trades.size())) * 100.0;
    result.metrics.avg_trade_return_pct = result.trades.empty()
                                              ? 0.0
                                              : (sum_returns / static_cast<double>(result.trades.size())) * 100.0;
    result.metrics.max_drawdown_pct = min_dd * 100.0;

    return result;
}

} // namespace stockbt
