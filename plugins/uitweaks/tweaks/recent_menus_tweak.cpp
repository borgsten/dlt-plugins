#include "tweaks/recent_menus_tweak.h"

#include <array>
#include <utility>

#include "plugin_utils/qt_compat.h"

using namespace Qt::StringLiterals;

namespace {

const auto kEnabledKey = u"enabled"_s;
const auto kEntriesKey = u"entries"_s;
constexpr int kDefaultEntries = 10;
// The viewer itself shows this many.
constexpr int kMinEntries = 5;

}  // namespace

RecentMenusTweak::RecentMenusTweak(plugin_utils::PluginSettings settings, QWidget* main_window)
    : settings_(std::move(settings)) {
    const std::array menus{
        MenuSpec{
            .id = "files",
            .label = "Files",
            .menu_name = "menuRecent_files",
            .open_slot = SLOT(openRecentFile()),
        },
        MenuSpec{
            .id = "projects",
            .label = "Projects",
            .menu_name = "menuRecent_projects",
            .open_slot = SLOT(openRecentProject()),
        },
        MenuSpec{
            .id = "filters",
            .label = "Filters",
            .menu_name = "menuRecent_Filters",
            .open_slot = SLOT(openRecentFilters()),
        },
    };
    for (const MenuSpec& spec : menus) {
        addMenu(main_window, spec);
    }
}

void RecentMenusTweak::apply() {
    applied_ = true;
    for (const Menu& menu : menus_) {
        if (isMenuEnabled(menu)) {
            menu.menu->apply();
        }
    }
}

void RecentMenusTweak::revert() {
    for (const Menu& menu : menus_) {
        menu.menu->revert();
    }
    applied_ = false;
}

void RecentMenusTweak::buildSettings(plugin_utils::SettingsForm& form) {
    // menus_ does not change after construction, so the entries stay put.
    for (const Menu& menu : menus_) {
        form.addCheckBox({.key = menu.id + u'/' + kEnabledKey, .label = menu.label, .default_value = true},
                         [this, menu_ptr = &menu](bool enabled) { setMenuEnabled(*menu_ptr, enabled); });
    }
    form.addSpinBox(
        {
            .key = kEntriesKey,
            .label = "Entries",
            .default_value = kDefaultEntries,
            .minimum = kMinEntries,
            .maximum = RecentMenu::kMaxEntriesLimit,
        },
        [this](int max_entries) { setMaxEntries(max_entries); });
}

void RecentMenusTweak::addMenu(QWidget* main_window, const MenuSpec& spec) {
    auto menu = std::make_unique<RecentMenu>(main_window, spec.menu_name, settings_.group(spec.id), spec.open_slot,
                                             settings_.value(kEntriesKey, kDefaultEntries));
    menus_.push_back({.id = spec.id, .label = spec.label, .menu = std::move(menu)});
}

bool RecentMenusTweak::isMenuEnabled(const Menu& menu) const {
    return settings_.group(menu.id).value(kEnabledKey, true);
}

void RecentMenusTweak::setMenuEnabled(const Menu& menu, bool enabled) const {
    if (!applied_) {
        return;
    }
    if (enabled) {
        menu.menu->apply();
    } else {
        menu.menu->revert();
    }
}

void RecentMenusTweak::setMaxEntries(int max_entries) {
    for (const Menu& menu : menus_) {
        menu.menu->setMaxEntries(max_entries);
    }
}
