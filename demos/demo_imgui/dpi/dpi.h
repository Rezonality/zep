#pragma once

#include <zep/mcommon/math/math.h>

namespace Zep
{

struct Dpi
{
    float scaleFactor = 1.0;
    NVec2f scaleFactorXY = NVec2f(1.0f);
};
extern Dpi dpi;

void check_dpi();
void set_dpi(const NVec2f& val);
float dpi_pixel_height_from_point_size(float pointSize, float pixelScaleY);

#define MDPI_VEC2(value) (value * dpi.scaleFactorXY)
#define MDPI_Y(value) (value * dpi.scaleFactorXY.y)
#define MDPI_X(value) (value * dpi.scaleFactorXY.x)
#define MDPI_RECT(value) (value * dpi.scaleFactorXY)

} // namespace Zep