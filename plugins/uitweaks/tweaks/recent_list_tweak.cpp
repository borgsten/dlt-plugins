#include "tweaks/recent_list_tweak.h"

#include <cstddef>
#include <utility>

#include <QDebug>
#include <QFileInfo>
#include <QTimer>
#include <QtLogging>
#include <qdlt/qdltsettingsmanager.h>

RecentListTweak::RecentListTweak(QWidget* mainWindow, QString menuName, QString historyKey, const char* openSlot,
                                 int maxEntries)
    : m_mainWindow(mainWindow),
      m_menuName(std::move(menuName)),
      m_historyKey(std::move(historyKey)),
      m_openSlot(openSlot),
      m_maxEntries(maxEntries) {}

void RecentListTweak::apply() {
    if (m_applied || !m_mainWindow) {
        return;
    }

    m_menu = m_mainWindow->findChild<QMenu*>(m_menuName);
    if (!m_menu) {
        qWarning() << "UI Tweaks: menu not found:" << m_menuName;
        return;
    }

    // The viewer fills the menu with its fixed set of actions before plugins
    // are loaded and only updates them afterwards.
    m_viewerActions.clear();
    for (QAction* action : m_menu->actions()) {
        m_viewerActions.append(action);
    }

    // The viewer's update function touches every action whenever its list
    // changes; react once after it is done.
    for (QAction const* action : m_menu->actions()) {
        m_connections.append(connect(action, &QAction::changed, this, &RecentListTweak::scheduleSync));
    }

    m_history = QDltSettingsManager::getInstance()->value(m_historyKey).toStringList();
    m_applied = true;
    sync();
}

void RecentListTweak::revert() {
    if (!m_applied) {
        return;
    }

    for (const QMetaObject::Connection& connection : std::as_const(m_connections)) {
        disconnect(connection);
    }
    m_connections.clear();

    m_extraActions.clear();
    m_viewerActions.clear();
    m_applied = false;
}

QStringList RecentListTweak::viewerEntries() const {
    QStringList entries;
    for (const QPointer<QAction>& action : m_viewerActions) {
        if (action && action->isVisible() && !action->data().toString().isEmpty()) {
            entries.append(action->data().toString());
        }
    }
    return entries;
}

void RecentListTweak::scheduleSync() {
    if (m_syncPending) {
        return;
    }
    m_syncPending = true;
    QTimer::singleShot(0, this, [this] {
        m_syncPending = false;
        if (m_applied) {
            sync();
        }
    });
}

void RecentListTweak::sync() {
    // The viewer's list always holds the newest entries, so it goes first.
    const QStringList viewer = viewerEntries();
    QStringList history = viewer;
    for (const QString& entry : std::as_const(m_history)) {
        if (history.size() >= m_maxEntries) {
            break;
        }
        // The viewer drops entries it fails to open; drop missing files here too.
        if (!history.contains(entry) && QFileInfo::exists(entry)) {
            history.append(entry);
        }
    }

    if (history != m_history) {
        m_history = history;
        saveHistory();
    }
    rebuildExtraActions();
}

void RecentListTweak::rebuildExtraActions() {
    m_extraActions.clear();

    if (!m_menu) {
        return;
    }

    const QStringList viewer = viewerEntries();
    size_t number = viewer.size();
    for (const QString& entry : std::as_const(m_history)) {
        if (viewer.contains(entry)) {
            continue;
        }
        ++number;
        auto action = std::make_unique<QAction>(QString("&%1 %2").arg(number).arg(entry));
        action->setData(entry);
        connect(action.get(), SIGNAL(triggered()), m_mainWindow, m_openSlot);
        m_menu->addAction(action.get());
        m_extraActions.push_back(std::move(action));
    }

    if (!m_extraActions.empty()) {
        m_menu->setEnabled(true);
    }
}

void RecentListTweak::saveHistory() const { QDltSettingsManager::getInstance()->setValue(m_historyKey, m_history); }
