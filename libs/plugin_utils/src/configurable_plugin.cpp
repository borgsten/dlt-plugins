#include "plugin_utils/configurable_plugin.h"

#include <memory>

namespace plugin_utils {

QWidget* ConfigurablePlugin::initViewer() {
    auto form = std::make_unique<SettingsForm>(settings());
    buildSettingsForm(*form);
    // The viewer puts the form in a dock widget, which takes ownership.
    return form.release();
}

}  // namespace plugin_utils
