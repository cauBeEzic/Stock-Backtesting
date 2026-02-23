#pragma once

#include <QFutureWatcher>
#include <QMainWindow>

#include <QtCharts/QChartView>

#include "backtest/types.hpp"

QT_BEGIN_NAMESPACE
class QAction;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void on_open_csv();
    void on_run_backtest();
    void on_export_results();

private:
    void setup_ui();
    void setup_actions();

    stockbt::DateFormat selected_date_format() const;
    stockbt::SmaParams current_sma_params() const;
    stockbt::BacktestSettings current_settings() const;

    void append_log_line(const QString& line);
    void render_import_result(const stockbt::ImportResult& result);
    void render_backtest_result();
    void render_price_chart();
    void render_equity_chart();
    void render_drawdown_chart();
    void render_trades_table();

    QAction* open_action_{nullptr};
    QAction* run_action_{nullptr};
    QAction* export_action_{nullptr};

    QComboBox* date_format_combo_{nullptr};
    QSpinBox* fast_window_spin_{nullptr};
    QSpinBox* slow_window_spin_{nullptr};
    QDoubleSpinBox* cash_spin_{nullptr};
    QDoubleSpinBox* commission_spin_{nullptr};

    QLabel* dataset_summary_label_{nullptr};
    QPlainTextEdit* import_log_{nullptr};
    QChartView* price_chart_view_{nullptr};
    QChartView* equity_chart_view_{nullptr};
    QChartView* drawdown_chart_view_{nullptr};
    QTableWidget* trades_table_{nullptr};

    QString loaded_csv_path_;
    stockbt::Series candles_;
    stockbt::BacktestResult last_backtest_;

    QFutureWatcher<stockbt::ImportResult> import_watcher_;
    QFutureWatcher<stockbt::BacktestResult> backtest_watcher_;
};
