#include "plugin_utils/configurable_plugin.h"

#include <memory>

#include <QFrame>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QTabWidget>

namespace plugin_utils {

namespace {

// Scrolls vertically only: at least as wide as its widget needs, with room
// for the vertical scroll bar, but as short as the dock wants.
class VerticalScrollArea : public QScrollArea {
 public:
    VerticalScrollArea() {
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    [[nodiscard]] QSize minimumSizeHint() const override {
        QSize hint = QScrollArea::minimumSizeHint();
        if (widget() != nullptr) {
            hint.setWidth(widget()->minimumSizeHint().width() + verticalScrollBar()->sizeHint().width() +
                          (2 * frameWidth()));
        }
        return hint;
    }
};

}  // namespace

QWidget* ConfigurablePlugin::initViewer() {
    auto form = std::make_unique<SettingsForm>(settings());
    buildSettingsForm(*form);
    // Without scrolling, the dock and the docks beside it can't get shorter than the form.
    auto scroll_area = std::make_unique<VerticalScrollArea>();
    scroll_area->setWidget(form.release());
    // The viewer puts the widget in a dock widget, which takes ownership.
    std::unique_ptr<QWidget> view = createView();
    if (!view) {
        return scroll_area.release();
    }
    auto tabs = std::make_unique<QTabWidget>();
    const QString title = view->windowTitle();
    tabs->addTab(view.release(), title);
    tabs->addTab(scroll_area.release(), QTabWidget::tr("Settings"));
    return tabs.release();
}

}  // namespace plugin_utils
