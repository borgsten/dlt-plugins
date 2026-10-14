#include "plugin_utils/settings_form.h"

#include <utility>

#include <QCheckBox>
#include <QMargins>
#include <QSpinBox>
#include <QVBoxLayout>

namespace plugin_utils {

namespace {

template <typename T>
auto storeAndNotify(const PluginSettings& settings, QString key, std::function<void(T)> onChanged) {
    return [settings, key = std::move(key), onChanged = std::move(onChanged)](T value) {
        settings.setValue(key, value);
        if (onChanged) {
            onChanged(value);
        }
    };
}

}  // namespace

SettingsForm::SettingsForm(PluginSettings settings, QWidget* parent)
    : QWidget(parent), m_settings(std::move(settings)) {}

void SettingsForm::addCheckBox(const CheckBoxField& field, std::function<void(bool)> onChanged) {
    auto checkBox = std::make_unique<QCheckBox>(field.label);
    checkBox->setChecked(m_settings.value(field.key, field.defaultValue));
    connect(checkBox.get(), &QCheckBox::toggled, this, storeAndNotify(m_settings, field.key, std::move(onChanged)));
    m_layout.addRow(checkBox.release());
}

void SettingsForm::addSpinBox(const SpinBoxField& field, std::function<void(int)> onChanged) {
    auto spinBox = std::make_unique<QSpinBox>();
    spinBox->setRange(field.minimum, field.maximum);
    spinBox->setValue(m_settings.value(field.key, field.defaultValue));
    // Don't report every typed digit.
    spinBox->setKeyboardTracking(false);
    connect(spinBox.get(), &QSpinBox::valueChanged, this, storeAndNotify(m_settings, field.key, std::move(onChanged)));
    m_layout.addRow(field.label, spinBox.release());
}

SettingsForm& SettingsForm::addGroup(const GroupField& field) {
    auto content = createSubForm(field.settingsGroup);
    SettingsForm& ref = *content;
    addBox(std::make_unique<QGroupBox>(field.title), std::move(content));
    return ref;
}

void SettingsForm::addCheckableGroup(const CheckableGroupField& field,
                                     const std::function<void(SettingsForm&)>& buildFields,
                                     std::function<void(bool)> onChanged) {
    auto content = createSubForm(field.settingsGroup);
    buildFields(*content);
    if (content->isEmpty()) {
        addCheckBox({.key = field.key, .label = field.title, .defaultValue = field.defaultValue}, std::move(onChanged));
        return;
    }

    // Unchecking the box also disables its fields.
    auto box = std::make_unique<QGroupBox>(field.title);
    box->setCheckable(true);
    box->setChecked(m_settings.value(field.key, field.defaultValue));
    connect(box.get(), &QGroupBox::toggled, this, storeAndNotify(m_settings, field.key, std::move(onChanged)));
    addBox(std::move(box), std::move(content));
}

std::unique_ptr<SettingsForm> SettingsForm::createSubForm(const QString& settingsGroup) const {
    return std::make_unique<SettingsForm>(settingsGroup.isEmpty() ? m_settings : m_settings.group(settingsGroup));
}

void SettingsForm::addBox(std::unique_ptr<QGroupBox> box, std::unique_ptr<SettingsForm> content) {
    content->m_layout.setContentsMargins(QMargins());
    auto boxLayout = std::make_unique<QVBoxLayout>();
    boxLayout->addWidget(content.release());
    box->setLayout(boxLayout.release());
    m_layout.addRow(box.release());
}

}  // namespace plugin_utils
