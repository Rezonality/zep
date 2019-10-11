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
    return pixels * float(qApp->desktop()->logicalDpiX() / 96.0f);
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

inline float GetFontPointSize()
{
    const QFont font = qApp->font();
    return font.pointSizeF();
}

inline float GetPointSizeFromFontPixelSize(int pixelSize)
{
    return (pixelSize * 72) / float(qApp->desktop()->logicalDpiX());
}

} // namespace DPI
