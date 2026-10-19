#include "tweaks/current_tab_tweak.h"

#include <utility>

#include <QDebug>
#include <QtLogging>

CurrentTabTweak::CurrentTabTweak(QWidget* main_window, QString tab_widget_name, QString tab_name)
    : main_window_(main_window), tab_widget_name_(std::move(tab_widget_name)), tab_name_(std::move(tab_name)) {}

void CurrentTabTweak::apply() {
    if (!main_window_) {
        return;
    }
    tab_widget_ = main_window_->findChild<QTabWidget*>(tab_widget_name_);
    auto* tab = tab_widget_ ? tab_widget_->findChild<QWidget*>(tab_name_) : nullptr;
    if (tab == nullptr || tab_widget_->indexOf(tab) < 0) {
        qWarning() << "UI Tweaks: tab not found:" << tab_widget_name_ << tab_name_;
        return;
    }
    previous_tab_ = tab_widget_->currentWidget();
    tab_widget_->setCurrentWidget(tab);
}

void CurrentTabTweak::revert() {
    // Leave a tab the user has switched to since.
    const QWidget* current = tab_widget_ ? tab_widget_->currentWidget() : nullptr;
    if (current != nullptr && previous_tab_ && current->objectName() == tab_name_) {
        tab_widget_->setCurrentWidget(previous_tab_);
    }
    previous_tab_.clear();
}
