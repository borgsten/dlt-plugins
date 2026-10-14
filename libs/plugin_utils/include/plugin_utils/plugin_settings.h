#pragma once

#include <QString>
#include <QVariant>

namespace plugin_utils {

/*
 * The plugin's part of the viewer's settings: every key is placed under the
 * plugin's group. Use QDltSettingsManager directly for the viewer's settings.
 */
class PluginSettings {
 public:
    explicit PluginSettings(QString group);

    template <typename T>
    [[nodiscard]] T value(const QString& key, const T& defaultValue) const {
        return rawValue(key, QVariant::fromValue(defaultValue)).template value<T>();
    }
    void setValue(const QString& key, const QVariant& value) const;

    [[nodiscard]] PluginSettings group(const QString& name) const;

 private:
    [[nodiscard]] QVariant rawValue(const QString& key, const QVariant& defaultValue) const;
    [[nodiscard]] QString fullKey(const QString& key) const;

    QString m_group;
};

}  // namespace plugin_utils
