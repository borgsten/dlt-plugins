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
    .settingsGroup = "dltproxy"_L1,
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
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { m_proxy.reset(); });
}

void DltProxyPlugin::onEnabledChanged(bool /*enabled*/) { restart(); }

void DltProxyPlugin::buildSettingsForm(plugin_utils::SettingsForm& form) {
    const auto onChanged = [this](const auto& /*value*/) { restart(); };
    form.addLineEdit({.key = kSourceHost, .label = "Source host", .defaultValue = kDefaultSourceHost}, onChanged);
    form.addSpinBox(
        {
            .key = kSourcePort,
            .label = "Source port",
            .defaultValue = kDefaultSourcePort,
            .minimum = 1,
            .maximum = kMaxPort,
        },
        onChanged);
    form.addSpinBox(
        {
            .key = kListenPort,
            .label = "Listen port",
            .defaultValue = kDefaultListenPort,
            .minimum = 1,
            .maximum = kMaxPort,
        },
        onChanged);
    form.addLineEdit({.key = kAppIds, .label = "App IDs", .placeholder = "All"}, onChanged);
}

void DltProxyPlugin::restart() {
    m_proxy.reset();
    if (!isEnabled()) {
        return;
    }

    if (auto options = readOptions()) {
        m_proxy.emplace(std::move(*options));
    }
}

std::optional<dlt_proxy::Options> DltProxyPlugin::readOptions() const {
    const plugin_utils::PluginSettings& config = settings();
    dlt_proxy::Options options{
        .sourceHost = config.value(kSourceHost, QString(kDefaultSourceHost)).toStdString(),
        .sourcePort = static_cast<std::uint16_t>(config.value(kSourcePort, kDefaultSourcePort)),
        .listenPort = static_cast<std::uint16_t>(config.value(kListenPort, kDefaultListenPort)),
        .appIds = {},
        .log = logMessage,
    };
    static const QRegularExpression kSeparators(u"[\\s,]+"_s);
    for (const QString& text : config.value(kAppIds, QString()).split(kSeparators, Qt::SkipEmptyParts)) {
        const auto appId = dlt_proxy::toId(text.toStdString());
        if (!appId) {
            logMessage((u"Not started, an app ID has 1 to 4 characters: "_s + text).toStdString());
            return std::nullopt;
        }
        options.appIds.push_back(*appId);
    }
    return options;
}
