#include "main_window.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
#include <QtConcurrent/QtConcurrent>

#include "backtest/backtester.hpp"
#include "backtest/csv_importer.hpp"
#include "backtest/downsampling.hpp"
#include "backtest/exporter.hpp"
#include "backtest/time_utils.hpp"

namespace {

constexpr std::size_t kDisplayCap = 50000;

class SortableNumericItem final : public QTableWidgetItem {
public:
    explicit SortableNumericItem(const QString& text) : QTableWidgetItem(text) {}

    bool operator<(const QTableWidgetItem& other) const override {
        const QVariant lhs = data(Qt::UserRole);
        const QVariant rhs = other.data(Qt::UserRole);
        if (lhs.isValid() && rhs.isValid()) {
            bool lhs_ok = false;
            bool rhs_ok = false;
            const double lhs_val = lhs.toDouble(&lhs_ok);
            const double rhs_val = rhs.toDouble(&rhs_ok);
            if (lhs_ok && rhs_ok) {
                return lhs_val < rhs_val;
            }
        }
        return QTableWidgetItem::operator<(other);
    }
};

QString issue_to_qstring(const stockbt::ImportIssue& issue) {
    if (issue.line == 0) {
        return QString::fromStdString(issue.message);
    }
    return QString("line %1: %2").arg(issue.line).arg(QString::fromStdString(issue.message));
}

std::vector<QPointF> display_points(const std::vector<stockbt::SeriesPoint>& src, int pixel_width) {
    std::vector<QPointF> out;
    if (src.empty()) {
        return out;
    }

    if (src.size() <= kDisplayCap) {
        out.reserve(src.size());
        for (const auto& p : src) {
            out.emplace_back(static_cast<qreal>(p.ts), p.value);
        }
        return out;
    }

    const auto buckets =
        stockbt::downsample_bucket_min_max(src, static_cast<std::size_t>(std::max(1, pixel_width)), kDisplayCap);
    out.reserve(buckets.size() * 2);
    for (const auto& bucket : buckets) {
        if (bucket.min_ts <= bucket.max_ts) {
            out.emplace_back(static_cast<qreal>(bucket.min_ts), bucket.min_value);
            out.emplace_back(static_cast<qreal>(bucket.max_ts), bucket.max_value);
        } else {
            out.emplace_back(static_cast<qreal>(bucket.max_ts), bucket.max_value);
            out.emplace_back(static_cast<qreal>(bucket.min_ts), bucket.min_value);
        }
    }
    return out;
}

std::vector<stockbt::SeriesPoint> points_in_visible_range(const std::vector<stockbt::SeriesPoint>& src,
                                                          qreal min_x,
                                                          qreal max_x) {
    if (src.empty()) {
        return {};
    }
    if (min_x > max_x) {
        std::swap(min_x, max_x);
    }

    const int64_t min_ts = static_cast<int64_t>(std::floor(min_x));
    const int64_t max_ts = static_cast<int64_t>(std::ceil(max_x));

    const auto begin = std::lower_bound(src.begin(), src.end(), min_ts, [](const stockbt::SeriesPoint& point, int64_t ts) {
        return point.ts < ts;
    });
    const auto end = std::upper_bound(src.begin(), src.end(), max_ts, [](int64_t ts, const stockbt::SeriesPoint& point) {
        return ts < point.ts;
    });
    return std::vector<stockbt::SeriesPoint>(begin, end);
}

void set_x_axis_full_range(QValueAxis* axis_x, const std::vector<stockbt::SeriesPoint>& src) {
    if (src.empty()) {
        axis_x->setRange(0.0, 1.0);
        return;
    }
    const qreal min_x = static_cast<qreal>(src.front().ts);
    const qreal max_x = static_cast<qreal>(src.back().ts);
    if (min_x == max_x) {
        axis_x->setRange(min_x - 1.0, max_x + 1.0);
    } else {
        axis_x->setRange(min_x, max_x);
    }
}

void update_y_axis(QValueAxis* axis_y, const std::vector<QPointF>& points) {
    if (points.empty()) {
        axis_y->setRange(0.0, 1.0);
        return;
    }

    qreal min_v = std::numeric_limits<qreal>::infinity();
    qreal max_v = -std::numeric_limits<qreal>::infinity();
    for (const QPointF& point : points) {
        min_v = std::min(min_v, point.y());
        max_v = std::max(max_v, point.y());
    }

    if (min_v == max_v) {
        const qreal pad = (std::abs(min_v) > 1e-9) ? std::abs(min_v) * 0.01 : 1.0;
        axis_y->setRange(min_v - pad, max_v + pad);
        return;
    }

    const qreal pad = (max_v - min_v) * 0.05;
    axis_y->setRange(min_v - pad, max_v + pad);
}

void refresh_line_series(QLineSeries* line,
                         QValueAxis* axis_y,
                         const std::vector<stockbt::SeriesPoint>& source,
                         qreal min_x,
                         qreal max_x,
                         int pixel_width) {
    const auto visible = points_in_visible_range(source, min_x, max_x);
    const auto& to_plot = visible.empty() ? source : visible;
    const auto points = display_points(to_plot, pixel_width);

    line->clear();
    for (const QPointF& point : points) {
        line->append(point);
    }
    update_y_axis(axis_y, points);
}

QString format_ts(int64_t ts) {
    return QString::fromStdString(stockbt::format_timestamp_utc_iso8601(ts));
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setup_ui();
    setup_actions();

    connect(&import_watcher_, &QFutureWatcher<stockbt::ImportResult>::finished, this, [this]() {
        const stockbt::ImportResult result = import_watcher_.result();
        render_import_result(result);
    });

    connect(&backtest_watcher_, &QFutureWatcher<stockbt::BacktestResult>::finished, this, [this]() {
        last_backtest_ = backtest_watcher_.result();
        render_backtest_result();
        run_action_->setEnabled(!candles_.empty());
        export_action_->setEnabled(!last_backtest_.equity.empty());
        statusBar()->showMessage("Done", 3000);
    });
}

void MainWindow::setup_ui() {
    setWindowTitle("Stock & Crypto Backtester (MVP)");
    resize(1280, 840);

    auto* controls = new QWidget(this);
    auto* controls_layout = new QFormLayout(controls);

    date_format_combo_ = new QComboBox(controls);
    date_format_combo_->addItem("ISO");
    date_format_combo_->addItem("MM/DD/YYYY");
    date_format_combo_->addItem("DD/MM/YYYY");

    fast_window_spin_ = new QSpinBox(controls);
    fast_window_spin_->setRange(1, 2000);
    fast_window_spin_->setValue(20);

    slow_window_spin_ = new QSpinBox(controls);
    slow_window_spin_->setRange(2, 4000);
    slow_window_spin_->setValue(50);

    cash_spin_ = new QDoubleSpinBox(controls);
    cash_spin_->setRange(1.0, 1000000000.0);
    cash_spin_->setValue(10000.0);

    commission_spin_ = new QDoubleSpinBox(controls);
    commission_spin_->setDecimals(6);
    commission_spin_->setRange(0.0, 0.2);
    commission_spin_->setSingleStep(0.0001);
    commission_spin_->setValue(0.001);

    dataset_summary_label_ = new QLabel("No dataset loaded", controls);
    dataset_summary_label_->setWordWrap(true);

    controls_layout->addRow("Date format", date_format_combo_);
    controls_layout->addRow("Fast SMA", fast_window_spin_);
    controls_layout->addRow("Slow SMA", slow_window_spin_);
    controls_layout->addRow("Starting cash", cash_spin_);
    controls_layout->addRow("Commission", commission_spin_);
    controls_layout->addRow("Dataset", dataset_summary_label_);

    auto* controls_dock = new QDockWidget("Controls", this);
    controls_dock->setWidget(controls);
    addDockWidget(Qt::LeftDockWidgetArea, controls_dock);

    auto* central_tabs = new QTabWidget(this);

    auto* chart_tabs = new QTabWidget(central_tabs);
    price_chart_view_ = new QChartView(chart_tabs);
    equity_chart_view_ = new QChartView(chart_tabs);
    drawdown_chart_view_ = new QChartView(chart_tabs);

    chart_tabs->addTab(price_chart_view_, "Price");
    chart_tabs->addTab(equity_chart_view_, "Equity");
    chart_tabs->addTab(drawdown_chart_view_, "Drawdown");

    trades_table_ = new QTableWidget(central_tabs);
    trades_table_->setColumnCount(7);
    trades_table_->setHorizontalHeaderLabels(
        {"Entry Time", "Entry Price", "Exit Time", "Exit Price", "Qty", "PnL", "Return %"});
    trades_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trades_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    trades_table_->setAlternatingRowColors(true);
    trades_table_->setSortingEnabled(true);

    price_chart_view_->setRubberBand(QChartView::RectangleRubberBand);
    equity_chart_view_->setRubberBand(QChartView::RectangleRubberBand);
    drawdown_chart_view_->setRubberBand(QChartView::RectangleRubberBand);

    auto* export_tab = new QWidget(central_tabs);
    auto* export_layout = new QVBoxLayout(export_tab);
    auto* export_text = new QLabel("Use File > Export to write equity.csv, trades.csv, and metrics.json.", export_tab);
    export_text->setWordWrap(true);
    export_layout->addWidget(export_text);
    export_layout->addStretch(1);

    central_tabs->addTab(chart_tabs, "Charts");
    central_tabs->addTab(trades_table_, "Trades");
    central_tabs->addTab(export_tab, "Export");
    setCentralWidget(central_tabs);

    import_log_ = new QPlainTextEdit(this);
    import_log_->setReadOnly(true);
    auto* log_dock = new QDockWidget("Import Log", this);
    log_dock->setWidget(import_log_);
    addDockWidget(Qt::BottomDockWidgetArea, log_dock);

    statusBar()->showMessage("Ready");
}

void MainWindow::setup_actions() {
    auto* file_menu = menuBar()->addMenu("File");
    auto* run_menu = menuBar()->addMenu("Run");
    auto* help_menu = menuBar()->addMenu("Help");

    open_action_ = file_menu->addAction("Open CSV");
    export_action_ = file_menu->addAction("Export");
    run_action_ = run_menu->addAction("Run Backtest");
    help_menu->addAction("About", this, [this]() {
        QMessageBox::information(this,
                                 "About",
                                 "Educational tool. Not investment advice. No live trading.");
    });

    run_action_->setEnabled(false);
    export_action_->setEnabled(false);

    connect(open_action_, &QAction::triggered, this, &MainWindow::on_open_csv);
    connect(run_action_, &QAction::triggered, this, &MainWindow::on_run_backtest);
    connect(export_action_, &QAction::triggered, this, &MainWindow::on_export_results);
}

stockbt::DateFormat MainWindow::selected_date_format() const {
    const int index = date_format_combo_->currentIndex();
    if (index == 1) {
        return stockbt::DateFormat::Mdy;
    }
    if (index == 2) {
        return stockbt::DateFormat::Dmy;
    }
    return stockbt::DateFormat::Iso;
}

stockbt::SmaParams MainWindow::current_sma_params() const {
    stockbt::SmaParams params;
    params.fast_window = static_cast<std::size_t>(fast_window_spin_->value());
    params.slow_window = static_cast<std::size_t>(slow_window_spin_->value());
    return params;
}

stockbt::BacktestSettings MainWindow::current_settings() const {
    stockbt::BacktestSettings settings;
    settings.starting_cash = cash_spin_->value();
    settings.commission_pct = commission_spin_->value();
    return settings;
}

void MainWindow::append_log_line(const QString& line) {
    import_log_->appendPlainText(line);
}

void MainWindow::on_open_csv() {
    const QString file = QFileDialog::getOpenFileName(this, "Open OHLCV CSV", QString(), "CSV Files (*.csv)");
    if (file.isEmpty()) {
        return;
    }

    loaded_csv_path_ = file;
    candles_.clear();
    last_backtest_ = stockbt::BacktestResult{};
    run_action_->setEnabled(false);
    export_action_->setEnabled(false);
    dataset_summary_label_->setText("Importing...");
    import_log_->clear();

    statusBar()->showMessage("Importing...");

    const std::string path = file.toStdString();
    const stockbt::DateFormat fmt = selected_date_format();
    import_watcher_.setFuture(QtConcurrent::run([path, fmt]() { return stockbt::import_ohlcv_csv(path, fmt); }));
}

void MainWindow::render_import_result(const stockbt::ImportResult& result) {
    for (const auto& warning : result.warnings) {
        append_log_line(issue_to_qstring(warning));
    }
    for (const auto& error : result.errors) {
        append_log_line(issue_to_qstring(error));
    }

    if (!result.success) {
        QStringList lines;
        const std::size_t cap = std::min<std::size_t>(20, result.errors.size());
        for (std::size_t i = 0; i < cap; ++i) {
            lines.append(issue_to_qstring(result.errors[i]));
        }
        QMessageBox::critical(this,
                              "Import Failed",
                              QString("Import failed:\n%1").arg(lines.join("\n")));
        dataset_summary_label_->setText("No dataset loaded");
        statusBar()->showMessage("Import failed", 5000);
        return;
    }

    candles_ = result.candles;
    QString summary = QString("Rows: %1\nStart: %2\nEnd: %3")
                          .arg(candles_.size())
                          .arg(format_ts(candles_.front().ts))
                          .arg(format_ts(candles_.back().ts));
    if (result.partial_success) {
        summary += QString("\nDropped rows: %1").arg(result.dropped_rows);
        statusBar()->showMessage("Import completed with warnings", 5000);
    } else {
        statusBar()->showMessage("Import complete", 3000);
    }

    dataset_summary_label_->setText(summary);
    run_action_->setEnabled(true);
}

void MainWindow::on_run_backtest() {
    if (candles_.empty()) {
        QMessageBox::warning(this, "No Data", "Load a valid CSV before running backtest.");
        return;
    }

    const stockbt::SmaParams params = current_sma_params();
    if (!params.is_valid()) {
        QMessageBox::warning(this, "Invalid Parameters", "Require fast_window < slow_window and both > 0.");
        return;
    }

    run_action_->setEnabled(false);
    export_action_->setEnabled(false);
    statusBar()->showMessage("Running backtest...");

    const stockbt::Series local_series = candles_;
    const stockbt::BacktestSettings settings = current_settings();

    backtest_watcher_.setFuture(
        QtConcurrent::run([local_series, params, settings]() { return stockbt::run_sma_backtest(local_series, params, settings); }));
}

void MainWindow::render_price_chart() {
    auto* chart = new QChart();
    chart->setTitle("Price (Close) + Buy/Sell markers");

    std::vector<stockbt::SeriesPoint> src;
    src.reserve(candles_.size());
    for (const auto& c : candles_) {
        src.push_back({c.ts, c.c});
    }

    auto* line = new QLineSeries(chart);
    line->setName("Close");
    chart->addSeries(line);

    auto* buys = new QScatterSeries(chart);
    buys->setName("Buy");
    buys->setMarkerSize(8.0);
    auto* sells = new QScatterSeries(chart);
    sells->setName("Sell");
    sells->setMarkerSize(8.0);

    for (const auto& t : last_backtest_.trades) {
        buys->append(static_cast<qreal>(t.entry_time), t.entry_price);
        sells->append(static_cast<qreal>(t.exit_time), t.exit_price);
    }

    chart->addSeries(buys);
    chart->addSeries(sells);

    auto* axis_x = new QValueAxis(chart);
    axis_x->setLabelFormat("%.0f");
    auto* axis_y = new QValueAxis(chart);

    chart->addAxis(axis_x, Qt::AlignBottom);
    chart->addAxis(axis_y, Qt::AlignLeft);
    line->attachAxis(axis_x);
    line->attachAxis(axis_y);
    buys->attachAxis(axis_x);
    buys->attachAxis(axis_y);
    sells->attachAxis(axis_x);
    sells->attachAxis(axis_y);

    set_x_axis_full_range(axis_x, src);
    const auto source = std::make_shared<std::vector<stockbt::SeriesPoint>>(std::move(src));
    auto refresh = [this, line, axis_x, axis_y, source]() {
        const int width = std::max(1, static_cast<int>(price_chart_view_->chart()->plotArea().width()));
        refresh_line_series(line, axis_y, *source, axis_x->min(), axis_x->max(), width);
    };
    connect(axis_x, &QValueAxis::rangeChanged, this, [refresh](qreal, qreal) { refresh(); });
    connect(chart, &QChart::plotAreaChanged, this, [refresh](const QRectF&) { refresh(); });
    refresh();

    price_chart_view_->setChart(chart);
}

void MainWindow::render_equity_chart() {
    auto* chart = new QChart();
    chart->setTitle("Equity Curve");

    std::vector<stockbt::SeriesPoint> src;
    const std::size_t count = std::min(candles_.size(), last_backtest_.equity.size());
    src.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        src.push_back({candles_[i].ts, last_backtest_.equity[i]});
    }

