#include "tweaks/payload_column_tweak.h"

#include <algorithm>
#include <utility>

#include <QDebug>
#include <QGuiApplication>
#include <QHeaderView>
#include <QTimer>

PayloadColumnTweak::PayloadColumnTweak(QWidget* main_window, QStringList table_names, int column)
    : main_window_(main_window), table_names_(std::move(table_names)), column_(column) {}

void PayloadColumnTweak::apply() {
    if (applied_ || !main_window_) {
        return;
    }
    applied_ = true;

    for (const QString& name : std::as_const(table_names_)) {
        auto* view = main_window_->findChild<QTableView*>(name);
        if (view == nullptr) {
            qWarning() << "UI Tweaks: table not found:" << name;
            continue;
        }
        tables_.append({.view = view});
    }

    // tables_ does not change until revert(), so the entries stay put.
    for (Table& table : tables_) {
        // Resizes when the window gets its size, and when the vertical scroll bar comes and goes.
        table.view->viewport()->installEventFilter(this);

        const QHeaderView* header = table.view->horizontalHeader();
        connections_.append(
            connect(header, &QHeaderView::sectionResized, this, [this, table_ptr = &table](int logical_index) {
                if (resizing_) {
                    return;
                }
                // The viewer resizes the column too; only a drag or double
                // click on the header is the user's.
                if (logical_index == column_ && QGuiApplication::mouseButtons() != Qt::NoButton) {
                    table_ptr->user_sized = true;
                } else {
                    scheduleFit(*table_ptr);
                }
            }));
        connections_.append(connect(header, &QHeaderView::sectionCountChanged, this,
                                    [this, table_ptr = &table] { scheduleFit(*table_ptr); }));
        scheduleFit(table);
    }
}

void PayloadColumnTweak::revert() {
    if (!applied_) {
        return;
    }
    applied_ = false;

    for (const QMetaObject::Connection& connection : std::as_const(connections_)) {
        disconnect(connection);
    }
    connections_.clear();
    for (const Table& table : std::as_const(tables_)) {
        if (table.view) {
            table.view->viewport()->removeEventFilter(this);
        }
    }
    tables_.clear();
}

bool PayloadColumnTweak::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Resize) {
        if (Table* table = tableFor(watched)) {
            scheduleFit(*table);
        }
    }
    return QObject::eventFilter(watched, event);
}

void PayloadColumnTweak::scheduleFit(Table& table) {
    // Deferred so that it runs after the viewer, and other tweaks, are done
    // changing the other columns.
    if (table.user_sized || table.fit_pending) {
        return;
    }
    table.fit_pending = true;
    QTimer::singleShot(0, this, [this, view = table.view] {
        if (Table* pending = tableFor(view ? view->viewport() : nullptr)) {
            pending->fit_pending = false;
            fit(*pending);
        }
    });
}

void PayloadColumnTweak::fit(Table& table) {
    if (table.user_sized || !table.view) {
        return;
    }
    QHeaderView* header = table.view->horizontalHeader();
    if (column_ >= header->count() || header->isSectionHidden(column_)) {
        return;
    }
    const int others = header->length() - header->sectionSize(column_);
    const int width = std::max(header->minimumSectionSize(), table.view->viewport()->width() - others);
    if (width == header->sectionSize(column_)) {
        return;
    }
    resizing_ = true;
    header->resizeSection(column_, width);
    resizing_ = false;
}

PayloadColumnTweak::Table* PayloadColumnTweak::tableFor(const QObject* object) {
    for (Table& table : tables_) {
        if (table.view && table.view->viewport() == object) {
            return &table;
        }
    }
    return nullptr;
}
