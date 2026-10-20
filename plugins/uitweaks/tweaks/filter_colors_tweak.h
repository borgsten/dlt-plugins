#pragma once

#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QWidget>

#include "plugin_utils/plugin_settings.h"

#include "tweaks/itweak.h"

/*
 * Replaces the filter dialog's harsh marker color presets with soft colors:
 * a row of hue families and the shades of the chosen one, computed with a
 * configurable count and lightness step, with the colors
 * other filters already use marked. Also keeps the color preview's color
 * while the dialog is inactive, and dims it, like the colors, while disabled.
 */
class FilterColorsTweak : public QObject, public ITweak {
    Q_OBJECT

 public:
    FilterColorsTweak(plugin_utils::PluginSettings settings, QWidget* main_window);

    void apply() override;
    void revert() override;
    void buildSettings(plugin_utils::SettingsForm& form) override;

    bool eventFilter(QObject* watched, QEvent* event) override;

 private:
    void extendDialog(QWidget* dialog) const;

    plugin_utils::PluginSettings settings_;
    QPointer<QWidget> main_window_;
    bool applied_ = false;
};
