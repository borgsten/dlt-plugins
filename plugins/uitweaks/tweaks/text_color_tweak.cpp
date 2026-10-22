#include "tweaks/text_color_tweak.h"

#include <memory>
#include <utility>

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QDebug>
#include <QModelIndex>
#include <QPalette>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QtLogging>

using namespace Qt::StringLiterals;

namespace {

const auto kThresholdKey = u"threshold"_s;
// The viewer uses 166, which gives white text on most marker colors.
constexpr int kDefaultThreshold = 100;
constexpr int kMaxBrightness = 255;

constexpr auto kFilterDialogClass = "FilterDialog";
const auto kColorPreviewName = u"labelSelectedColor"_s;

// Perceived brightness, 0 to 255, weighted like the viewer's optimalTextColor().
constexpr double kRedWeight = 0.299;
constexpr double kGreenWeight = 0.587;
constexpr double kBlueWeight = 0.114;

int brightness(const QColor& color) {
    return static_cast<int>((kRedWeight * color.red()) + (kGreenWeight * color.green()) + (kBlueWeight * color.blue()));
}

QColor textColorFor(const QColor& background, int threshold) {
    return brightness(background) > threshold ? Qt::black : Qt::white;
}

}  // namespace

class TextColorDelegate : public QStyledItemDelegate {
 public:
    TextColorDelegate(int threshold, QObject* parent) : QStyledItemDelegate(parent), threshold_(threshold) {}

    void setThreshold(int threshold) { threshold_ = threshold; }

 protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        // Filled in from the model's BackgroundRole by the base class.
        if (option->backgroundBrush.style() == Qt::NoBrush) {
            return;
        }
        option->palette.setColor(QPalette::Text, textColorFor(option->backgroundBrush.color(), threshold_));
    }

 private:
    int threshold_;
};

TextColorTweak::TextColorTweak(plugin_utils::PluginSettings settings, QWidget* main_window, QStringList view_names)
    : settings_(std::move(settings)), main_window_(main_window), view_names_(std::move(view_names)) {}

void TextColorTweak::apply() {
    if (applied_ || !main_window_) {
        return;
    }
    applied_ = true;

    for (const QString& name : std::as_const(view_names_)) {
        auto* view = main_window_->findChild<QAbstractItemView*>(name);
        if (view == nullptr) {
            qWarning() << "UI Tweaks: view not found:" << name;
            continue;
        }
        // The view owns the delegate through its parent.
        auto delegate = std::make_unique<TextColorDelegate>(threshold(), view);
        views_.append({.view = view, .previous_delegate = view->itemDelegate(), .delegate = delegate.get()});
        view->setItemDelegate(delegate.release());
    }

    // The viewer creates a new filter dialog for every edit; catch it when shown.
    QApplication::instance()->installEventFilter(this);
}

void TextColorTweak::revert() {
    if (!applied_) {
        return;
    }
    applied_ = false;

    QApplication::instance()->removeEventFilter(this);
    for (const QPointer<QWidget>& label : std::as_const(color_previews_)) {
        if (label) {
            label->removeEventFilter(this);
        }
    }
    color_previews_.clear();

    for (const View& view : std::as_const(views_)) {
        if (view.view && view.view->itemDelegate() == view.delegate) {
            view.view->setItemDelegate(view.previous_delegate);
        }
        if (view.delegate) {
            view.delegate->deleteLater();
        }
    }
    views_.clear();
}

void TextColorTweak::buildSettings(plugin_utils::SettingsForm& form) {
    form.addSpinBox(
        {
            .key = kThresholdKey,
            .label = "Dark text above brightness",
            .default_value = kDefaultThreshold,
            .minimum = 0,
            .maximum = kMaxBrightness,
        },
        [this](int threshold) { setThreshold(threshold); });
}

bool TextColorTweak::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Show && watched->inherits(kFilterDialogClass)) {
        watchColorPreview(watched);
    } else if (event->type() == QEvent::PaletteChange && color_previews_.contains(watched)) {
        updateColorPreview(qobject_cast<QWidget*>(watched));
    }
    return false;
}

int TextColorTweak::threshold() const { return settings_.value(kThresholdKey, kDefaultThreshold); }

void TextColorTweak::setThreshold(int threshold) {
    for (const View& view : std::as_const(views_)) {
        if (view.delegate && view.view) {
            view.delegate->setThreshold(threshold);
            view.view->viewport()->update();
        }
    }
    for (const QPointer<QWidget>& label : std::as_const(color_previews_)) {
        if (label) {
            updateColorPreview(label);
        }
    }
}

void TextColorTweak::watchColorPreview(QObject* dialog) {
    auto* label = dialog->findChild<QWidget*>(kColorPreviewName);
    if (label == nullptr || color_previews_.contains(label)) {
        return;
    }
    color_previews_.removeAll(nullptr);
    color_previews_.append(label);
    // The dialog sets the label's palette for every picked color.
    label->installEventFilter(this);
    updateColorPreview(label);
}

void TextColorTweak::updateColorPreview(QWidget* label) const {
    QPalette palette = label->palette();
    // The dialog only sets the active background to the picked color.
    const QColor text = textColorFor(palette.color(QPalette::Active, label->backgroundRole()), threshold());
    if (palette.color(QPalette::Active, QPalette::WindowText) == text &&
        palette.color(QPalette::Inactive, QPalette::WindowText) == text) {
        return;
    }
    // Disabled text keeps the dialog's color.
    palette.setColor(QPalette::Active, QPalette::WindowText, text);
    palette.setColor(QPalette::Inactive, QPalette::WindowText, text);
    // Triggers another PaletteChange, which then finds the color already set.
    label->setPalette(palette);
}