    auto* line = new QLineSeries(chart);
    chart->addSeries(line);
    auto* axis_x = new QValueAxis(chart);
    axis_x->setLabelFormat("%.0f");
    auto* axis_y = new QValueAxis(chart);
    chart->addAxis(axis_x, Qt::AlignBottom);
    chart->addAxis(axis_y, Qt::AlignLeft);
    line->attachAxis(axis_x);
    line->attachAxis(axis_y);

    set_x_axis_full_range(axis_x, src);
    const auto source = std::make_shared<std::vector<stockbt::SeriesPoint>>(std::move(src));
    auto refresh = [this, line, axis_x, axis_y, source]() {
        const int width = std::max(1, static_cast<int>(equity_chart_view_->chart()->plotArea().width()));
        refresh_line_series(line, axis_y, *source, axis_x->min(), axis_x->max(), width);
    };
    connect(axis_x, &QValueAxis::rangeChanged, this, [refresh](qreal, qreal) { refresh(); });
    connect(chart, &QChart::plotAreaChanged, this, [refresh](const QRectF&) { refresh(); });
    refresh();

    equity_chart_view_->setChart(chart);
}

void MainWindow::render_drawdown_chart() {
    auto* chart = new QChart();
    chart->setTitle("Drawdown (%)");

    std::vector<stockbt::SeriesPoint> src;
    const std::size_t count = std::min(candles_.size(), last_backtest_.drawdown.size());
    src.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        src.push_back({candles_[i].ts, last_backtest_.drawdown[i] * 100.0});
    }

    auto* line = new QLineSeries(chart);
    chart->addSeries(line);
    auto* axis_x = new QValueAxis(chart);
    axis_x->setLabelFormat("%.0f");
    auto* axis_y = new QValueAxis(chart);
    chart->addAxis(axis_x, Qt::AlignBottom);
    chart->addAxis(axis_y, Qt::AlignLeft);
    line->attachAxis(axis_x);
    line->attachAxis(axis_y);

    set_x_axis_full_range(axis_x, src);
    const auto source = std::make_shared<std::vector<stockbt::SeriesPoint>>(std::move(src));
    auto refresh = [this, line, axis_x, axis_y, source]() {
        const int width = std::max(1, static_cast<int>(drawdown_chart_view_->chart()->plotArea().width()));
        refresh_line_series(line, axis_y, *source, axis_x->min(), axis_x->max(), width);
    };
    connect(axis_x, &QValueAxis::rangeChanged, this, [refresh](qreal, qreal) { refresh(); });
    connect(chart, &QChart::plotAreaChanged, this, [refresh](const QRectF&) { refresh(); });
    refresh();

    drawdown_chart_view_->setChart(chart);
}

