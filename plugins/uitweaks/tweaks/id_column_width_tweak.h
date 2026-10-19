#pragma once

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTableView>

#include "tweaks/itweak.h"

/*
 * Keeps ID columns wide enough for any 4 character ID made of [a-zA-Z0-9_-]
 * in the table's current font.
 */
class IdColumnWidthTweak : public QObject, public ITweak {
    Q_OBJECT

 public:
    IdColumnWidthTweak(QWidget* main_window, QStringList table_names, QList<int> columns);

    void apply() override;
    void revert() override;
    void buildSettings(plugin_utils::SettingsForm& /*form*/) override {}

    bool eventFilter(QObject* watched, QEvent* event) override;

 private:
    void scheduleEnforce();
    void enforce();

    QPointer<QWidget> main_window_;
    QStringList table_names_;
    QList<int> columns_;
    bool applied_ = false;
    bool enforce_pending_ = false;
    QList<QPointer<QTableView>> tables_;
    QList<QMetaObject::Connection> connections_;
};
