#include "tweaks/filter_colors_tweak.h"

#include <array>
#include <cstddef>
#include <memory>
#include <utility>

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QDebug>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPalette>
#include <QPushButton>
#include <QSet>
#include <QSize>
#include <QString>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "plugin_utils/qt_compat.h"

using namespace Qt::StringLiterals;

namespace {

constexpr auto kFilterDialogClass = "FilterDialog";
constexpr auto kExtendedProperty = "uitweaksFilterColors";
const auto kColorPreviewName = u"labelSelectedColor"_s;

constexpr std::size_t kShadeCount = 7;

struct HueFamily {
    QLatin1StringView name;
    // Tailwind's 100 to 700; up to 500 bright enough for dark text.
    std::array<QLatin1StringView, kShadeCount> shades;
};

constexpr std::array kFamilies{
    HueFamily{
        .name = "Red"_L1,
        .shades = {"#fee2e2"_L1, "#fecaca"_L1, "#fca5a5"_L1, "#f87171"_L1, "#ef4444"_L1, "#dc2626"_L1, "#b91c1c"_L1},
    },
    HueFamily{
        .name = "Orange"_L1,
        .shades = {"#ffedd5"_L1, "#fed7aa"_L1, "#fdba74"_L1, "#fb923c"_L1, "#f97316"_L1, "#ea580c"_L1, "#c2410c"_L1},
    },
    HueFamily{
        .name = "Amber"_L1,
        .shades = {"#fef3c7"_L1, "#fde68a"_L1, "#fcd34d"_L1, "#fbbf24"_L1, "#f59e0b"_L1, "#d97706"_L1, "#b45309"_L1},
    },
    HueFamily{
        .name = "Yellow"_L1,
        .shades = {"#fef9c3"_L1, "#fef08a"_L1, "#fde047"_L1, "#facc15"_L1, "#eab308"_L1, "#ca8a04"_L1, "#a16207"_L1},
    },
    HueFamily{
        .name = "Lime"_L1,
        .shades = {"#ecfccb"_L1, "#d9f99d"_L1, "#bef264"_L1, "#a3e635"_L1, "#84cc16"_L1, "#65a30d"_L1, "#4d7c0f"_L1},
    },
    HueFamily{
        .name = "Green"_L1,
        .shades = {"#dcfce7"_L1, "#bbf7d0"_L1, "#86efac"_L1, "#4ade80"_L1, "#22c55e"_L1, "#16a34a"_L1, "#15803d"_L1},
    },
    HueFamily{
        .name = "Emerald"_L1,
        .shades = {"#d1fae5"_L1, "#a7f3d0"_L1, "#6ee7b7"_L1, "#34d399"_L1, "#10b981"_L1, "#059669"_L1, "#047857"_L1},
    },
    HueFamily{
        .name = "Teal"_L1,
        .shades = {"#ccfbf1"_L1, "#99f6e4"_L1, "#5eead4"_L1, "#2dd4bf"_L1, "#14b8a6"_L1, "#0d9488"_L1, "#0f766e"_L1},
    },
    HueFamily{
        .name = "Cyan"_L1,
        .shades = {"#cffafe"_L1, "#a5f3fc"_L1, "#67e8f9"_L1, "#22d3ee"_L1, "#06b6d4"_L1, "#0891b2"_L1, "#0e7490"_L1},
    },
    HueFamily{
        .name = "Sky"_L1,
        .shades = {"#e0f2fe"_L1, "#bae6fd"_L1, "#7dd3fc"_L1, "#38bdf8"_L1, "#0ea5e9"_L1, "#0284c7"_L1, "#0369a1"_L1},
    },
    HueFamily{
        .name = "Blue"_L1,
        .shades = {"#dbeafe"_L1, "#bfdbfe"_L1, "#93c5fd"_L1, "#60a5fa"_L1, "#3b82f6"_L1, "#2563eb"_L1, "#1d4ed8"_L1},
    },
    HueFamily{
        .name = "Indigo"_L1,
        .shades = {"#e0e7ff"_L1, "#c7d2fe"_L1, "#a5b4fc"_L1, "#818cf8"_L1, "#6366f1"_L1, "#4f46e5"_L1, "#4338ca"_L1},
    },
    HueFamily{
        .name = "Violet"_L1,
        .shades = {"#ede9fe"_L1, "#ddd6fe"_L1, "#c4b5fd"_L1, "#a78bfa"_L1, "#8b5cf6"_L1, "#7c3aed"_L1, "#6d28d9"_L1},
    },
    HueFamily{
        .name = "Purple"_L1,
        .shades = {"#f3e8ff"_L1, "#e9d5ff"_L1, "#d8b4fe"_L1, "#c084fc"_L1, "#a855f7"_L1, "#9333ea"_L1, "#7e22ce"_L1},
    },
    HueFamily{
        .name = "Fuchsia"_L1,
        .shades = {"#fae8ff"_L1, "#f5d0fe"_L1, "#f0abfc"_L1, "#e879f9"_L1, "#d946ef"_L1, "#c026d3"_L1, "#a21caf"_L1},
    },
    HueFamily{
        .name = "Pink"_L1,
        .shades = {"#fce7f3"_L1, "#fbcfe8"_L1, "#f9a8d4"_L1, "#f472b6"_L1, "#ec4899"_L1, "#db2777"_L1, "#be185d"_L1},
    },
    HueFamily{
        .name = "Rose"_L1,
        .shades = {"#ffe4e6"_L1, "#fecdd3"_L1, "#fda4af"_L1, "#fb7185"_L1, "#f43f5e"_L1, "#e11d48"_L1, "#be123c"_L1},
    },
    HueFamily{
        .name = "Slate"_L1,
        .shades = {"#f1f5f9"_L1, "#e2e8f0"_L1, "#cbd5e1"_L1, "#94a3b8"_L1, "#64748b"_L1, "#475569"_L1, "#334155"_L1},
    },
};

// The shade shown for each family in the base row.
constexpr std::size_t kBaseShade = 3;

constexpr QSize kSwatchSize(20, 18);
constexpr int kSwatchSpacing = 3;
constexpr int kDisabledAlpha = 90;
constexpr float kMaxAlpha = 255;
constexpr int kSectionSpacing = 8;

// pushButton_c0 to pushButton_c10
constexpr int kPresetCount = 11;

QString presetName(int index) { return u"pushButton_c%1"_s.arg(index); }

// Disabled colors let the window show through, dimming them on light and dark themes.
QColor dimmed(QColor color) {
    color.setAlpha(kDisabledAlpha);
    return color;
}

// What dimmed() looks like over the window, for colors that are not blended.
QColor dimmedOver(const QColor& color, const QColor& window) {
    const float opacity = static_cast<float>(kDisabledAlpha) / kMaxAlpha;
    const auto mix = [opacity](float channel, float window_channel) {
        return (channel * opacity) + (window_channel * (1 - opacity));
    };
    return QColor::fromRgbF(mix(color.redF(), window.redF()), mix(color.greenF(), window.greenF()),
                            mix(color.blueF(), window.blueF()));
}

QString swatchStyle(const QColor& color) {
    const QColor dim = dimmed(color);
    return u"QPushButton { background-color: %1; border: 1px solid rgba(0, 0, 0, 60); border-radius: 3px; }"
           u" QPushButton:disabled { background-color: rgba(%2, %3, %4, %5); border-color: rgba(0, 0, 0, 25); }"_s
               .arg(color.name())
               .arg(dim.red())
               .arg(dim.green())
               .arg(dim.blue())
               .arg(dim.alpha());
}

QSet<QRgb> usedColors(const QWidget* main_window) {
    QSet<QRgb> used;
    const auto* filters = main_window->findChild<QTreeWidget*>(u"filterWidget"_s);
    if (filters == nullptr) {
        return used;
    }
    // The viewer shows each filter's color as the background of column 1.
    for (int i = 0; i < filters->topLevelItemCount(); ++i) {
        const QBrush background = filters->topLevelItem(i)->background(1);
        if (background.style() != Qt::NoBrush) {
            used.insert(background.color().rgb());
        }
    }
    return used;
}

// The dialog reads the picked color from the clicked preset's style sheet
// background, so pick through a preset to keep its own handling.
void selectColor(const QWidget* dialog, const QColor& color) {
    auto* preset = dialog->findChild<QPushButton*>(presetName(0));
    if (preset == nullptr) {
        return;
    }
    const QString style = preset->styleSheet();
    preset->setStyleSheet(swatchStyle(color));
    preset->click();
    preset->setStyleSheet(style);
}

std::unique_ptr<QPushButton> createSwatch(const QWidget* dialog, const QColor& color, const QString& name, bool used) {
    auto swatch = std::make_unique<QPushButton>(used ? u"•"_s : QString());
    swatch->setFixedSize(kSwatchSize);
    swatch->setFocusPolicy(Qt::NoFocus);
    swatch->setStyleSheet(swatchStyle(color));
    swatch->setToolTip(used ? u"%1 %2, used by a filter"_s.arg(name, color.name())
                            : u"%1 %2"_s.arg(name, color.name()));
    QObject::connect(swatch.get(), &QPushButton::clicked, dialog, [dialog, color] { selectColor(dialog, color); });
    return swatch;
}

// The viewer sets the preview's inactive and disabled backgrounds to white, so
// the picked color disappears whenever the dialog loses focus, and glares
// while the marker color is off. Disabled, the preview is dimmed as a whole,
// text included, so its text does not change color when toggled.
void syncPreviewColor(QWidget* preview) {
    QPalette palette = preview->palette();
    const QPalette::ColorRole background = preview->backgroundRole();
    const QPalette::ColorRole text = preview->foregroundRole();
    const QColor color = palette.color(QPalette::Active, background);
    // Widgets fill their background without blending, so mix with the window here.
    const QColor window = preview->parentWidget()->palette().color(QPalette::Active, QPalette::Window);
    const QColor disabled_background = dimmedOver(color, window);
    const QColor disabled_text = dimmedOver(palette.color(QPalette::Active, text), window);
    if (palette.color(QPalette::Inactive, background) == color &&
        palette.color(QPalette::Disabled, background) == disabled_background &&
        palette.color(QPalette::Disabled, text) == disabled_text) {
        return;
    }
    palette.setColor(QPalette::Inactive, background, color);
    palette.setColor(QPalette::Disabled, background, disabled_background);
    palette.setColor(QPalette::Disabled, text, disabled_text);
    // Triggers another PaletteChange, which then finds the color already set.
    preview->setPalette(palette);
}

std::unique_ptr<QFrame> createSeparator() {
    auto separator = std::make_unique<QFrame>();
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    return separator;
}

// A row of base colors, and below it the shades of the last chosen family.
class ShadesChooser : public QWidget {
 public:
    ShadesChooser(const QWidget* dialog, QSet<QRgb> used) : dialog_(dialog), used_(std::move(used)) {
        layout_.setContentsMargins(0, 0, 0, 0);
        layout_.setSpacing(kSwatchSpacing);
        auto bases = std::make_unique<QHBoxLayout>();
        bases->setSpacing(kSwatchSpacing);
        for (const HueFamily& family : kFamilies) {
            bool family_used = false;
            for (const QLatin1StringView shade : family.shades) {
                family_used = family_used || used_.contains(QColor(shade).rgb());
            }
            auto swatch = createSwatch(dialog, QColor(family.shades.at(kBaseShade)), QString(family.name), family_used);
            connect(swatch.get(), &QPushButton::clicked, this, [this, &family] { showShades(family); });
            bases->addWidget(swatch.release());
        }
        bases->addStretch();
        layout_.addLayout(bases.release());

        shades_layout_.setContentsMargins(0, 0, 0, 0);
        shades_layout_.setSpacing(kSwatchSpacing);
        shades_layout_.addStretch();
        layout_.addWidget(&shades_row_);
        showShades(familyOf(currentColor()));
    }

