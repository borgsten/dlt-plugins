#pragma once

#include <QLatin1StringView>

namespace plugin_utils {

/*
 * The views must refer to string literals. Changing name resets the plugin's
 * mode in the viewer; changing settingsGroup loses its settings.
 */
struct PluginInfo {
    QLatin1StringView name;
    QLatin1StringView version;
    QLatin1StringView description;
    QLatin1StringView settingsGroup;
};

}  // namespace plugin_utils
