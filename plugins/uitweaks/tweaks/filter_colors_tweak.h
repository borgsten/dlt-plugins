#pragma once

#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QWidget>

#include "tweaks/itweak.h"

/*
 * Replaces the filter dialog's harsh marker color presets with soft colors:
 * a row of hue families and the shades of the chosen one, with the colors
 * other filters already use marked. Also keeps the color preview's color
 * while the dialog is inactive.
 */
class FilterColorsTweak : public QObject, public ITweak {
    Q_OBJECT

 public:
    explicit FilterColorsTweak(QWidget* main_window);

    void apply() override;
    void revert() override;
    void buildSettings(plugin_utils::SettingsForm& /*form*/) override {}

    bool eventFilter(QObject* watched, QEvent* event) override;

 private:
    void extendDialog(QWidget* dialog) const;

    QPointer<QWidget> main_window_;
    bool applied_ = false;
};