 private:
    [[nodiscard]] QColor currentColor() const {
        const auto* preview = dialog_->findChild<QWidget*>(kColorPreviewName);
        return preview != nullptr ? preview->palette().color(QPalette::Active, preview->backgroundRole()) : QColor();
    }

    static const HueFamily& familyOf(const QColor& color) {
        for (const HueFamily& family : kFamilies) {
            for (const QLatin1StringView shade : family.shades) {
                if (QColor(shade) == color) {
                    return family;
                }
            }
        }
        return kFamilies.front();
    }

    void showShades(const HueFamily& family) {
        for (QPushButton* swatch : shades_row_.findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly)) {
            swatch->hide();
            swatch->deleteLater();
        }
        for (std::size_t shade = 0; shade < kShadeCount; ++shade) {
            const QColor color(family.shades.at(shade));
            const QString name = u"%1 %2"_s.arg(QString(family.name)).arg(shade + 1);
            // Before the trailing stretch.
            shades_layout_.insertWidget(static_cast<int>(shade),
                                        createSwatch(dialog_, color, name, used_.contains(color.rgb())).release());
        }
    }

    const QWidget* dialog_;
    QSet<QRgb> used_;
    // Destroyed before the QWidget base, which then no longer owns them.
    QVBoxLayout layout_{this};
    QWidget shades_row_;
    QHBoxLayout shades_layout_{&shades_row_};
};

