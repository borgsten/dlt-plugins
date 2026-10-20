#include "tweaks/current_tab_tweak.h"

#include <utility>

#include <QDebug>

CurrentTabTweak::CurrentTabTweak(QWidget* main_window, QString tab_widget_name, QString tab_name)
    : main_window_(main_window), tab_widget_name_(std::move(tab_widget_name)), tab_name_(std::move(tab_name)) {}

void CurrentTabTweak::apply() {
    if (!main_window_) {
        return;
    }
    auto* tab_widget = main_window_->findChild<QTabWidget*>(tab_widget_name_);
    auto* tab = (tab_widget != nullptr) ? tab_widget->findChild<QWidget*>(tab_name_) : nullptr;
    if (tab == nullptr || tab_widget->indexOf(tab) < 0) {
        qWarning() << "UI Tweaks: tab not found:" << tab_widget_name_ << tab_name_;
        return;
    }
    tab_widget->setCurrentWidget(tab);
}
