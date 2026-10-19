#include "plugin_utils/configurable_plugin.h"

#include <memory>

#include <QFrame>
#include <QScrollArea>

namespace plugin_utils {

QWidget* ConfigurablePlugin::initViewer() {
    auto form = std::make_unique<SettingsForm>(settings());
    buildSettingsForm(*form);
    // Without scrolling, the dock and the docks beside it can't get shorter than the form.
    auto scroll_area = std::make_unique<QScrollArea>();
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);
    scroll_area->setWidget(form.release());
    // The viewer puts the widget in a dock widget, which takes ownership.
    return scroll_area.release();
}

}  // namespace plugin_utils