void MainWindow::render_trades_table() {
    trades_table_->setSortingEnabled(false);
    if (last_backtest_.trades.empty()) {
        trades_table_->setRowCount(1);
        auto* msg = new QTableWidgetItem("No trades generated for current data/parameters.");
        trades_table_->setItem(0, 0, msg);
        for (int c = 1; c < trades_table_->columnCount(); ++c) {
            trades_table_->setItem(0, c, new QTableWidgetItem(""));
        }
        trades_table_->resizeColumnsToContents();
        trades_table_->setSortingEnabled(true);
        return;
    }

    trades_table_->setRowCount(static_cast<int>(last_backtest_.trades.size()));
    int row = 0;
    for (const auto& t : last_backtest_.trades) {
        auto* entry_time = new SortableNumericItem(format_ts(t.entry_time));
        entry_time->setData(Qt::UserRole, static_cast<qlonglong>(t.entry_time));
        trades_table_->setItem(row, 0, entry_time);

        auto* entry_price = new SortableNumericItem(QString::number(t.entry_price, 'f', 6));
        entry_price->setData(Qt::UserRole, t.entry_price);
        trades_table_->setItem(row, 1, entry_price);

        auto* exit_time = new SortableNumericItem(format_ts(t.exit_time));
        exit_time->setData(Qt::UserRole, static_cast<qlonglong>(t.exit_time));
        trades_table_->setItem(row, 2, exit_time);

        auto* exit_price = new SortableNumericItem(QString::number(t.exit_price, 'f', 6));
        exit_price->setData(Qt::UserRole, t.exit_price);
        trades_table_->setItem(row, 3, exit_price);

        auto* qty = new SortableNumericItem(QString::number(t.qty));
        qty->setData(Qt::UserRole, t.qty);
        trades_table_->setItem(row, 4, qty);

        auto* pnl = new SortableNumericItem(QString::number(t.pnl, 'f', 6));
        pnl->setData(Qt::UserRole, t.pnl);
        trades_table_->setItem(row, 5, pnl);

        auto* ret = new SortableNumericItem(QString::number(t.return_pct * 100.0, 'f', 4));
        ret->setData(Qt::UserRole, t.return_pct * 100.0);
        trades_table_->setItem(row, 6, ret);

        ++row;
    }
    trades_table_->resizeColumnsToContents();
    trades_table_->setSortingEnabled(true);
}

