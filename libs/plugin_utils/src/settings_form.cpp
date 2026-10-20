#include "plugin_utils/settings_form.h"

#include <memory>
#include <utility>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QMargins>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSize>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtMath>

#include "plugin_utils/qt_compat.h"

using namespace Qt::StringLiterals;

namespace plugin_utils {

namespace {

template <typename T>
auto storeAndNotify(const PluginSettings& settings, QString key, std::function<void(T)> on_changed) {
    return [settings, key = std::move(key), on_changed = std::move(on_changed)](const T& value) {
        settings.setValue(key, value);
        if (on_changed) {
            on_changed(value);
        }
    };
}

constexpr QSize kSwatchSize{24, 16};

QIcon swatch(const QColor& color) {
    QPixmap pixmap(kSwatchSize);
    // Opaque, so the color reads the same on any button background.
    pixmap.fill(QColor(color.rgb()));
    return {pixmap};
}

}  // namespace

QColor colorValue(const PluginSettings& settings, const ColorField& field) {
    QColor color = colorFromString(settings.value(field.key, field.default_value));
    if (!color.isValid()) {
        color = colorFromString(field.default_value);
    }
    color.setAlphaF(static_cast<float>(settings.value(field.opacity_key, field.default_opacity)) / kMaxOpacity);
    return color;
}

SettingsForm::SettingsForm(PluginSettings settings, QWidget* parent)
    : QWidget(parent), settings_(std::move(settings)) {}

void SettingsForm::addCheckBox(const CheckBoxField& field, std::function<void(bool)> on_changed) {
    auto check_box = std::make_unique<QCheckBox>(field.label);
    check_box->setChecked(settings_.value(field.key, field.default_value));
    connect(check_box.get(), &QCheckBox::toggled, this, storeAndNotify(settings_, field.key, std::move(on_changed)));
    layout_.addRow(check_box.release());
}

void SettingsForm::addSpinBox(const SpinBoxField& field, std::function<void(int)> on_changed) {
    auto spin_box = std::make_unique<QSpinBox>();
    spin_box->setRange(field.minimum, field.maximum);
    spin_box->setValue(settings_.value(field.key, field.default_value));
    // Don't report every typed digit.
    spin_box->setKeyboardTracking(false);
    connect(spin_box.get(), qOverload<int>(&QSpinBox::valueChanged), this,
            storeAndNotify(settings_, field.key, std::move(on_changed)));
    layout_.addRow(field.label, spin_box.release());
}

void SettingsForm::addLineEdit(const LineEditField& field, std::function<void(QString)> on_changed) {
    auto line_edit = std::make_unique<QLineEdit>(settings_.value(field.key, field.default_value));
    line_edit->setPlaceholderText(field.placeholder);
    // editingFinished is also emitted when focus leaves an unchanged field.
    connect(line_edit.get(), &QLineEdit::editingFinished, this,
            [line_edit = line_edit.get(), store = storeAndNotify(settings_, field.key, std::move(on_changed))] {
                if (line_edit->isModified()) {
                    line_edit->setModified(false);
                    store(line_edit->text());
                }
            });
    layout_.addRow(field.label, line_edit.release());
}

void SettingsForm::addColorPicker(const ColorField& field, std::function<void(QColor)> on_changed) {
    auto row = std::make_unique<QWidget>();
    auto* row_layout = std::make_unique<QHBoxLayout>(row.get()).release();
    row_layout->setContentsMargins(QMargins());

    const QColor color = colorValue(settings_, field);
    auto button = std::make_unique<QPushButton>(swatch(color), color.name());
    auto opacity = std::make_unique<QSpinBox>();
    opacity->setRange(0, kMaxOpacity);
    opacity->setSuffix(u" %"_s);
    opacity->setValue(settings_.value(field.opacity_key, field.default_opacity));
    // Don't report every typed digit.
    opacity->setKeyboardTracking(false);

    const auto notify = [settings = settings_, field, on_changed = std::move(on_changed)] {
        if (on_changed) {
            on_changed(colorValue(settings, field));
        }
    };
    connect(opacity.get(), qOverload<int>(&QSpinBox::valueChanged), this,
            [settings = settings_, field, notify](int value) {
                settings.setValue(field.opacity_key, value);
                notify();
            });
    connect(button.get(), &QPushButton::clicked, this,
            [settings = settings_, field, notify, button = button.get(), opacity = opacity.get()] {
                const QColor picked = QColorDialog::getColor(colorValue(settings, field), button, field.label,
                                                             QColorDialog::ShowAlphaChannel);
                if (!picked.isValid()) {
                    return;
                }
                settings.setValue(field.key, picked.name());
                settings.setValue(field.opacity_key, qRound(picked.alphaF() * kMaxOpacity));
                button->setIcon(swatch(picked));
                button->setText(picked.name());
                // Already stored, and reported once below.
                const QSignalBlocker blocker(opacity);
                opacity->setValue(qRound(picked.alphaF() * kMaxOpacity));
                notify();
            });

    row_layout->addWidget(button.release());
    row_layout->addWidget(opacity.release());
    row_layout->addStretch();
    layout_.addRow(field.label, row.release());
}

void SettingsForm::addComboBox(const ComboBoxField& field, std::function<void(QString)> on_changed) {
    auto combo_box = std::make_unique<QComboBox>();
    for (const ComboBoxField::Choice& choice : field.choices) {
        combo_box->addItem(choice.label, choice.value);
    }
    const int current = combo_box->findData(settings_.value(field.key, field.default_value));
    combo_box->setCurrentIndex(current >= 0 ? current : combo_box->findData(field.default_value));
    connect(combo_box.get(), qOverload<int>(&QComboBox::currentIndexChanged), this,
            [combo_box = combo_box.get(), store = storeAndNotify(settings_, field.key, std::move(on_changed))] {
                store(combo_box->currentData().toString());
            });
    layout_.addRow(field.label, combo_box.release());
}

SettingsForm& SettingsForm::addGroup(const GroupField& field) {
    auto content = createSubForm(field.settings_group);
    SettingsForm& ref = *content;
    addBox(std::make_unique<QGroupBox>(field.title), std::move(content));
    return ref;
}

void SettingsForm::addCheckableGroup(const CheckableGroupField& field,
                                     const std::function<void(SettingsForm&)>& build_fields,
                                     std::function<void(bool)> on_changed) {
    auto content = createSubForm(field.settings_group);
    build_fields(*content);
    if (content->isEmpty()) {
        addCheckBox({.key = field.key, .label = field.title, .default_value = field.default_value},
                    std::move(on_changed));
        return;
    }

    // Unchecking the box also disables its fields.
    auto box = std::make_unique<QGroupBox>(field.title);
    box->setCheckable(true);
    box->setChecked(settings_.value(field.key, field.default_value));
    connect(box.get(), &QGroupBox::toggled, this, storeAndNotify(settings_, field.key, std::move(on_changed)));
    addBox(std::move(box), std::move(content));
}

std::unique_ptr<SettingsForm> SettingsForm::createSubForm(const QString& settings_group) const {
    return std::make_unique<SettingsForm>(settings_group.isEmpty() ? settings_ : settings_.group(settings_group));
}

void SettingsForm::addBox(std::unique_ptr<QGroupBox> box, std::unique_ptr<SettingsForm> content) {
    content->layout_.setContentsMargins(QMargins());
    auto box_layout = std::make_unique<QVBoxLayout>();
    box_layout->addWidget(content.release());
    box->setLayout(box_layout.release());
    layout_.addRow(box.release());
}

}  // namespace plugin_utils
