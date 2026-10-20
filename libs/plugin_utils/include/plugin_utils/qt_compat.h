#pragma once

#include <QColor>
#include <QString>  // IWYU pragma: export
#include <QtGlobal>

/*
 * What the plugins use from Qt 6.4 and later, for Qt 5 and older Qt 6. Drop
 * it together with support for those.
 */

#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
#include <cstddef>

using QLatin1StringView = QLatin1String;

// Where Qt 6.4 puts them, so `using namespace Qt::StringLiterals` works.
namespace Qt {
inline namespace Literals {
inline namespace StringLiterals {

constexpr QLatin1String operator""_L1(const char* str, std::size_t size) {
    return QLatin1String(str, static_cast<int>(size));
}

inline QString operator""_s(const char16_t* str, std::size_t size) {
    return QString::fromUtf16(str, static_cast<int>(size));
}

}  // namespace StringLiterals
}  // namespace Literals
}  // namespace Qt
#endif

namespace plugin_utils {

inline QColor colorFromString(const QString& name) {
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
    return QColor(name);
#else
    return QColor::fromString(name);
#endif
}

}  // namespace plugin_utils
