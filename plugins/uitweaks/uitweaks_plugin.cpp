#include "uitweaks_plugin.h"

#include <array>
#include <cstddef>

#include <QLatin1StringView>
#include <QList>
#include <QStringList>
#include <qdlt/fieldnames.h>

#include "plugin_utils/plugin_info.h"

#include "tweaks/hidden_widgets_tweak.h"
#include "tweaks/id_column_width_tweak.h"
#include "tweaks/recent_menus_tweak.h"

using namespace Qt::StringLiterals;

namespace {

constexpr plugin_utils::PluginInfo kInfo{
    .name = "UI Tweaks"_L1,
    .version = "0.1.0"_L1,
    .description = "Opinionated UI tweaks that doesn't make sense to upstream"_L1,
    .settingsGroup = "uitweaks"_L1,
};

constexpr std::array kHiddenWidgets{
    "checkBoxFilterRange"_L1,
    "lineEditFilterStart"_L1,
    "lineEditFilterEnd"_L1,
};

constexpr std::array kLogTables{
    "tableView"_L1,
    "tableView_SearchIndex"_L1,
};

template <std::size_t N>
QStringList toStringList(const std::array<QLatin1StringView, N>& names) {
    return QStringList(names.begin(), names.end());
}

}  // namespace

UiTweaksPlugin::UiTweaksPlugin() : ConfigurablePlugin(kInfo), m_tweaks(settings()) {}

void UiTweaksPlugin::onMainWindowReady(QWidget* mainWindow) {
    m_tweaks.add<HiddenWidgetsTweak>({.id = "hiddenWidgets", .title = "Hide the filter index range"}, mainWindow,
                                     toStringList(kHiddenWidgets));
    m_tweaks.add<IdColumnWidthTweak>({.id = "idColumnWidth", .title = "Keep app and context ID columns wide enough"},
                                     mainWindow, toStringList(kLogTables),
                                     QList<int>{FieldNames::AppId, FieldNames::ContextId});
    m_tweaks.add<RecentMenusTweak>({.id = "recentMenus", .title = "Extend Recent menus"}, mainWindow);
}

void UiTweaksPlugin::onEnabledChanged(bool enabled) { m_tweaks.setEnabled(enabled); }

void UiTweaksPlugin::buildSettingsForm(plugin_utils::SettingsForm& form) { m_tweaks.buildSettingsForm(form); }
