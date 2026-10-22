#include "index_range_filter.h"

#include <algorithm>
#include <limits>

#include <QDebug>
#include <QObject>
#include <qdlt/qdltfilter.h>
#include <qdlt/qdltfilterlist.h>
#include <qdlt/qdltsettingsmanager.h>

#include "plugin_utils/qt_compat.h"

using namespace Qt::StringLiterals;

IndexRangeFilter::IndexRangeFilter(QWidget* main_window)
    : enabled_(main_window->findChild<QCheckBox*>(u"checkBoxFilterRange"_s)),
      start_(main_window->findChild<QLineEdit*>(u"lineEditFilterStart"_s)),
      end_(main_window->findChild<QLineEdit*>(u"lineEditFilterEnd"_s)),
      apply_(main_window->findChild<QPushButton*>(u"applyConfig"_s)) {
    if (!isAvailable()) {
        qWarning() << "Test Overview: filter index range widgets not found";
    }
}

QString IndexRangeFilter::limit(int start, int end) const {
    if (!isAvailable()) {
        return QObject::tr("The viewer's filter index range is not available.");
    }
    // The range is applied with the filters; without them the viewer ignores it.
    if (!QDltSettingsManager::getInstance()->value(u"startup/filtersEnabled"_s, true).toBool()) {
        return QObject::tr("Enable filters to limit the log to a suite or test.");
    }
    start_->setText(QString::number(start));
    // Past the last message means up to the last one, also for live messages.
    end_->setText(QString::number(end >= 0 ? end : std::numeric_limits<int>::max()));
    enabled_->setChecked(true);
    apply();
    return {};
}

void IndexRangeFilter::clear() const {
    if (!isAvailable() || !enabled_->isChecked()) {
        return;
    }
    enabled_->setChecked(false);
    apply();
}

// Until fix/filter_index_range is merged, DltFileIndexer::run() applies the
// range only along with such a filter.
bool IndexRangeFilter::appliesUnpatched(const QDltFile& file) {
    const QDltFilterList filters = file.getFilterList();
    return std::ranges::any_of(filters.filters, [](const QDltFilter* filter) {
        return filter != nullptr && filter->enableFilter && (filter->isPositive() || filter->isNegative());
    });
}

bool IndexRangeFilter::isAvailable() const { return enabled_ && start_ && end_ && apply_; }

void IndexRangeFilter::apply() const { apply_->click(); }
