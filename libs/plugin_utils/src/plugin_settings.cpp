#include "plugin_utils/plugin_settings.h"

#include <utility>

#include <qdlt/qdltsettingsmanager.h>

namespace plugin_utils {

PluginSettings::PluginSettings(QString group) : group_(std::move(group)) {}

void PluginSettings::setValue(const QString& key, const QVariant& value) const {
    QDltSettingsManager::getInstance()->setValue(fullKey(key), value);
}

PluginSettings PluginSettings::group(const QString& name) const { return PluginSettings(fullKey(name)); }

QVariant PluginSettings::rawValue(const QString& key, const QVariant& default_value) const {
    return QDltSettingsManager::getInstance()->value(fullKey(key), default_value);
}

QString PluginSettings::fullKey(const QString& key) const { return group_ + u'/' + key; }

}  // namespace plugin_utils
