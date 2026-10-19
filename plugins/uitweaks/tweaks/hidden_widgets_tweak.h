#pragma once

#include <QList>
#include <QPointer>
#include <QStringList>
#include <QWidget>

#include "tweaks/itweak.h"

/* Hides main window widgets by object name. */
class HiddenWidgetsTweak : public ITweak {
 public:
    HiddenWidgetsTweak(QWidget* main_window, QStringList object_names);

    void apply() override;
    void revert() override;
    void buildSettings(plugin_utils::SettingsForm& /*form*/) override {}

 private:
    QPointer<QWidget> main_window_;
    QStringList object_names_;
    QList<QPointer<QWidget>> hidden_by_us_;
};
