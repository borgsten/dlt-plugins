#include "tweaks/search_hits_tweak.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include <QCheckBox>
#include <QDateTimeEdit>
#include <QDebug>
#include <QDialog>
#include <QEvent>
#include <QHash>
#include <QHelpEvent>
#include <QIcon>
#include <QLineEdit>
#include <QLocale>
#include <QModelIndex>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QRadioButton>
#include <QRectF>
#include <QRegularExpression>
#include <QSize>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolBar>
#include <QVariant>
#include <QtLogging>
#include <qdlt/dltmessagematcher.h>
#include <qdlt/qdltfile.h>
#include <qdlt/qdltmsg.h>
#include <qdlt/qdltsettingsmanager.h>

using namespace Qt::StringLiterals;

struct SearchMatcher {
    DltMessageMatcher matcher;
    DltMessageMatcher::Pattern pattern;
};

namespace {

const plugin_utils::ColorField kColorField{
    .key = u"color"_s,
    .opacity_key = u"opacity"_s,
    .label = u"Color"_s,
    .default_value = u"#ffb300"_s,
    .default_opacity = 30,
};

constexpr auto kSearchDialogClass = "CSearchDialog";
const auto kSearchToolbarName = u"searchToolbar"_s;
// Rows whose match is remembered; a screenful is far fewer.
constexpr qsizetype kMaxCachedMatches = 4096;
// The viewer rejects longer IDs.
constexpr int kMaxIdLength = 4;

// What CSearchDialog::find() and startParallelFindAll() give the matcher.
std::optional<SearchMatcher> readSearch(const QObject& dialog) {
    const auto* text = dialog.findChild<QLineEdit*>(u"lineEditSearch"_s);
    const auto* regexp = dialog.findChild<QCheckBox*>(u"checkBoxRegExp"_s);
    const auto* case_sensitive = dialog.findChild<QCheckBox*>(u"checkBoxCaseSensitive"_s);
    const auto* header = dialog.findChild<QCheckBox*>(u"checkBoxHeader"_s);
    const auto* payload = dialog.findChild<QCheckBox*>(u"checkBoxPayload"_s);
    const auto* app_id = dialog.findChild<QLineEdit*>(u"lineEditApld"_s);
    const auto* ctx_id = dialog.findChild<QLineEdit*>(u"lineEditCtid"_s);
    const auto* timestamp_range = dialog.findChild<QRadioButton*>(u"radioTimestamp"_s);
    const auto* timestamp_start = dialog.findChild<QLineEdit*>(u"lineEditTimestampStart"_s);
    const auto* timestamp_end = dialog.findChild<QLineEdit*>(u"lineEditTimestampEnd"_s);
    const auto* time_range = dialog.findChild<QRadioButton*>(u"radioTime"_s);
    const auto* time_start = dialog.findChild<QDateTimeEdit*>(u"dateTimeStart"_s);
    const auto* time_end = dialog.findChild<QDateTimeEdit*>(u"dateTimeEnd"_s);
    if (text == nullptr || regexp == nullptr || case_sensitive == nullptr || header == nullptr || payload == nullptr ||
        app_id == nullptr || ctx_id == nullptr || timestamp_range == nullptr || timestamp_start == nullptr ||
        timestamp_end == nullptr || time_range == nullptr || time_start == nullptr || time_end == nullptr) {
        qWarning() << "UI Tweaks: search dialog widgets not found, not marking new matches";
        return std::nullopt;
    }
    if (app_id->text().size() > kMaxIdLength || ctx_id->text().size() > kMaxIdLength) {
        return std::nullopt;
    }

    SearchMatcher search;
    DltMessageMatcher& matcher = search.matcher;
    matcher.setCaseSentivity(case_sensitive->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive);
    matcher.setSearchAppId(app_id->text());
    matcher.setSearchCtxId(ctx_id->text());
    matcher.setHeaderSearchEnabled(header->isChecked());
    matcher.setPayloadSearchEnabled(payload->isChecked());
    if (timestamp_range->isChecked() && !timestamp_start->text().isEmpty() && !timestamp_end->text().isEmpty()) {
        const double start = timestamp_start->text().toDouble();
        const double end = timestamp_end->text().toDouble();
        if (end >= start) {
            matcher.setTimestampRange(start, end);
        }
    }
    if (time_range->isChecked()) {
        matcher.setTimeRange(time_start->dateTime(), time_end->dateTime());
    }
    const QDltSettingsManager* viewer_settings = QDltSettingsManager::getInstance();
    if (viewer_settings->value(u"startup/showMsgId"_s, true).toBool()) {
        matcher.setMessageIdFormat(viewer_settings->value(u"startup/msgIdFormat"_s, u"0x%x"_s).toString());
    }

    if (regexp->isChecked()) {
        QRegularExpression::PatternOptions options = QRegularExpression::DotMatchesEverythingOption;
        if (!case_sensitive->isChecked()) {
            options |= QRegularExpression::CaseInsensitiveOption;
        }
        QRegularExpression pattern(text->text(), options);
        if (!pattern.isValid()) {
            return std::nullopt;
        }
        search.pattern = std::move(pattern);
    } else {
        search.pattern = text->text();
    }
    return search;
}

// The mark color, crossed out; colored, unlike the theme's clear icons, so
// that it looks different while disabled.
QIcon clearMarksIcon(const QColor& color) {
    constexpr int kSize = 32;
    constexpr int kInset = 3;
    constexpr int kRadius = 5;
    constexpr int kCrossInset = 9;
    constexpr qreal kCrossWidth = 3.5;
    constexpr int kBorderAlpha = 90;
    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(0, 0, 0, kBorderAlpha), 1));
    painter.setBrush(QColor(color.rgb()));
    painter.drawRoundedRect(QRectF(kInset, kInset, kSize - (2 * kInset), kSize - (2 * kInset)), kRadius, kRadius);
    painter.setPen(QPen(Qt::black, kCrossWidth, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(kCrossInset, kCrossInset, kSize - kCrossInset, kSize - kCrossInset);
    painter.drawLine(kCrossInset, kSize - kCrossInset, kSize - kCrossInset, kCrossInset);
    painter.end();
    return {pixmap};
}

// Both tables show the index with group separators ("%L1").
std::optional<qulonglong> messageIndex(const QModelIndex& index) {
    bool parsed = false;
    const qulonglong value = QLocale().toULongLong(index.data().toString(), &parsed);
    return parsed ? std::optional(value) : std::nullopt;
}

}  // namespace

