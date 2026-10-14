#pragma once

#include <QList>
#include <QPointer>
#include <QStringList>
#include <QWidget>

#include "tweaks/itweak.h"

/* Hides main window widgets by object name. */
class HiddenWidgetsTweak : public ITweak {
 public:
    HiddenWidgetsTweak(QWidget* mainWindow, QStringList objectNames);

    void apply() override;
    void revert() override;

 private:
    QPointer<QWidget> m_mainWindow;
    QStringList m_objectNames;
    QList<QPointer<QWidget>> m_hiddenByUs;
};
