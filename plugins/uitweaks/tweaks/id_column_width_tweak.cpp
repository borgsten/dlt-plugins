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

/*
 * Characters IDs are assumed to consist of common characters..
 */
constexpr QLatin1StringView kIdChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-");

int minimumIdWidth(const QTableView* table) {
    const QFontMetrics metrics(table->font());
    int widestChar = 0;
    for (char const c : kIdChars) {
        widestChar = qMax(widestChar, metrics.horizontalAdvance(QLatin1Char(c)));
    }

    // Item views draw text inset by this margin on both sides.
    const int textMargin = table->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, table) + 1;
    // The grid line is drawn inside the cell.
    const int grid = table->showGrid() ? 1 : 0;
    return (kIdLength * widestChar) + (2 * textMargin) + grid;
}

}  // namespace

IdColumnWidthTweak::IdColumnWidthTweak(QWidget* mainWindow, QStringList tableNames, QList<int> columns)
    : m_mainWindow(mainWindow), m_tableNames(std::move(tableNames)), m_columns(std::move(columns)) {}

void IdColumnWidthTweak::apply() {
    if (m_applied || !m_mainWindow) {
        return;
    }

    for (const QString& name : std::as_const(m_tableNames)) {
        auto* table = m_mainWindow->findChild<QTableView*>(name);
        if (table == nullptr) {
            qWarning() << "UI Tweaks: table not found:" << name;
            continue;
        }
        m_tables.append(table);

        // Listen to font change events.
        table->installEventFilter(this);

        const QHeaderView* header = table->horizontalHeader();
        m_connections.append(connect(header, &QHeaderView::sectionResized, this, [this](int logicalIndex) {
            if (m_columns.contains(logicalIndex)) {
                scheduleEnforce();
            }
        }));
        m_connections.append(
            connect(header, &QHeaderView::sectionCountChanged, this, &IdColumnWidthTweak::scheduleEnforce));
    }

    m_applied = true;
    enforce();
}

void IdColumnWidthTweak::revert() {
    if (!m_applied) {
        return;
    }

    for (const QMetaObject::Connection& connection : std::as_const(m_connections)) {
        disconnect(connection);
    }
    m_connections.clear();

    for (const QPointer<QTableView>& table : std::as_const(m_tables)) {
        if (table) {
            table->removeEventFilter(this);
        }
    }
    m_tables.clear();
    m_applied = false;
}

bool IdColumnWidthTweak::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        scheduleEnforce();
    }
    return QObject::eventFilter(watched, event);
}

void IdColumnWidthTweak::scheduleEnforce() {
    // Deferred so that it runs after the viewer is done changing the table.
    if (m_enforcePending) {
        return;
    }
    m_enforcePending = true;
    QTimer::singleShot(0, this, [this] {
        m_enforcePending = false;
        if (m_applied) {
            enforce();
        }
    });
}

void IdColumnWidthTweak::enforce() {
    for (const QPointer<QTableView>& table : std::as_const(m_tables)) {
        if (!table) {
            continue;
        }
        QHeaderView* header = table->horizontalHeader();
        const int minWidth = minimumIdWidth(table);
        for (int const column : std::as_const(m_columns)) {
            if (column >= header->count() || header->isSectionHidden(column)) {
                continue;
            }
            if (header->sectionSize(column) < minWidth) {
                header->resizeSection(column, minWidth);
            }
        }
    }
}
