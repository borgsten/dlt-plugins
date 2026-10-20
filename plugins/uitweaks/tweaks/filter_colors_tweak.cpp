#include "tweaks/filter_colors_tweak.h"

#include <algorithm>
#include <array>
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
#include <QList>
#include <QPalette>
#include <QPushButton>
#include <QSet>
#include <QSize>
#include <QString>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "plugin_utils/qt_compat.h"

#include "tweaks/shades.h"

using namespace Qt::StringLiterals;

namespace {

constexpr auto kFilterDialogClass = "FilterDialog";
constexpr auto kExtendedProperty = "uitweaksFilterColors";
const auto kColorPreviewName = u"labelSelectedColor"_s;

const auto kShadeCountKey = u"shadeCount"_s;
constexpr int kDefaultShadeCount = 7;
constexpr int kMaxShadeCount = 15;
const auto kStepKey = u"lightnessStep"_s;
// About the spacing of Tailwind's palettes.
constexpr int kDefaultStep = 7;
constexpr int kMaxStep = 20;
constexpr double kPercent = 100;

struct HueFamily {
    QLatin1StringView name;
    // Tailwind's 400, bright enough for dark text.
    QLatin1StringView base;
};

constexpr std::array kFamilies{
    HueFamily{.name = "Red"_L1, .base = "#f87171"_L1},     HueFamily{.name = "Orange"_L1, .base = "#fb923c"_L1},
    HueFamily{.name = "Amber"_L1, .base = "#fbbf24"_L1},   HueFamily{.name = "Yellow"_L1, .base = "#facc15"_L1},
    HueFamily{.name = "Lime"_L1, .base = "#a3e635"_L1},    HueFamily{.name = "Green"_L1, .base = "#4ade80"_L1},
    HueFamily{.name = "Emerald"_L1, .base = "#34d399"_L1}, HueFamily{.name = "Teal"_L1, .base = "#2dd4bf"_L1},
    HueFamily{.name = "Cyan"_L1, .base = "#22d3ee"_L1},    HueFamily{.name = "Sky"_L1, .base = "#38bdf8"_L1},
    HueFamily{.name = "Blue"_L1, .base = "#60a5fa"_L1},    HueFamily{.name = "Indigo"_L1, .base = "#818cf8"_L1},
    HueFamily{.name = "Violet"_L1, .base = "#a78bfa"_L1},  HueFamily{.name = "Purple"_L1, .base = "#c084fc"_L1},
    HueFamily{.name = "Fuchsia"_L1, .base = "#e879f9"_L1}, HueFamily{.name = "Pink"_L1, .base = "#f472b6"_L1},
    HueFamily{.name = "Rose"_L1, .base = "#fb7185"_L1},    HueFamily{.name = "Slate"_L1, .base = "#94a3b8"_L1},
};

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
    ShadesChooser(const QWidget* dialog, QSet<QRgb> used, const ShadeOptions& options)
        : dialog_(dialog), used_(std::move(used)) {
        layout_.setContentsMargins(0, 0, 0, 0);
        layout_.setSpacing(kSwatchSpacing);
        auto bases = std::make_unique<QHBoxLayout>();
        bases->setSpacing(kSwatchSpacing);
        for (const HueFamily& hue : kFamilies) {
            const QColor base(hue.base);
            families_.append({.name = QString(hue.name), .base = base, .shades = shadesOf(base, options)});
        }
        // families_ does not change from here on, so the entries stay put.
        for (const Family& family : std::as_const(families_)) {
            const bool family_used =
                std::ranges::any_of(family.shades, [this](const QColor& shade) { return used_.contains(shade.rgb()); });
            auto swatch = createSwatch(dialog, family.base, family.name, family_used);
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
    struct Family {
        QString name;
        QColor base;
        QList<QColor> shades;
    };

    [[nodiscard]] QColor currentColor() const {
        const auto* preview = dialog_->findChild<QWidget*>(kColorPreviewName);
        return preview != nullptr ? preview->palette().color(QPalette::Active, preview->backgroundRole()) : QColor();
    }

    [[nodiscard]] const Family& familyOf(const QColor& color) const {
        const auto family = std::ranges::find_if(
            families_, [&color](const Family& candidate) { return candidate.shades.contains(color); });
        return family != families_.end() ? *family : families_.front();
    }

    void showShades(const Family& family) {
        for (QPushButton* swatch : shades_row_.findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly)) {
            swatch->hide();
            swatch->deleteLater();
        }
        for (int shade = 0; shade < family.shades.size(); ++shade) {
            const QColor& color = family.shades.at(shade);
            const QString name = u"%1 %2"_s.arg(family.name).arg(shade + 1);
            // Before the trailing stretch.
            shades_layout_.insertWidget(shade,
                                        createSwatch(dialog_, color, name, used_.contains(color.rgb())).release());
        }
    }

    const QWidget* dialog_;
    QSet<QRgb> used_;
    QList<Family> families_;
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

FilterColorsTweak::FilterColorsTweak(plugin_utils::PluginSettings settings, QWidget* main_window)
    : settings_(std::move(settings)), main_window_(main_window) {}

void FilterColorsTweak::apply() {
    if (applied_) {
        return;
    }
    applied_ = true;
    // The viewer creates a new filter dialog for every edit; catch it, and
    // its color preview's palette changes, at the application.
    QApplication::instance()->installEventFilter(this);
}

void FilterColorsTweak::buildSettings(plugin_utils::SettingsForm& form) {
    // Read when a filter dialog opens.
    form.addSpinBox({
        .key = kShadeCountKey,
        .label = "Shades",
        .default_value = kDefaultShadeCount,
        .minimum = 1,
        .maximum = kMaxShadeCount,
    });
    form.addSpinBox({
        .key = kStepKey,
        .label = "Lightness step (%)",
        .default_value = kDefaultStep,
        .minimum = 1,
        .maximum = kMaxStep,
    });
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
    chooser_layout->addWidget(
        std::make_unique<ShadesChooser>(dialog, usedColors(main_window_),
                                        ShadeOptions{
                                            .count = settings_.value(kShadeCountKey, kDefaultShadeCount),
                                            .step = settings_.value(kStepKey, kDefaultStep) / kPercent,
                                        })
            .release());
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