// The presets, columns 1 to 11 of the group box's grid, stay hidden;
// selectColor() clicks one. The chooser takes their place.
void extendColorGrid(const QWidget& dialog, QGridLayout& colors, std::unique_ptr<QWidget> chooser) {
    for (int i = 0; i < kPresetCount; ++i) {
        if (auto* preset = dialog.findChild<QWidget*>(presetName(i))) {
            preset->hide();
        }
    }
    colors.setHorizontalSpacing(kSectionSpacing);
    colors.addWidget(chooser.release(), 0, 1);
}

// Before the viewer's layout fix (upstream, not yet released) the group box
// has a fixed size and places the color row by hand. The presets stay in the
// hidden row.
bool extendLegacyColorRow(const QWidget& dialog, QGroupBox& group, QWidget& preview, QPushButton& pick,
                          std::unique_ptr<QWidget> chooser) {
    auto* color_row = dialog.findChild<QWidget*>(u"gridLayoutWidget_2"_s);
    if (color_row == nullptr) {
        return false;
    }
    color_row->hide();
    auto row = std::make_unique<QHBoxLayout>();
    row->setSpacing(kSectionSpacing);
    row->addWidget(&preview);
    row->addWidget(chooser.release());
    row->addWidget(&pick);
    row->addStretch();
    group.setMaximumHeight(QWIDGETSIZE_MAX);
    group.setLayout(row.release());
    return true;
}

}  // namespace

