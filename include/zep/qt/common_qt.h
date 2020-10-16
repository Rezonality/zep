#pragma once

#include <QApplication>
#include <QDesktopWidget>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QScreen>
#include <QStyle>

namespace DPI
{
enum class PixelSize
{
    ToolBarIcon,
};

inline int GetPixelSize(PixelSize sz)
{
    switch (sz)
    {
        case PixelSize::ToolBarIcon:
            return qApp->style()->pixelMetric(QStyle::PM_ButtonIconSize);
        default:
            return 0;
    }
}

inline int ScalePixels(int pixels)
{
// Qt/MacOS handle scaling and we shouldn't interfere with that, and we don't want to do it ourselves
#ifdef __APPLE__
    return pixels;
#else
    return pixels * qRound(float(qApp->primaryScreen()->logicalDotsPerInch() / 96.0));
#endif
}

inline QSize ScalePixels(const QSize& size)
{
    const int w = ScalePixels(size.width());
    const int h = ScalePixels(size.height());
    return QSize(w, h);
}

inline QSize ScalePixels(int w, int h)
{
    return ScalePixels(QSize(w, h));
}

} // namespace DPI
