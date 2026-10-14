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
    RecentMenusTweak(plugin_utils::PluginSettings settings, QWidget* mainWindow);

    void apply() override;
    void revert() override;
    void buildSettings(plugin_utils::SettingsForm& form) override;

 private:
    struct MenuSpec {
        QString id;
        QString label;
        QString menuName;
        /* As SLOT(openRecentFile()) */
        const char* openSlot;
    };

    struct Menu {
        QString id;
        QString label;
        std::unique_ptr<RecentMenu> menu;
    };

    void addMenu(QWidget* mainWindow, const MenuSpec& spec);
    [[nodiscard]] bool isMenuEnabled(const Menu& menu) const;
    void setMenuEnabled(const Menu& menu, bool enabled) const;
    void setMaxEntries(int maxEntries);

    plugin_utils::PluginSettings m_settings;
    std::vector<Menu> m_menus;
    bool m_applied = false;
};
