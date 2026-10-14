#include "tweak_list.h"

#include <ranges>

#include <QDebug>
#include <QtLogging>

using namespace Qt::StringLiterals;

namespace {

const auto kEnabledKey = u"enabled"_s;
constexpr bool kEnabledByDefault = true;

}  // namespace

TweakList::TweakList(plugin_utils::PluginSettings settings) : m_settings(std::move(settings)) {}

void TweakList::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (enabled) {
        for (const Entry& entry : m_entries) {
            if (isTweakEnabled(entry)) {
                applyTweak(entry);
            }
        }
    } else {
        // Later tweaks may build on earlier ones.
        for (const Entry& entry : std::views::reverse(m_entries)) {
            revertTweak(entry);
        }
    }
}

void TweakList::buildSettingsForm(plugin_utils::SettingsForm& form) {
    // m_entries does not change once the tweaks are added, so the entries stay put.
    for (const Entry& entry : m_entries) {
        form.addCheckableGroup(
            {
                .key = entry.info.id + u'/' + kEnabledKey,
                .title = entry.info.title,
                .defaultValue = kEnabledByDefault,
                .settingsGroup = entry.info.id,
            },
            [&entry](plugin_utils::SettingsForm& fields) { entry.tweak->buildSettings(fields); },
            [this, entryPtr = &entry](bool enabled) { onTweakToggled(*entryPtr, enabled); });
    }
}

bool TweakList::isTweakEnabled(const Entry& entry) const {
    return m_settings.group(entry.info.id).value(kEnabledKey, kEnabledByDefault);
}

void TweakList::onTweakToggled(const Entry& entry, bool enabled) const {
    if (!m_enabled) {
        return;
    }
    if (enabled) {
        applyTweak(entry);
    } else {
        revertTweak(entry);
    }
}

void TweakList::applyTweak(const Entry& entry) {
    qDebug() << "UI Tweaks: applying" << entry.info.id;
    entry.tweak->apply();
}

void TweakList::revertTweak(const Entry& entry) {
    qDebug() << "UI Tweaks: reverting" << entry.info.id;
    entry.tweak->revert();
}
