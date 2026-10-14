#pragma once

#include <memory>
#include <vector>

#include <QAction>
#include <QList>
#include <QMenu>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include "plugin_utils/plugin_settings.h"

/*
 * Extends one of the viewer's fixed-size "Recent ..." menus with a longer
 * history of its own. Entries open through the viewer's slot for the menu.
 */
class RecentMenu : public QObject {
    Q_OBJECT

 public:
    static constexpr int kMaxEntriesLimit = 50;

    /* openSlot as SLOT(openRecentFile()) */
    RecentMenu(QWidget* mainWindow, QString menuName, plugin_utils::PluginSettings settings, const char* openSlot,
               int maxEntries);

    void apply();
    void revert();

    /* Including the viewer's own entries */
    void setMaxEntries(int maxEntries);

 private:
    [[nodiscard]] QStringList viewerEntries() const;
    void scheduleSync();
    void sync();
    void rebuildExtraActions();
    void saveHistory() const;

    QPointer<QWidget> m_mainWindow;
    QString m_menuName;
    plugin_utils::PluginSettings m_settings;
    const char* m_openSlot;
    int m_maxEntries;
    QPointer<QMenu> m_menu;
    bool m_applied = false;
    bool m_syncPending = false;
    QList<QPointer<QAction>> m_viewerActions;
    std::vector<std::unique_ptr<QAction>> m_extraActions;
    QList<QMetaObject::Connection> m_connections;
    QStringList m_history;
};
