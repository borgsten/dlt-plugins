#pragma once

#include <QCheckBox>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <qdlt/qdltfile.h>

/*
 * The viewer's filter index range (in the Filter tab, hidden by UI Tweaks),
 * set and applied as if by hand. Global message indices, both inclusive.
 * dlt-viewer 2.31 ignores it without an active positive or negative filter,
 * and applies it to live messages only when re-filtering.
 */
class IndexRangeFilter {
 public:
    explicit IndexRangeFilter(QWidget* main_window);

    /* An error to show, if the range could not be applied */
    [[nodiscard]] QString limit(int start, int end) const;
    void clear() const;
    /* Whether a released dlt-viewer (2.31) would apply the range to file */
    [[nodiscard]] static bool appliesUnpatched(const QDltFile& file);

 private:
    [[nodiscard]] bool isAvailable() const;
    void apply() const;

    QPointer<QCheckBox> enabled_;
    QPointer<QLineEdit> start_;
    QPointer<QLineEdit> end_;
    QPointer<QPushButton> apply_;
};
