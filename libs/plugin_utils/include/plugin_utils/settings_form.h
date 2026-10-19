#pragma once

#include <functional>
#include <limits>
#include <memory>

#include <QFormLayout>
#include <QGroupBox>
#include <QList>
#include <QString>
#include <QWidget>

#include "plugin_utils/plugin_settings.h"

namespace plugin_utils {

struct CheckBoxField {
    QString key;
    QString label;
    bool default_value = false;
};

struct ComboBoxField {
    struct Choice {
        QString value;
        QString label;
    };

    QString key;
    QString label;
    QList<Choice> choices;
    QString default_value;
};

struct CheckableGroupField {
    QString key;
    QString title;
    bool default_value = false;
    QString settings_group = {};
};

struct GroupField {
    QString title;
    QString settings_group = {};
};

struct LineEditField {
    QString key;
    QString label;
    QString default_value = {};
    QString placeholder = {};
};

struct SpinBoxField {
    QString key;
    QString label;
    int default_value = 0;
    int minimum = 0;
    int maximum = std::numeric_limits<int>::max();
};

/*
 * Fields bound to PluginSettings keys. Changes are stored before on_changed is
 * called. Groups use their settings_group, if set.
 */
class SettingsForm : public QWidget {
 public:
    explicit SettingsForm(PluginSettings settings, QWidget* parent = nullptr);

    void addCheckBox(const CheckBoxField& field, std::function<void(bool)> on_changed = {});
    void addSpinBox(const SpinBoxField& field, std::function<void(int)> on_changed = {});
    void addLineEdit(const LineEditField& field, std::function<void(QString)> on_changed = {});
    /* Stores the chosen Choice::value */
    void addComboBox(const ComboBoxField& field, std::function<void(QString)> on_changed = {});
    SettingsForm& addGroup(const GroupField& field);
    /*
     * A group box with a checkbox in its title, around the fields build_fields
     * adds; a plain checkbox if it adds none.
     */
    void addCheckableGroup(const CheckableGroupField& field, const std::function<void(SettingsForm&)>& build_fields,
                           std::function<void(bool)> on_changed = {});

    [[nodiscard]] bool isEmpty() const { return layout_.rowCount() == 0; }

 private:
    [[nodiscard]] std::unique_ptr<SettingsForm> createSubForm(const QString& settings_group) const;
    void addBox(std::unique_ptr<QGroupBox> box, std::unique_ptr<SettingsForm> content);

    PluginSettings settings_;
    // Destroyed before the QWidget base, which then no longer owns it.
    QFormLayout layout_{this};
};

}  // namespace plugin_utils
