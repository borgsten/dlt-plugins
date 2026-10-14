#include "tweaks/hidden_widgets_tweak.h"

#include <utility>

#include <QDebug>
#include <QtLogging>

HiddenWidgetsTweak::HiddenWidgetsTweak(QWidget* mainWindow, QStringList objectNames)
    : m_mainWindow(mainWindow), m_objectNames(std::move(objectNames)) {}

void HiddenWidgetsTweak::apply() {
    if (!m_mainWindow) {
        return;
    }

    for (const QString& name : std::as_const(m_objectNames)) {
        auto* widget = m_mainWindow->findChild<QWidget*>(name);
        if (widget == nullptr) {
            qWarning() << "UI Tweaks: widget not found:" << name;
            continue;
        }
        if (widget->isHidden()) {
            continue;
        }
        widget->hide();
        m_hiddenByUs.append(widget);
    }
}

void HiddenWidgetsTweak::revert() {
    for (const QPointer<QWidget>& widget : std::as_const(m_hiddenByUs)) {
        if (widget) {
            widget->show();
        }
    }
    m_hiddenByUs.clear();
}
