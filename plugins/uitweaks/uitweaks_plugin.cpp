#include "uitweaks_plugin.h"

#include <array>
#include <cstddef>
#include <ranges>

#include <QDebug>
#include <QLatin1StringView>
#include <QList>
#include <QMetaObject>
#include <QStringList>
#include <QtLogging>
#include <qdlt/fieldnames.h>

#include "plugin_utils/plugin_info.h"

#include "tweaks/hidden_widgets_tweak.h"
#include "tweaks/id_column_width_tweak.h"
#include "tweaks/recent_list_tweak.h"

using namespace Qt::StringLiterals;

namespace {

constexpr plugin_utils::PluginInfo kInfo{
    .name = "UI Tweaks"_L1,
    .version = "0.1.0"_L1,
    .description = "Opinionated UI tweaks that doesn't make sense to upstream"_L1,
};

constexpr std::array kHiddenWidgets{
    "checkBoxFilterRange"_L1,
    "lineEditFilterStart"_L1,
    "lineEditFilterEnd"_L1,
};

/* Main table and search results table */
constexpr std::array kLogTables{
    "tableView"_L1,
    "tableView_SearchIndex"_L1,
};

constexpr int kMaxRecentEntries = 10;

template <std::size_t N>
QStringList toStringList(const std::array<QLatin1StringView, N>& names) {
    return QStringList(names.begin(), names.end());
}

}  // namespace

UiTweaksPlugin::UiTweaksPlugin() : ControlPlugin(kInfo) {}

void UiTweaksPlugin::onMainWindowReady(QWidget* mainWindow) {
    addTweak<HiddenWidgetsTweak>("hiddenWidgets", mainWindow, toStringList(kHiddenWidgets));
    addTweak<IdColumnWidthTweak>("idColumnWidth", mainWindow, toStringList(kLogTables),
                                 QList<int>{FieldNames::AppId, FieldNames::ContextId});
    addTweak<RecentListTweak>("recentFiles", mainWindow, "menuRecent_files", "uitweaks/recentFileList",
                              SLOT(openRecentFile()), kMaxRecentEntries);
    addTweak<RecentListTweak>("recentProjects", mainWindow, "menuRecent_projects", "uitweaks/recentProjectList",
                              SLOT(openRecentProject()), kMaxRecentEntries);
    addTweak<RecentListTweak>("recentFilters", mainWindow, "menuRecent_Filters", "uitweaks/recentFiltersList",
                              SLOT(openRecentFilters()), kMaxRecentEntries);
}

void UiTweaksPlugin::onEnabledChanged(bool enabled) {
    if (enabled) {
        for (const Entry& entry : m_tweaks) {
            qDebug() << name() << "applying" << entry.id;
            entry.tweak->apply();
        }
    } else {
        // Later tweaks may build on earlier ones, so undo them first.
        for (const Entry& entry : std::views::reverse(m_tweaks)) {
            qDebug() << name() << "reverting" << entry.id;
            entry.tweak->revert();
        }
    }
}
