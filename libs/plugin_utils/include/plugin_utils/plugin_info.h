#pragma once

#include <QLatin1StringView>

namespace plugin_utils {

/*
 * What the viewer shows for a plugin. The views must refer to string
 * literals. The name is also the key the viewer stores the plugin's mode
 * under, so changing it resets the mode.
 */
struct PluginInfo {
    QLatin1StringView name;
    QLatin1StringView version;
    QLatin1StringView description;
};

}  // namespace plugin_utils
