#include "plugin_utils/plugin_state.h"

#include <qdlt/qdltplugin.h>
#include <qdlt/qdltsettingsmanager.h>

namespace plugin_utils {

bool isPluginEnabled(const QString& pluginName) {
    QDltSettingsManager const* settings = QDltSettingsManager::getInstance();
    const bool pluginsEnabled = settings->value("startup/pluginsEnabled", true).toBool();
    const int mode = settings->value("plugin/pluginmodefor" + pluginName, QDltPlugin::ModeDisable).toInt();
    return pluginsEnabled && mode != QDltPlugin::ModeDisable;
}

}  // namespace plugin_utils
