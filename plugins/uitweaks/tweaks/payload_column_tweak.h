#pragma once

#include <QEvent>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTableView>
#include <QWidget>

#include "tweaks/itweak.h"

/*
 * Sizes a column, the payload, to fill the rest of each table's width, and
 * keeps it filling as the table or the other columns change, until the user
 * resizes it. Reverting leaves the widths as they are.
 */
class PayloadColumnTweak : public QObject, public ITweak {
    Q_OBJECT

 public:
    PayloadColumnTweak(QWidget* main_window, QStringList table_names, int column);

    void apply() override;
    void revert() override;
    void buildSettings(plugin_utils::SettingsForm& /*form*/) override {}

    bool eventFilter(QObject* watched, QEvent* event) override;

 private:
    struct Table {
        QPointer<QTableView> view;
        bool user_sized = false;
        bool fit_pending = false;
    };

    void scheduleFit(Table& table);
    void fit(Table& table);
    Table* tableFor(const QObject* object);

    QPointer<QWidget> main_window_;
    QStringList table_names_;
    int column_;
    bool applied_ = false;
    bool resizing_ = false;
    QList<Table> tables_;
    QList<QMetaObject::Connection> connections_;
};