void MainWindow::render_backtest_result() {
    for (const auto& warning : last_backtest_.warnings) {
        append_log_line(QString::fromStdString(warning));
    }

    if (last_backtest_.trades.empty()) {
        const stockbt::SmaParams params = current_sma_params();
        if (candles_.size() < params.slow_window) {
            const QString hint =
                QString("No trades: dataset has %1 bars but slow_window is %2. Use smaller windows (e.g., fast=2, slow=3) "
                        "or load a longer dataset.")
                    .arg(candles_.size())
                    .arg(params.slow_window);
            append_log_line(hint);
            statusBar()->showMessage(hint, 10000);
        } else {
            const QString hint = "No trades: no SMA crossovers met entry/exit conditions for current parameters.";
            append_log_line(hint);
            statusBar()->showMessage(hint, 8000);
        }
    }

    render_price_chart();
    render_equity_chart();
    render_drawdown_chart();
    render_trades_table();

    QString summary = dataset_summary_label_->text();
    summary += QString("\nTrades: %1 | Return: %2% | MaxDD: %3%")
                   .arg(last_backtest_.metrics.trades)
                   .arg(QString::number(last_backtest_.metrics.total_return_pct, 'f', 2))
                   .arg(QString::number(last_backtest_.metrics.max_drawdown_pct, 'f', 2));
    dataset_summary_label_->setText(summary);
}

