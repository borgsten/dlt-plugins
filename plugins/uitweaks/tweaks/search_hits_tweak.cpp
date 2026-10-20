#include "tweaks/search_hits_tweak.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include <QDebug>
#include <QDialog>
#include <QEvent>
#include <QHelpEvent>
#include <QLocale>
#include <QModelIndex>
#include <QPainter>
#include <QSet>
#include <QSize>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVariant>
#include <QtLogging>

using namespace Qt::StringLiterals;

namespace {

const plugin_utils::ColorField kColorField{
    .key = u"color"_s,
    .opacity_key = u"opacity"_s,
    .label = u"Color"_s,
    .default_value = u"#ffb300"_s,
    .default_opacity = 30,
};

constexpr auto kSearchDialogClass = "CSearchDialog";

// Both tables show the index with group separators ("%L1").
std::optional<qulonglong> messageIndex(const QModelIndex& index) {
    bool parsed = false;
    const qulonglong value = QLocale().toULongLong(index.data().toString(), &parsed);
    return parsed ? std::optional(value) : std::nullopt;
}

}  // namespace

/* Paints like the delegate it replaces, then tints the rows of search hits */
class SearchHitsDelegate : public QAbstractItemDelegate {
 public:
    SearchHitsDelegate(QAbstractItemDelegate* inner, int index_column, QColor color, QObject* parent)
        : QAbstractItemDelegate(parent), inner_(inner), index_column_(index_column), color_(color) {}

    void setColor(QColor color) { color_ = color; }
    void setHits(QSet<qulonglong> hits) { hits_ = std::move(hits); }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        inner()->paint(painter, option, index);
        if (hits_.isEmpty()) {
            return;
        }
        const std::optional<qulonglong> message = messageIndex(index.siblingAtColumn(index_column_));
        if (message && hits_.contains(*message)) {
            painter->fillRect(option.rect, color_);
        }
    }

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        return inner()->sizeHint(option, index);
    }

    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override {
        return inner()->helpEvent(event, view, option, index);
    }

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override {
        return inner()->editorEvent(event, model, option, index);
    }

 private:
    // Another tweak may delete the delegate this one wraps.
    [[nodiscard]] QAbstractItemDelegate* inner() const { return inner_ ? inner_.data() : &fallback_; }

    QPointer<QAbstractItemDelegate> inner_;
    mutable QStyledItemDelegate fallback_;
    int index_column_;
    QColor color_;
    QSet<qulonglong> hits_;
};

SearchHitsTweak::SearchHitsTweak(plugin_utils::PluginSettings settings, QWidget* main_window, QString log_table_name,
                                 QString search_table_name, int index_column)
    : settings_(std::move(settings)),
      main_window_(main_window),
      log_table_name_(std::move(log_table_name)),
      search_table_name_(std::move(search_table_name)),
      index_column_(index_column) {
    // Coalesces changes made one result at a time, as when the viewer restores
    // a search from its history.
    rebuild_timer_.setSingleShot(true);
    rebuild_timer_.setInterval(0);
    connect(&rebuild_timer_, &QTimer::timeout, this, &SearchHitsTweak::rebuildHits);
}

void SearchHitsTweak::apply() {
    if (applied_ || !main_window_) {
        return;
    }
    applied_ = true;
    // Plugins are loaded before the viewer creates the search model and dialog.
    QTimer::singleShot(0, this, &SearchHitsTweak::attach);
}

void SearchHitsTweak::attach() {
    if (!applied_ || delegate_ || !main_window_) {
        return;
    }
    auto* log_view = main_window_->findChild<QAbstractItemView*>(log_table_name_);
    const auto* search_view = main_window_->findChild<QAbstractItemView*>(search_table_name_);
    const QList<QDialog*> dialogs = main_window_->findChildren<QDialog*>();
    const auto search_dialog =
        std::ranges::find_if(dialogs, [](const QDialog* dialog) { return dialog->inherits(kSearchDialogClass); });
    if (log_view == nullptr || search_view == nullptr || search_view->model() == nullptr ||
        search_dialog == dialogs.end()) {
        qWarning() << "UI Tweaks: search tables or dialog not found";
        return;
    }

    log_view_ = log_view;
    previous_delegate_ = log_view->itemDelegate();
    // The view owns the delegate through its parent.
    auto delegate = std::make_unique<SearchHitsDelegate>(previous_delegate_, index_column_,
                                                         plugin_utils::colorValue(settings_, kColorField), log_view);
    delegate_ = delegate.get();
    log_view->setItemDelegate(delegate.release());

    search_model_ = search_view->model();
    const QAbstractItemModel* model = search_model_;
    // A search adds its results in many batches, then all of them again in
    // order; reading them as they come made searches much slower.
    const auto on_changed = [this] {
        if (!searching_) {
            rebuild_timer_.start();
        }
    };
    connections_ = {
        connect(model, &QAbstractItemModel::modelReset, this, on_changed),
        connect(model, &QAbstractItemModel::layoutChanged, this, on_changed),
        connect(model, &QAbstractItemModel::rowsRemoved, this, on_changed),
        connect(model, &QAbstractItemModel::rowsInserted, this, on_changed),
        connect(*search_dialog, SIGNAL(searchProgressChanged(bool)), this, SLOT(onSearchProgressChanged(bool))),
    };
    rebuildHits();
}

void SearchHitsTweak::revert() {
    if (!applied_) {
        return;
    }
    applied_ = false;

    for (const QMetaObject::Connection& connection : std::as_const(connections_)) {
        disconnect(connection);
    }
    connections_.clear();
    rebuild_timer_.stop();
    searching_ = false;

    if (log_view_ && log_view_->itemDelegate() == delegate_) {
        if (previous_delegate_) {
            log_view_->setItemDelegate(previous_delegate_);
        } else {
            log_view_->setItemDelegate(std::make_unique<QStyledItemDelegate>(log_view_).release());
        }
        log_view_->viewport()->update();
    }
    if (delegate_) {
        delegate_->deleteLater();
    }
    delegate_ = nullptr;
}

void SearchHitsTweak::buildSettings(plugin_utils::SettingsForm& form) {
    form.addColorPicker(kColorField, [this](const QColor& color) {
        if (delegate_ && log_view_) {
            delegate_->setColor(color);
            log_view_->viewport()->update();
        }
    });
}

void SearchHitsTweak::onSearchProgressChanged(bool in_progress) {
    searching_ = in_progress;
    if (!in_progress) {
        rebuildHits();
    }
}

void SearchHitsTweak::rebuildHits() {
    rebuild_timer_.stop();
    if (!delegate_ || !search_model_ || !log_view_) {
        return;
    }
    QSet<qulonglong> hits;
    const int rows = search_model_->rowCount();
    hits.reserve(rows);
    for (int row = 0; row < rows; ++row) {
        if (const std::optional<qulonglong> message = messageIndex(search_model_->index(row, index_column_))) {
            hits.insert(*message);
        }
    }
    delegate_->setHits(std::move(hits));
    log_view_->viewport()->update();
}
