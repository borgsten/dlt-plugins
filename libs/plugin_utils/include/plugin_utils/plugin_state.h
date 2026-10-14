#pragma once

#include <QString>

namespace plugin_utils {

/*
 * True when the plugin is enabled in the viewer: its mode is not Disabled and
 * plugins are globally enabled.
 *
 * The plugin API does not expose either, so this reads the settings the viewer
 * stores them in. The viewer saves both before it calls configurationChanged().
 */
bool isPluginEnabled(const QString& pluginName);

}  // namespace plugin_utils
