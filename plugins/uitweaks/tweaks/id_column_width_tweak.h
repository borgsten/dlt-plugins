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
    IdColumnWidthTweak(QWidget* mainWindow, QStringList tableNames, QList<int> columns);

    void apply() override;
    void revert() override;
    void buildSettings(plugin_utils::SettingsForm& /*form*/) override {}

    bool eventFilter(QObject* watched, QEvent* event) override;

 private:
    void scheduleEnforce();
    void enforce();

    QPointer<QWidget> m_mainWindow;
    QStringList m_tableNames;
    QList<int> m_columns;
    bool m_applied = false;
    bool m_enforcePending = false;
    QList<QPointer<QTableView>> m_tables;
    QList<QMetaObject::Connection> m_connections;
};
