#include "tweak_list.h"

#include <ranges>

#include <QDebug>
#include <QtLogging>

using namespace Qt::StringLiterals;

namespace {

const auto kEnabledKey = u"enabled"_s;
constexpr bool kEnabledByDefault = true;

}  // namespace

TweakList::TweakList(plugin_utils::PluginSettings settings) : settings_(std::move(settings)) {}

void TweakList::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (enabled) {
        for (const Entry& entry : entries_) {
            if (isTweakEnabled(entry)) {
                applyTweak(entry);
            }
        }
    } else {
        // Later tweaks may build on earlier ones.
        for (const Entry& entry : std::views::reverse(entries_)) {
            revertTweak(entry);
        }
    }
}

void TweakList::buildSettingsForm(plugin_utils::SettingsForm& form) {
    // entries_ does not change once the tweaks are added, so the entries stay put.
    for (const Entry& entry : entries_) {
        form.addCheckableGroup(
            {
                .key = entry.info.id + u'/' + kEnabledKey,
                .title = entry.info.title,
                .default_value = kEnabledByDefault,
                .settings_group = entry.info.id,
            },
            [&entry](plugin_utils::SettingsForm& fields) { entry.tweak->buildSettings(fields); },
            [this, entry_ptr = &entry](bool enabled) { onTweakToggled(*entry_ptr, enabled); });
    }
}

bool TweakList::isTweakEnabled(const Entry& entry) const {
    return settings_.group(entry.info.id).value(kEnabledKey, kEnabledByDefault);
}

void TweakList::onTweakToggled(const Entry& entry, bool enabled) const {
    if (!enabled_) {
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