/* Paints like the delegate it replaces, then tints the rows the search matches */
class SearchHitsDelegate : public QAbstractItemDelegate {
 public:
    SearchHitsDelegate(QAbstractItemDelegate* inner, int index_column, QColor color, QObject* parent)
        : QAbstractItemDelegate(parent), inner_(inner), index_column_(index_column), color_(color) {}

    void setColor(QColor color) { color_ = color; }

    void setSearch(std::optional<SearchMatcher> search) {
        search_ = std::move(search);
        matches_.clear();
    }

    void setFile(QDltFile* file) {
        file_ = file;
        matches_.clear();
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        inner()->paint(painter, option, index);
        if (!search_ || file_ == nullptr) {
            return;
        }
        const std::optional<qulonglong> message = messageIndex(index.siblingAtColumn(index_column_));
        if (message && matches(*search_, *message)) {
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
    // Each cell of a row asks; the message is read and matched once.
    [[nodiscard]] bool matches(const SearchMatcher& search, qulonglong index) const {
        if (const auto cached = matches_.constFind(index); cached != matches_.cend()) {
            return *cached;
        }
        QDltMsg msg;
        // Not getMsg(), whose cache the viewer tunes for its own reads.
        bool match = file_->getMsgNoCache(static_cast<int>(index), msg);
        if (match) {
            msg.setIndex(static_cast<int>(index));
            match = search.matcher.match(msg, search.pattern);
        }
        if (matches_.size() >= kMaxCachedMatches) {
            matches_.clear();
        }
        matches_.insert(index, match);
        return match;
    }

    // Another tweak may delete the delegate this one wraps.
    [[nodiscard]] QAbstractItemDelegate* inner() const { return inner_ ? inner_.data() : &fallback_; }

    QPointer<QAbstractItemDelegate> inner_;
    mutable QStyledItemDelegate fallback_;
    int index_column_;
    QColor color_;
    std::optional<SearchMatcher> search_;
    // The viewer's, which outlives the views.
    QDltFile* file_ = nullptr;
    mutable QHash<qulonglong, bool> matches_;
};

SearchHitsTweak::SearchHitsTweak(plugin_utils::PluginSettings settings, QWidget* main_window, QString log_table_name,
                                 QString search_table_name, int index_column)
    : settings_(std::move(settings)),
      main_window_(main_window),
      log_table_name_(std::move(log_table_name)),
      search_table_name_(std::move(search_table_name)),
      index_column_(index_column) {
    clear_action_.setText(u"Clear search marks"_s);
    clear_action_.setIcon(clearMarksIcon(plugin_utils::colorValue(settings_, kColorField)));
    clear_action_.setEnabled(false);
    connect(&clear_action_, &QAction::triggered, this, [this] { setSearch(std::nullopt); });
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
    delegate_->setFile(file_);
    log_view->setItemDelegate(delegate.release());

    search_dialog_ = *search_dialog;
    const QAbstractItemModel* model = search_view->model();
    // Results changed outside a search, as when a file is opened or filters
    // are applied: what they are is unknown, so are the matches.
    const auto on_changed = [this] {
        if (!searching_) {
            setSearch(std::nullopt);
        }
    };
    connections_ = {
        connect(model, &QAbstractItemModel::modelReset, this, on_changed),
        connect(model, &QAbstractItemModel::layoutChanged, this, on_changed),
        connect(model, &QAbstractItemModel::rowsRemoved, this, on_changed),
        connect(model, &QAbstractItemModel::rowsInserted, this, on_changed),
        connect(*search_dialog, SIGNAL(searchProgressChanged(bool)), this, SLOT(onSearchProgressChanged(bool))),
    };

    // At the end: the viewer finds the toolbar's own actions by position.
    toolbar_ = main_window_->findChild<QToolBar*>(kSearchToolbarName);
    if (toolbar_) {
        toolbar_->addAction(&clear_action_);
    } else {
        qWarning() << "UI Tweaks: toolbar not found:" << kSearchToolbarName;
    }
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
    searching_ = false;
    if (toolbar_) {
        toolbar_->removeAction(&clear_action_);
    }
    clear_action_.setEnabled(false);

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
        clear_action_.setIcon(clearMarksIcon(color));
        if (delegate_ && log_view_) {
            delegate_->setColor(color);
            log_view_->viewport()->update();
        }
    });
}

void SearchHitsTweak::setFile(QDltFile* file) {
    file_ = file;
    if (delegate_) {
        delegate_->setFile(file);
    }
}

void SearchHitsTweak::onSearchProgressChanged(bool in_progress) {
    searching_ = in_progress;
    if (!in_progress || !search_dialog_) {
        return;
    }
    // Only Find All fills the results; a search the viewer rejects leaves them.
    const auto* find_all = search_dialog_->findChild<QCheckBox*>(u"checkBoxFindAll"_s);
    if (find_all != nullptr && find_all->isChecked()) {
        if (std::optional<SearchMatcher> search = readSearch(*search_dialog_)) {
            setSearch(std::move(search));
        }
    }
}

void SearchHitsTweak::setSearch(std::optional<SearchMatcher> search) {
    clear_action_.setEnabled(search.has_value());
    if (delegate_ && log_view_) {
        delegate_->setSearch(std::move(search));
        log_view_->viewport()->update();
    }
}
