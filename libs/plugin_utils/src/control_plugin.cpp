#include "plugin_utils/control_plugin.h"

#include <QTableView>

#include "plugin_utils/plugin_state.h"

namespace plugin_utils {

// The viewer calls this once for every plugin, whatever its mode, right after
// building the main window. It is the only way into the main window.
void ControlPlugin::initMainTableView(QTableView* pTableView) {
    m_mainWindow = pTableView->window();
    onMainWindowReady(m_mainWindow);
    syncEnabled();
}

// Called on Apply Configuration and when plugins are globally toggled, which
// is when the viewer applies plugin mode changes.
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
