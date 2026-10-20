#include "uitweaks_plugin.h"

#include <array>
#include <cstddef>

#include <QList>
#include <QStringList>
#include <qdlt/fieldnames.h>

#include "plugin_utils/plugin_info.h"
#include "plugin_utils/qt_compat.h"

#include "tweaks/current_tab_tweak.h"
#include "tweaks/filter_colors_tweak.h"
#include "tweaks/filter_type_box_tweak.h"
#include "tweaks/hidden_widgets_tweak.h"
#include "tweaks/id_column_width_tweak.h"
#include "tweaks/payload_column_tweak.h"
#include "tweaks/recent_menus_tweak.h"
#include "tweaks/search_hits_tweak.h"
#include "tweaks/text_color_tweak.h"

using namespace Qt::StringLiterals;

namespace {

constexpr plugin_utils::PluginInfo kInfo{
    .name = "UI Tweaks"_L1,
    .version = "0.1.0"_L1,
    .description = "Opinionated UI tweaks that doesn't make sense to upstream"_L1,
    .settings_group = "uitweaks"_L1,
};

constexpr std::array kHiddenFilterRangeWidgets{
    "checkBoxFilterRange"_L1,
    "lineEditFilterStart"_L1,
    "lineEditFilterEnd"_L1,
};

constexpr std::array kHiddenFilterAppendWidgets{
    "comboBoxFilterSelection"_L1,
    "pushButtonDefaultFilterUpdateCache"_L1,
    "checkBoxAppendDefaultFilter"_L1,
};

constexpr std::array kLogTables{
    "tableView"_L1,
    "tableView_SearchIndex"_L1,
};

constexpr std::array kFilterColoredViews{
    "tableView"_L1,
    "tableView_SearchIndex"_L1,
    "filterWidget"_L1,
};

template <std::size_t N>
QStringList toStringList(const std::array<QLatin1StringView, N>& names) {
    return QStringList(names.begin(), names.end());
}

}  // namespace

UiTweaksPlugin::UiTweaksPlugin() : ConfigurablePlugin(kInfo), tweaks_(settings()) {}

void UiTweaksPlugin::onMainWindowReady(QWidget* main_window) {
    tweaks_.add<HiddenWidgetsTweak>({.id = "hiddenFilterRangeWidgets", .title = "Hide the filter index range"},
                                    main_window, toStringList(kHiddenFilterRangeWidgets));
    tweaks_.add<HiddenWidgetsTweak>({.id = "hiddenFilterAppendWidgets", .title = "Hide the filter append widgets"},
                                    main_window, toStringList(kHiddenFilterAppendWidgets));
    tweaks_.add<IdColumnWidthTweak>({.id = "idColumnWidth", .title = "Keep app and context ID columns wide enough"},
                                    main_window, toStringList(kLogTables),
                                    QList<int>{FieldNames::AppId, FieldNames::ContextId});
    tweaks_.add<PayloadColumnTweak>({.id = "payloadFit", .title = "Fit the payload column to the table width"},
                                    main_window, toStringList(kLogTables), FieldNames::Payload);
    tweaks_.add<TextColorTweak>({.id = "textColor", .title = "Prefer dark text on filter colors"}, main_window,
                                toStringList(kFilterColoredViews));
    tweaks_.add<SearchHitsTweak>({.id = "searchHits", .title = "Mark search results in the log"}, main_window,
                                 u"tableView"_s, u"tableView_SearchIndex"_s, FieldNames::Index);
    tweaks_.add<FilterColorsTweak>({.id = "filterColors", .title = "Soft filter color palette"}, main_window);
    tweaks_.add<FilterTypeBoxTweak>({.id = "filterTypeBox", .title = "Unclip the filter type buttons (temporary)"});
    tweaks_.add<CurrentTabTweak>({.id = "filterTab", .title = "Start on the Filter tab"}, main_window, u"tabWidget"_s,
                                 u"tabPFilter"_s);
    tweaks_.add<RecentMenusTweak>({.id = "recentMenus", .title = "Extend Recent menus"}, main_window);
}

void UiTweaksPlugin::onEnabledChanged(bool enabled) { tweaks_.setEnabled(enabled); }

void UiTweaksPlugin::buildSettingsForm(plugin_utils::SettingsForm& form) { tweaks_.buildSettingsForm(form); }
