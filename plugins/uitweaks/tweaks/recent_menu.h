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

    /* open_slot as SLOT(openRecentFile()) */
    RecentMenu(QWidget* main_window, QString menu_name, plugin_utils::PluginSettings settings, const char* open_slot,
               int max_entries);

    void apply();
    void revert();

    /* Including the viewer's own entries */
    void setMaxEntries(int max_entries);

 private:
    [[nodiscard]] QStringList viewerEntries() const;
    void scheduleSync();
    void sync();
    void rebuildExtraActions();
    void saveHistory() const;

    QPointer<QWidget> main_window_;
    QString menu_name_;
    plugin_utils::PluginSettings settings_;
    const char* open_slot_;
    int max_entries_;
    QPointer<QMenu> menu_;
    bool applied_ = false;
    bool sync_pending_ = false;
    QList<QPointer<QAction>> viewer_actions_;
    std::vector<std::unique_ptr<QAction>> extra_actions_;
    QList<QMetaObject::Connection> connections_;
    QStringList history_;
};
