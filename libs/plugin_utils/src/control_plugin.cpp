#include "plugin_utils/control_plugin.h"

#include <QTableView>
#include <qdlt/qdltplugin.h>
#include <qdlt/qdltsettingsmanager.h>

namespace plugin_utils {

namespace {

// The plugin API does not expose the plugin's mode or the global plugin
// switch, so read them from the viewer's settings.
bool isPluginEnabled(const QString& pluginName) {
    const QDltSettingsManager* settings = QDltSettingsManager::getInstance();
    const bool pluginsEnabled = settings->value("startup/pluginsEnabled", true).toBool();
    const int mode = settings->value("plugin/pluginmodefor" + pluginName, QDltPlugin::ModeDisable).toInt();
    return pluginsEnabled && mode != QDltPlugin::ModeDisable;
}

}  // namespace

// Called once for every plugin, whatever its mode; the only way to reach the
// main window.
void ControlPlugin::initMainTableView(QTableView* pTableView) {
    m_mainWindow = pTableView->window();
    onMainWindowReady(m_mainWindow);
    syncEnabled();
}

void ControlPlugin::configurationChanged() { syncEnabled(); }

void ControlPlugin::syncEnabled() {
    const bool enabled = isPluginEnabled(name());
    if (enabled == m_enabled) {
        return;
    }
    m_enabled = enabled;
    onEnabledChanged(enabled);
}

}  // namespace plugin_utils
