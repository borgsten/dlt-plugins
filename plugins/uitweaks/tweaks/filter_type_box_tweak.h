#pragma once

#include <QEvent>
#include <QObject>

#include "tweaks/itweak.h"

/*
 * Lets the filter dialog's Type box grow to fit its buttons; its fixed
 * maximum height clips them. Temporary, until the viewer's filterdialog.ui
 * drops that maximum.
 */
class FilterTypeBoxTweak : public QObject, public ITweak {
    Q_OBJECT

 public:
    void apply() override;
    void revert() override;
    void buildSettings(plugin_utils::SettingsForm& /*form*/) override {}

    bool eventFilter(QObject* watched, QEvent* event) override;

 private:
    bool applied_ = false;
};
