#include "plugin_utils/settings_form.h"

#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QMargins>
#include <QSpinBox>
#include <QVBoxLayout>

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

}  // namespace

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
    connect(spin_box.get(), &QSpinBox::valueChanged, this, storeAndNotify(settings_, field.key, std::move(on_changed)));
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

void SettingsForm::addComboBox(const ComboBoxField& field, std::function<void(QString)> on_changed) {
    auto combo_box = std::make_unique<QComboBox>();
    for (const ComboBoxField::Choice& choice : field.choices) {
        combo_box->addItem(choice.label, choice.value);
    }
    const int current = combo_box->findData(settings_.value(field.key, field.default_value));
    combo_box->setCurrentIndex(current >= 0 ? current : combo_box->findData(field.default_value));
    connect(combo_box.get(), &QComboBox::currentIndexChanged, this,
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
