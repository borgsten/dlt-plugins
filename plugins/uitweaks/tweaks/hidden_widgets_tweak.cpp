#include "tweaks/hidden_widgets_tweak.h"

#include <utility>

#include <QDebug>

HiddenWidgetsTweak::HiddenWidgetsTweak(QWidget* main_window, QStringList object_names)
    : main_window_(main_window), object_names_(std::move(object_names)) {}

void HiddenWidgetsTweak::apply() {
    if (!main_window_) {
        return;
    }

    for (const QString& name : std::as_const(object_names_)) {
        auto* widget = main_window_->findChild<QWidget*>(name);
        if (widget == nullptr) {
            qWarning() << "UI Tweaks: widget not found:" << name;
            continue;
        }
        if (widget->isHidden()) {
            continue;
        }
        widget->hide();
        hidden_by_us_.append(widget);
    }
}

void HiddenWidgetsTweak::revert() {
    for (const QPointer<QWidget>& widget : std::as_const(hidden_by_us_)) {
        if (widget) {
            widget->show();
        }
    }
    hidden_by_us_.clear();
}
