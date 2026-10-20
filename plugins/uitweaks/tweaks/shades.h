#pragma once

#include <QColor>
#include <QList>

struct ShadeOptions {
    int count = 1;
    // Perceived lightness (OKLab L, 0 to 1) between neighboring shades.
    double step = 0;
};

/*
 * Lighter to darker shades of a color's hue, evenly spaced in perceived
 * lightness, with the color itself among them. Shades that would be equal
 * are left out.
 */
QList<QColor> shadesOf(const QColor& base, const ShadeOptions& options);
