#pragma once

#include <QString>
#include <QSize>

namespace DPI
{
enum class PixelSize
{
    ToolBarIcon,
};
int GetPixelSize(PixelSize pm);

int ScalePixels(int pixels);
QSize ScalePixels(int w, int h);
QSize ScalePixels(const QSize& size);
float GetFontPointSize();
float GetPointSizeFromFontPixelSize(int pixelSize);
} // namespace DPI
