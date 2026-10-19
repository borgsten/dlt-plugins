#include "tweaks/id_column_width_tweak.h"

#include <utility>

#include <QDebug>
#include <QEvent>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLatin1Char>
#include <QLatin1StringView>
#include <QStyle>
#include <QTimer>
#include <QWidget>
#include <QtLogging>
#include <QtMinMax>

namespace {

constexpr int kIdLength = 4;

constexpr QLatin1StringView kIdChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-");

int minimumIdWidth(const QTableView* table) {
    const QFontMetrics metrics(table->font());
    int widest_char = 0;
    for (char const c : kIdChars) {
        widest_char = qMax(widest_char, metrics.horizontalAdvance(QLatin1Char(c)));
    }

    // Item views draw text inset by this margin on both sides.
    const int text_margin = table->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, table) + 1;
    // The grid line is drawn inside the cell.
    const int grid = table->showGrid() ? 1 : 0;
    return (kIdLength * widest_char) + (2 * text_margin) + grid;
}

}  // namespace

IdColumnWidthTweak::IdColumnWidthTweak(QWidget* main_window, QStringList table_names, QList<int> columns)
    : main_window_(main_window), table_names_(std::move(table_names)), columns_(std::move(columns)) {}

void IdColumnWidthTweak::apply() {
    if (applied_ || !main_window_) {
        return;
    }

    for (const QString& name : std::as_const(table_names_)) {
        auto* table = main_window_->findChild<QTableView*>(name);
        if (table == nullptr) {
            qWarning() << "UI Tweaks: table not found:" << name;
            continue;
        }
        tables_.append(table);

        table->installEventFilter(this);

        const QHeaderView* header = table->horizontalHeader();
        connections_.append(connect(header, &QHeaderView::sectionResized, this, [this](int logical_index) {
            if (columns_.contains(logical_index)) {
                scheduleEnforce();
            }
        }));
        connections_.append(
            connect(header, &QHeaderView::sectionCountChanged, this, &IdColumnWidthTweak::scheduleEnforce));
    }

    applied_ = true;
    enforce();
}

void IdColumnWidthTweak::revert() {
    if (!applied_) {
        return;
    }

    for (const QMetaObject::Connection& connection : std::as_const(connections_)) {
        disconnect(connection);
    }
    connections_.clear();

    for (const QPointer<QTableView>& table : std::as_const(tables_)) {
        if (table) {
            table->removeEventFilter(this);
        }
    }
    tables_.clear();
    applied_ = false;
}

bool IdColumnWidthTweak::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        scheduleEnforce();
    }
    return QObject::eventFilter(watched, event);
}

void IdColumnWidthTweak::scheduleEnforce() {
    // Deferred so that it runs after the viewer is done changing the table.
    if (enforce_pending_) {
        return;
    }
    enforce_pending_ = true;
    QTimer::singleShot(0, this, [this] {
        enforce_pending_ = false;
        if (applied_) {
            enforce();
        }
    });
}

void IdColumnWidthTweak::enforce() {
    for (const QPointer<QTableView>& table : std::as_const(tables_)) {
        if (!table) {
            continue;
        }
        QHeaderView* header = table->horizontalHeader();
        const int min_width = minimumIdWidth(table);
        for (int const column : std::as_const(columns_)) {
            if (column >= header->count() || header->isSectionHidden(column)) {
                continue;
            }
            if (header->sectionSize(column) < min_width) {
                header->resizeSection(column, min_width);
            }
        }
    }
}
