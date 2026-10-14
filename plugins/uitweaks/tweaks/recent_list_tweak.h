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

#include "tweaks/itweak.h"

/*
 * Extends one of the viewer's "Recent ..." menus beyond its fixed limit.
 *
 * The viewer keeps only its newest entries (MaxRecentFiles and friends), so
 * this keeps a longer history of its own and appends the older entries to the
 * menu. They open through the viewer's own slot for that menu.
 */
class RecentListTweak : public QObject, public ITweak {
    Q_OBJECT

 public:
    /*
     * menuName:   object name of the viewer's menu, e.g. "menuRecent_files"
     * historyKey: settings key this tweak stores its history under
     * openSlot:   the viewer's open slot as SLOT(openRecentFile())
     */
    RecentListTweak(QWidget* mainWindow, QString menuName, QString historyKey, const char* openSlot, int maxEntries);

    void apply() override;
    void revert() override;

 private:
    [[nodiscard]] QStringList viewerEntries() const;
    void scheduleSync();
    void sync();
    void rebuildExtraActions();
    void saveHistory() const;

    QPointer<QWidget> m_mainWindow;
    QString m_menuName;
    QString m_historyKey;
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
