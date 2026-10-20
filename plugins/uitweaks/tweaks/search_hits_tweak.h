#pragma once

#include <QAbstractItemDelegate>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QWidget>

#include "plugin_utils/plugin_settings.h"

#include "tweaks/itweak.h"

class SearchHitsDelegate;

/*
 * Tints the rows of the log table that are in the search results, once a
 * search is done. Both tables show the global message index in the index
 * column, which is how rows are matched; the viewer has no other way to reach
 * its search results.
 */
class SearchHitsTweak : public QObject, public ITweak {
    Q_OBJECT

 public:
    SearchHitsTweak(plugin_utils::PluginSettings settings, QWidget* main_window, QString log_table_name,
                    QString search_table_name, int index_column);

    void apply() override;
    void revert() override;
    void buildSettings(plugin_utils::SettingsForm& form) override;

 private:
    void attach();
    // A slot, since the search dialog's class is not available to connect to.
    Q_SLOT void onSearchProgressChanged(bool in_progress);
    void rebuildHits();

    plugin_utils::PluginSettings settings_;
    QPointer<QWidget> main_window_;
    QString log_table_name_;
    QString search_table_name_;
    int index_column_;
    bool applied_ = false;
    QPointer<QAbstractItemView> log_view_;
    QPointer<QAbstractItemDelegate> previous_delegate_;
    QPointer<SearchHitsDelegate> delegate_;
    QPointer<QAbstractItemModel> search_model_;
    QList<QMetaObject::Connection> connections_;
    QTimer rebuild_timer_;
    bool searching_ = false;
};