FilterColorsTweak::FilterColorsTweak(QWidget* main_window) : main_window_(main_window) {}

void FilterColorsTweak::apply() {
    if (applied_) {
        return;
    }
    applied_ = true;
    // The viewer creates a new filter dialog for every edit; catch it, and
    // its color preview's palette changes, at the application.
    QApplication::instance()->installEventFilter(this);
}

void FilterColorsTweak::revert() {
    if (!applied_) {
        return;
    }
    applied_ = false;
    QApplication::instance()->removeEventFilter(this);
}

bool FilterColorsTweak::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Show && watched->inherits(kFilterDialogClass) &&
        !watched->property(kExtendedProperty).toBool()) {
        watched->setProperty(kExtendedProperty, true);
        extendDialog(qobject_cast<QWidget*>(watched));
    } else if (event->type() == QEvent::PaletteChange && watched->objectName() == kColorPreviewName) {
        auto* preview = qobject_cast<QWidget*>(watched);
        if (preview != nullptr && preview->window()->inherits(kFilterDialogClass)) {
            syncPreviewColor(preview);
        }
    }
    return false;
}

void FilterColorsTweak::extendDialog(QWidget* dialog) const {
    auto* group = dialog->findChild<QGroupBox*>(u"groupBox_marker"_s);
    auto* preview = dialog->findChild<QWidget*>(kColorPreviewName);
    auto* pick = dialog->findChild<QPushButton*>(u"buttonSelectColor"_s);
    if (group == nullptr || preview == nullptr || pick == nullptr || !main_window_) {
        qWarning() << "UI Tweaks: filter dialog color widgets not found";
        return;
    }
    syncPreviewColor(preview);

    auto chooser = std::make_unique<QWidget>();
    auto* chooser_layout = std::make_unique<QHBoxLayout>(chooser.get()).release();
    chooser_layout->setContentsMargins(0, 0, 0, 0);
    chooser_layout->setSpacing(kSectionSpacing);
    chooser_layout->addWidget(createSeparator().release());
    chooser_layout->addWidget(std::make_unique<ShadesChooser>(dialog, usedColors(main_window_)).release());
    chooser_layout->addWidget(createSeparator().release());

    // The explicit minimum size from the viewer hides the layout's, which then
    // gets squeezed.
    group->setMinimumSize(0, 0);
    if (auto* colors = qobject_cast<QGridLayout*>(group->layout())) {
        extendColorGrid(*dialog, *colors, std::move(chooser));
    } else if (!extendLegacyColorRow(*dialog, *group, *preview, *pick, std::move(chooser))) {
        qWarning() << "UI Tweaks: filter dialog color row not found";
        return;
    }
    dialog->adjustSize();
}
