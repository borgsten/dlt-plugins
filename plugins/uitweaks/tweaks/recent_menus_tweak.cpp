#include "tweaks/recent_menus_tweak.h"

#include <array>
#include <utility>

using namespace Qt::StringLiterals;

namespace {

const auto kEnabledKey = u"enabled"_s;
const auto kEntriesKey = u"entries"_s;
constexpr int kDefaultEntries = 10;
// The viewer itself shows this many.
constexpr int kMinEntries = 5;

}  // namespace

RecentMenusTweak::RecentMenusTweak(plugin_utils::PluginSettings settings, QWidget* mainWindow)
    : m_settings(std::move(settings)) {
    const std::array menus{
        MenuSpec{
            .id = "files",
            .label = "Files",
            .menuName = "menuRecent_files",
            .openSlot = SLOT(openRecentFile()),
        },
        MenuSpec{
            .id = "projects",
            .label = "Projects",
            .menuName = "menuRecent_projects",
            .openSlot = SLOT(openRecentProject()),
        },
        MenuSpec{
            .id = "filters",
            .label = "Filters",
            .menuName = "menuRecent_Filters",
            .openSlot = SLOT(openRecentFilters()),
        },
    };
    for (const MenuSpec& spec : menus) {
        addMenu(mainWindow, spec);
    }
}

void RecentMenusTweak::apply() {
    m_applied = true;
    for (const Menu& menu : m_menus) {
        if (isMenuEnabled(menu)) {
            menu.menu->apply();
        }
    }
}

void RecentMenusTweak::revert() {
    for (const Menu& menu : m_menus) {
        menu.menu->revert();
    }
    m_applied = false;
}

void RecentMenusTweak::buildSettings(plugin_utils::SettingsForm& form) {
    // m_menus does not change after construction, so the entries stay put.
    for (const Menu& menu : m_menus) {
        form.addCheckBox({.key = menu.id + u'/' + kEnabledKey, .label = menu.label, .defaultValue = true},
                         [this, menuPtr = &menu](bool enabled) { setMenuEnabled(*menuPtr, enabled); });
    }
    form.addSpinBox(
        {
            .key = kEntriesKey,
            .label = "Entries",
            .defaultValue = kDefaultEntries,
            .minimum = kMinEntries,
            .maximum = RecentMenu::kMaxEntriesLimit,
        },
        [this](int maxEntries) { setMaxEntries(maxEntries); });
}

void RecentMenusTweak::addMenu(QWidget* mainWindow, const MenuSpec& spec) {
    auto menu = std::make_unique<RecentMenu>(mainWindow, spec.menuName, m_settings.group(spec.id), spec.openSlot,
                                             m_settings.value(kEntriesKey, kDefaultEntries));
    m_menus.push_back({.id = spec.id, .label = spec.label, .menu = std::move(menu)});
}

bool RecentMenusTweak::isMenuEnabled(const Menu& menu) const {
    return m_settings.group(menu.id).value(kEnabledKey, true);
}

void RecentMenusTweak::setMenuEnabled(const Menu& menu, bool enabled) const {
    if (!m_applied) {
        return;
    }
    if (enabled) {
        menu.menu->apply();
    } else {
        menu.menu->revert();
    }
}

void RecentMenusTweak::setMaxEntries(int maxEntries) {
    for (const Menu& menu : m_menus) {
        menu.menu->setMaxEntries(maxEntries);
    }
}
