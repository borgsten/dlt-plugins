#pragma once

#include <memory>
#include <vector>

#include <QString>
#include <QWidget>

#include "plugin_utils/plugin_settings.h"
#include "plugin_utils/settings_form.h"

#include "tweaks/itweak.h"
#include "tweaks/recent_menu.h"

/* Extends the viewer's Recent files, projects and filters menus. */
class RecentMenusTweak : public ITweak {
 public:
    RecentMenusTweak(plugin_utils::PluginSettings settings, QWidget* main_window);

    void apply() override;
    void revert() override;
    void buildSettings(plugin_utils::SettingsForm& form) override;

 private:
    struct MenuSpec {
        QString id;
        QString label;
        QString menu_name;
        /* As SLOT(openRecentFile()) */
        const char* open_slot;
    };

    struct Menu {
        QString id;
        QString label;
        std::unique_ptr<RecentMenu> menu;
    };

    void addMenu(QWidget* main_window, const MenuSpec& spec);
    [[nodiscard]] bool isMenuEnabled(const Menu& menu) const;
    void setMenuEnabled(const Menu& menu, bool enabled) const;
    void setMaxEntries(int max_entries);

    plugin_utils::PluginSettings settings_;
    std::vector<Menu> menus_;
    bool applied_ = false;
};
