#include "tweaks/recent_menu.h"

#include <cstddef>
#include <utility>

#include <QDebug>
#include <QFileInfo>
#include <QTimer>

#include "plugin_utils/qt_compat.h"

using namespace Qt::StringLiterals;

namespace {

const auto kHistoryKey = u"history"_s;

}  // namespace

RecentMenu::RecentMenu(QWidget* main_window, QString menu_name, plugin_utils::PluginSettings settings,
                       const char* open_slot, int max_entries)
    : main_window_(main_window),
      menu_name_(std::move(menu_name)),
      settings_(std::move(settings)),
      open_slot_(open_slot),
      max_entries_(max_entries) {}

void RecentMenu::apply() {
    if (applied_ || !main_window_) {
        return;
    }

    menu_ = main_window_->findChild<QMenu*>(menu_name_);
    if (!menu_) {
        qWarning() << "UI Tweaks: menu not found:" << menu_name_;
        return;
    }

    // The viewer fills the menu with its fixed set of actions before plugins
    // are loaded and only updates them afterwards.
    viewer_actions_.clear();
    for (QAction* action : menu_->actions()) {
        viewer_actions_.append(action);
    }

    // The viewer's update function touches every action whenever its list
    // changes; react once after it is done.
    for (QAction const* action : menu_->actions()) {
        connections_.append(connect(action, &QAction::changed, this, &RecentMenu::scheduleSync));
    }

    history_ = settings_.value<QStringList>(kHistoryKey, {});
    applied_ = true;
    sync();
}

void RecentMenu::revert() {
    if (!applied_) {
        return;
    }

    for (const QMetaObject::Connection& connection : std::as_const(connections_)) {
        disconnect(connection);
    }
    connections_.clear();

    extra_actions_.clear();
    viewer_actions_.clear();
    applied_ = false;
}

void RecentMenu::setMaxEntries(int max_entries) {
    max_entries_ = max_entries;
    if (applied_) {
        rebuildExtraActions();
    }
}

QStringList RecentMenu::viewerEntries() const {
    QStringList entries;
    for (const QPointer<QAction>& action : viewer_actions_) {
        if (action && action->isVisible() && !action->data().toString().isEmpty()) {
            entries.append(action->data().toString());
        }
    }
    return entries;
}

void RecentMenu::scheduleSync() {
    if (sync_pending_) {
        return;
    }
    sync_pending_ = true;
    QTimer::singleShot(0, this, [this] {
        sync_pending_ = false;
        if (applied_) {
            sync();
        }
    });
}

void RecentMenu::sync() {
    // The viewer's list always holds the newest entries, so it goes first.
    const QStringList viewer = viewerEntries();
    QStringList history = viewer;
    for (const QString& entry : std::as_const(history_)) {
        if (history.size() >= kMaxEntriesLimit) {
            break;
        }
        // The viewer drops entries it fails to open; drop missing files here too.
        if (!history.contains(entry) && QFileInfo::exists(entry)) {
            history.append(entry);
        }
    }

    if (history != history_) {
        history_ = history;
        saveHistory();
    }
    rebuildExtraActions();
}

void RecentMenu::rebuildExtraActions() {
    extra_actions_.clear();

    if (!menu_) {
        return;
    }

    const QStringList viewer = viewerEntries();
    size_t number = viewer.size();
    for (const QString& entry : std::as_const(history_)) {
        if (std::cmp_greater_equal(number, max_entries_)) {
            break;
        }
        if (viewer.contains(entry)) {
            continue;
        }
        ++number;
        auto action = std::make_unique<QAction>(QString("&%1 %2").arg(number).arg(entry));
        action->setData(entry);
        connect(action.get(), SIGNAL(triggered()), main_window_, open_slot_);
        menu_->addAction(action.get());
        extra_actions_.push_back(std::move(action));
    }

    if (!extra_actions_.empty()) {
        menu_->setEnabled(true);
    }
}

void RecentMenu::saveHistory() const { settings_.setValue(kHistoryKey, history_); }
