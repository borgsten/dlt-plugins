#include "tweaks/filter_type_box_tweak.h"

#include <QApplication>
#include <QDebug>
#include <QGroupBox>
#include <QString>
#include <QWidget>

#include "plugin_utils/qt_compat.h"

using namespace Qt::StringLiterals;

namespace {

constexpr auto kFilterDialogClass = "FilterDialog";

}  // namespace

void FilterTypeBoxTweak::apply() {
    if (applied_) {
        return;
    }
    applied_ = true;
    // The viewer creates a new filter dialog for every edit; catch it when shown.
    QApplication::instance()->installEventFilter(this);
}

void FilterTypeBoxTweak::revert() {
    if (!applied_) {
        return;
    }
    applied_ = false;
    QApplication::instance()->removeEventFilter(this);
}

bool FilterTypeBoxTweak::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Show && watched->inherits(kFilterDialogClass)) {
        auto* type_box = watched->findChild<QGroupBox*>(u"groupBox"_s);
        if (type_box == nullptr) {
            qWarning() << "UI Tweaks: filter dialog type box not found";
        } else {
            type_box->setMaximumHeight(QWIDGETSIZE_MAX);
        }
    }
    return false;
}
