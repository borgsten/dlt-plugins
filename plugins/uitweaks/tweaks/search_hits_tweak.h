#pragma once

#include <optional>

#include <QAbstractItemDelegate>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>
#include <qdlt/qdltfile.h>

#include "plugin_utils/plugin_settings.h"

#include "tweaks/itweak.h"

class SearchHitsDelegate;
struct SearchMatcher;

/*
 * Tints the rows of the log table that the last Find All matches, live ones
 * included, by matching the visible rows with the search's own matcher, read
 * back from the search dialog. The search results themselves are not used:
 * copying them does not scale to huge files. Plugin decoders are not applied,
 * so their payloads match undecoded.
 */
class SearchHitsTweak : public QObject, public ITweak {
    Q_OBJECT

 public:
    SearchHitsTweak(plugin_utils::PluginSettings settings, QWidget* main_window, QString log_table_name,
                    QString search_table_name, int index_column);

    void apply() override;
    void revert() override;
    void buildSettings(plugin_utils::SettingsForm& form) override;

    /* The viewer's file, whose messages the rows show */
    void setFile(QDltFile* file);

 private:
    void attach();
    // A slot, since the search dialog's class is not available to connect to.
    Q_SLOT void onSearchProgressChanged(bool in_progress);
    void setSearch(std::optional<SearchMatcher> search);

    plugin_utils::PluginSettings settings_;
    QPointer<QWidget> main_window_;
    QString log_table_name_;
    QString search_table_name_;
    int index_column_;
    bool applied_ = false;
    QPointer<QAbstractItemView> log_view_;
    QPointer<QAbstractItemDelegate> previous_delegate_;
    QPointer<SearchHitsDelegate> delegate_;
    QPointer<QObject> search_dialog_;
    QDltFile* file_ = nullptr;
    QList<QMetaObject::Connection> connections_;
    bool searching_ = false;
};
