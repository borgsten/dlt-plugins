#pragma once

#include <functional>
#include <limits>
#include <memory>

#include <QFormLayout>
#include <QGroupBox>
#include <QString>
#include <QWidget>

#include "plugin_utils/plugin_settings.h"

namespace plugin_utils {

struct CheckBoxField {
    QString key;
    QString label;
    bool defaultValue = false;
};

struct CheckableGroupField {
    QString key;
    QString title;
    bool defaultValue = false;
    QString settingsGroup = {};
};

struct GroupField {
    QString title;
    QString settingsGroup = {};
};

struct LineEditField {
    QString key;
    QString label;
    QString defaultValue = {};
    QString placeholder = {};
};

struct SpinBoxField {
    QString key;
    QString label;
    int defaultValue = 0;
    int minimum = 0;
    int maximum = std::numeric_limits<int>::max();
};

/*
 * Fields bound to PluginSettings keys. Changes are stored before onChanged is
 * called. Groups use their settingsGroup, if set.
 */
class SettingsForm : public QWidget {
 public:
    explicit SettingsForm(PluginSettings settings, QWidget* parent = nullptr);

    void addCheckBox(const CheckBoxField& field, std::function<void(bool)> onChanged = {});
    void addSpinBox(const SpinBoxField& field, std::function<void(int)> onChanged = {});
    void addLineEdit(const LineEditField& field, std::function<void(QString)> onChanged = {});
    SettingsForm& addGroup(const GroupField& field);
    /*
     * A group box with a checkbox in its title, around the fields buildFields
     * adds; a plain checkbox if it adds none.
     */
    void addCheckableGroup(const CheckableGroupField& field, const std::function<void(SettingsForm&)>& buildFields,
                           std::function<void(bool)> onChanged = {});

    [[nodiscard]] bool isEmpty() const { return m_layout.rowCount() == 0; }

 private:
    [[nodiscard]] std::unique_ptr<SettingsForm> createSubForm(const QString& settingsGroup) const;
    void addBox(std::unique_ptr<QGroupBox> box, std::unique_ptr<SettingsForm> content);

    PluginSettings m_settings;
    // Destroyed before the QWidget base, which then no longer owns it.
    QFormLayout m_layout{this};
};

}  // namespace plugin_utils
