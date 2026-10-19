#include "plugin_utils/control_plugin.h"

#include <QTableView>
#include <qdlt/qdltplugin.h>
#include <qdlt/qdltsettingsmanager.h>

namespace plugin_utils {

namespace {

// The plugin API does not expose the plugin's mode or the global plugin
// switch, so read them from the viewer's settings.
bool isPluginEnabled(const QString& plugin_name) {
    const QDltSettingsManager* settings = QDltSettingsManager::getInstance();
    const bool plugins_enabled = settings->value("startup/pluginsEnabled", true).toBool();
    const int mode = settings->value("plugin/pluginmodefor" + plugin_name, QDltPlugin::ModeDisable).toInt();
    return plugins_enabled && mode != QDltPlugin::ModeDisable;
}

}  // namespace

// Called once for every plugin, whatever its mode; the only way to reach the
// main window.
void ControlPlugin::initMainTableView(QTableView* p_table_view) {
    main_window_ = p_table_view->window();
    onMainWindowReady(main_window_);
    syncEnabled();
}

void ControlPlugin::configurationChanged() { syncEnabled(); }

void ControlPlugin::syncEnabled() {
    const bool enabled = isPluginEnabled(name());
    if (enabled == enabled_) {
        return;
    }
    enabled_ = enabled;
    onEnabledChanged(enabled);
}

}  // namespace plugin_utils
