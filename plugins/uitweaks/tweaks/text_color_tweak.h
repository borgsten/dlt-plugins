#pragma once

#include <QAbstractItemDelegate>
#include <QAbstractItemView>
#include <QEvent>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QWidget>

#include "plugin_utils/plugin_settings.h"

#include "tweaks/itweak.h"

class TextColorDelegate;

/*
 * Uses black text on colored rows of the given item views, and on the color
 * preview of the filter dialog, unless the background is darker than a
 * configurable brightness; the viewer switches to white text already on
 * fairly light colors.
 */
class TextColorTweak : public QObject, public ITweak {
    Q_OBJECT

 public:
    TextColorTweak(plugin_utils::PluginSettings settings, QWidget* main_window, QStringList view_names);

    void apply() override;
    void revert() override;
    void buildSettings(plugin_utils::SettingsForm& form) override;

    bool eventFilter(QObject* watched, QEvent* event) override;

 private:
    struct View {
        QPointer<QAbstractItemView> view;
        QPointer<QAbstractItemDelegate> previous_delegate;
        QPointer<TextColorDelegate> delegate;
    };

    [[nodiscard]] int threshold() const;
    void setThreshold(int threshold);
    void watchColorPreview(QObject* dialog);
    void updateColorPreview(QWidget* label) const;

    plugin_utils::PluginSettings settings_;
    QPointer<QWidget> main_window_;
    QStringList view_names_;
    bool applied_ = false;
    QList<View> views_;
    QList<QPointer<QWidget>> color_previews_;
};
