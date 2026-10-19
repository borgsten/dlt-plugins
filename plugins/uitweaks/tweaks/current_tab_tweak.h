#pragma once

#include <QPointer>
#include <QString>
#include <QTabWidget>
#include <QWidget>

#include "tweaks/itweak.h"

/* Selects a tab of a main window tab widget, both by object name. */
class CurrentTabTweak : public ITweak {
 public:
    CurrentTabTweak(QWidget* main_window, QString tab_widget_name, QString tab_name);

    void apply() override;
    void revert() override {};
    void buildSettings(plugin_utils::SettingsForm& /*form*/) override {}

 private:
    QPointer<QWidget> main_window_;
    QString tab_widget_name_;
    QString tab_name_;
};
