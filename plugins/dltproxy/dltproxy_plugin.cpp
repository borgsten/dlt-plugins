#include "dltproxy_plugin.h"

#include <cstdint>
#include <string>
#include <utility>

#include <QCoreApplication>
#include <QLatin1StringView>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QtLogging>

#include "plugin_utils/plugin_info.h"
#include "plugin_utils/plugin_settings.h"

using namespace Qt::StringLiterals;

namespace {

constexpr plugin_utils::PluginInfo kInfo{
    .name = "DLT Proxy"_L1,
    .version = "0.1.0"_L1,
    .description = "Forwards selected apps from a DLT source to a local port"_L1,
    .settings_group = "dltproxy"_L1,
};

constexpr int kMaxPort = 65535;
constexpr auto kSourceHost = "sourceHost"_L1;
constexpr auto kDefaultSourceHost = "localhost"_L1;
constexpr auto kSourcePort = "sourcePort"_L1;
constexpr int kDefaultSourcePort = 3490;
constexpr auto kListenPort = "listenPort"_L1;
constexpr int kDefaultListenPort = 9999;
constexpr auto kAppIds = "appIds"_L1;

void logMessage(const std::string& message) { qInfo().noquote() << "DLT Proxy:" << QString::fromStdString(message); }

}  // namespace

DltProxyPlugin::DltProxyPlugin() : ConfigurablePlugin(kInfo) {
    // Stop while Qt, which the proxy logs through, is still fully up.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { proxy_.reset(); });
}

void DltProxyPlugin::onEnabledChanged(bool /*enabled*/) { restart(); }

void DltProxyPlugin::buildSettingsForm(plugin_utils::SettingsForm& form) {
    const auto on_changed = [this](const auto& /*value*/) { restart(); };
    form.addLineEdit({.key = kSourceHost, .label = "Source host", .default_value = kDefaultSourceHost}, on_changed);
    form.addSpinBox(
        {
            .key = kSourcePort,
            .label = "Source port",
            .default_value = kDefaultSourcePort,
            .minimum = 1,
            .maximum = kMaxPort,
        },
        on_changed);
    form.addSpinBox(
        {
            .key = kListenPort,
            .label = "Listen port",
            .default_value = kDefaultListenPort,
            .minimum = 1,
            .maximum = kMaxPort,
        },
        on_changed);
    form.addLineEdit({.key = kAppIds, .label = "App IDs", .placeholder = "All"}, on_changed);
}

void DltProxyPlugin::restart() {
    proxy_.reset();
    if (!isEnabled()) {
        return;
    }

    if (auto options = readOptions()) {
        proxy_.emplace(std::move(*options));
    }
}

std::optional<dlt_proxy::Options> DltProxyPlugin::readOptions() const {
    const plugin_utils::PluginSettings& config = settings();
    dlt_proxy::Options options{
        .source_host = config.value(kSourceHost, QString(kDefaultSourceHost)).toStdString(),
        .source_port = static_cast<std::uint16_t>(config.value(kSourcePort, kDefaultSourcePort)),
        .listen_port = static_cast<std::uint16_t>(config.value(kListenPort, kDefaultListenPort)),
        .app_ids = {},
        .log = logMessage,
    };
    static const QRegularExpression kSeparators(u"[\\s,]+"_s);
    for (const QString& text : config.value(kAppIds, QString()).split(kSeparators, Qt::SkipEmptyParts)) {
        const auto app_id = dlt_proxy::toId(text.toStdString());
        if (!app_id) {
            logMessage((u"Not started, an app ID has 1 to 4 characters: "_s + text).toStdString());
            return std::nullopt;
        }
        options.app_ids.push_back(*app_id);
    }
    return options;
}
