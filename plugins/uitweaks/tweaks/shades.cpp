#include "tweaks/shades.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace {

using Vector = std::array<double, 3>;
using Matrix = std::array<Vector, 3>;

// From https://bottosson.github.io/posts/oklab/
constexpr Matrix kLinearRgbToLms{
    Vector{0.4122214708, 0.5363325363, 0.0514459929},
    Vector{0.2119034982, 0.6806995451, 0.1073969566},
    Vector{0.0883024619, 0.2817188376, 0.6299787005},
};
constexpr Matrix kLmsToLab{
    Vector{0.2104542553, 0.7936177850, -0.0040720468},
    Vector{1.9779984951, -2.4285922050, 0.4505937099},
    Vector{0.0259040371, 0.7827717662, -0.8086757660},
};
constexpr Matrix kLabToLms{
    Vector{1.0, 0.3963377774, 0.2158037573},
    Vector{1.0, -0.1055613458, -0.0638541728},
    Vector{1.0, -0.0894841775, -1.2914855480},
};
constexpr Matrix kLmsToLinearRgb{
    Vector{4.0767416621, -3.3077115913, 0.2309699292},
    Vector{-1.2684380046, 2.6097574011, -0.3413193965},
    Vector{-0.0045196850, -0.7034186147, 1.7076147010},
};

// sRGB transfer function.
constexpr double kLinearLimit = 0.04045;
constexpr double kEncodedLimit = 0.0031308;
constexpr double kLinearSlope = 12.92;
constexpr double kOffset = 0.055;
constexpr double kGamma = 2.4;

// Keeps the darkest and lightest shades from turning black or white.
constexpr double kMinLightness = 0.2;
constexpr double kMaxLightness = 0.97;
constexpr int kChromaSearchSteps = 16;

struct Lch {
    double lightness;
    double chroma;
    double hue;
};

Vector multiply(const Matrix& matrix, const Vector& vector) {
    Vector result{};
    for (std::size_t row = 0; row < matrix.size(); ++row) {
        for (std::size_t column = 0; column < vector.size(); ++column) {
            result.at(row) += matrix.at(row).at(column) * vector.at(column);
        }
    }
    return result;
}

double toLinear(double channel) {
    return channel <= kLinearLimit ? channel / kLinearSlope : std::pow((channel + kOffset) / (1 + kOffset), kGamma);
}

double toEncoded(double channel) {
    return channel <= kEncodedLimit ? channel * kLinearSlope
                                    : ((1 + kOffset) * std::pow(channel, 1 / kGamma)) - kOffset;
}

Lch toLch(const QColor& color) {
    Vector lms = multiply(kLinearRgbToLms, {toLinear(color.redF()), toLinear(color.greenF()), toLinear(color.blueF())});
    for (double& value : lms) {
        value = std::cbrt(value);
    }
    const auto [lightness, green_red, blue_yellow] = multiply(kLmsToLab, lms);
    return {
        .lightness = lightness,
        .chroma = std::hypot(green_red, blue_yellow),
        .hue = std::atan2(blue_yellow, green_red),
    };
}

// Linear RGB, possibly outside 0 to 1 for colors sRGB cannot show.
Vector toLinearRgb(const Lch& lch) {
    Vector lms = multiply(kLabToLms, {lch.lightness, lch.chroma * std::cos(lch.hue), lch.chroma * std::sin(lch.hue)});
    for (double& value : lms) {
        value = value * value * value;
    }
    return multiply(kLmsToLinearRgb, lms);
}

std::optional<QColor> toColor(const Lch& lch) {
    const Vector rgb = toLinearRgb(lch);
    if (std::ranges::any_of(rgb, [](double channel) { return channel < 0 || channel > 1; })) {
        return std::nullopt;
    }
    const auto [red, green, blue] = rgb;
    return QColor::fromRgbF(static_cast<float>(toEncoded(red)), static_cast<float>(toEncoded(green)),
                            static_cast<float>(toEncoded(blue)));
}

// Lowers the chroma, keeping lightness and hue, until sRGB can show it.
QColor toColorInGamut(Lch lch) {
    if (const std::optional<QColor> color = toColor(lch)) {
        return *color;
    }
    double low = 0;
    double high = lch.chroma;
    for (int i = 0; i < kChromaSearchSteps; ++i) {
        lch.chroma = (low + high) / 2;
        (toColor(lch) ? low : high) = lch.chroma;
    }
    lch.chroma = low;
    return toColor(lch).value_or(Qt::gray);
}

}  // namespace

QList<QColor> shadesOf(const QColor& base, const ShadeOptions& options) {
    const Lch base_lch = toLch(base);
    const int lighter = options.count / 2;
    const int darker = options.count - 1 - lighter;
    // Where the steps do not fit, the shades on that side are spread over the
    // room there is, so that light or dark bases get as many shades.
    const double lighter_step =
        lighter > 0 ? std::min(options.step, (kMaxLightness - base_lch.lightness) / lighter) : 0;
    const double darker_step = darker > 0 ? std::min(options.step, (base_lch.lightness - kMinLightness) / darker) : 0;
    QList<QColor> shades;
    for (int i = 0; i < options.count; ++i) {
        const int offset = lighter - i;
        Lch lch = base_lch;
        lch.lightness += offset * (offset > 0 ? lighter_step : darker_step);
        // Fades toward white instead of turning into bright pastels.
        if (offset > 0) {
            lch.chroma *= (1 - lch.lightness) / (1 - base_lch.lightness);
        }
        const QColor shade = offset == 0 ? base : toColorInGamut(lch);
        if (shades.isEmpty() || shades.last().rgb() != shade.rgb()) {
            shades.append(shade);
        }
    }
    return shades;
}