void MainWindow::on_export_results() {
    if (candles_.empty() || last_backtest_.equity.empty()) {
        QMessageBox::warning(this, "No Results", "Run a backtest before exporting.");
        return;
    }

    const QString out_dir = QFileDialog::getExistingDirectory(this, "Select Export Folder");
    if (out_dir.isEmpty()) {
        return;
    }

    std::string error;
    const std::string base = QDir(out_dir).absolutePath().toStdString();

    const bool ok_equity = stockbt::export_equity_csv(base + "/equity.csv", candles_, last_backtest_, &error);
    if (!ok_equity) {
        QMessageBox::critical(this, "Export Failed", QString::fromStdString(error));
        return;
    }

    const bool ok_trades = stockbt::export_trades_csv(base + "/trades.csv", last_backtest_, &error);
    if (!ok_trades) {
        QMessageBox::critical(this, "Export Failed", QString::fromStdString(error));
        return;
    }

    stockbt::DatasetMetadata dataset;
    dataset.rows = candles_.size();
    dataset.start_ts = candles_.front().ts;
    dataset.end_ts = candles_.back().ts;

    const bool ok_metrics = stockbt::export_metrics_json(
        base + "/metrics.json", dataset, current_sma_params(), current_settings(), last_backtest_.metrics, &error);
    if (!ok_metrics) {
        QMessageBox::critical(this, "Export Failed", QString::fromStdString(error));
        return;
    }

    append_log_line(QString("Exported files to %1").arg(out_dir));
    statusBar()->showMessage("Export complete", 3000);
}
