#include <cassert>

#ifdef WIN32
#include <ShellScalingApi.h>

#include "winuser.h"

#pragma comment(lib, "shcore.lib")
#endif

#include "dpi.h"
#include "zep/mcommon/logger.h"

namespace Zep
{

Dpi dpi;

void check_dpi()
{
#ifdef WIN32
    PROCESS_DPI_AWARENESS awareness;
    HRESULT hr = GetProcessDpiAwareness(nullptr, &awareness);
    if (SUCCEEDED(hr))
    {
        switch (awareness)
        {
        default:
        case PROCESS_DPI_UNAWARE:
            ZLOG(DBG, "Not DPI Aware");
            assert(!"Unexpected!");
            break;
        case PROCESS_SYSTEM_DPI_AWARE:
            ZLOG(DBG, "DPI Aware");
            break;
        case PROCESS_PER_MONITOR_DPI_AWARE:
            ZLOG(DBG, "Per Monitor Aware");
            break;
        }
    }
    auto d = GetSystemDpiForProcess(nullptr);
    dpi.scaleFactor = d / 96.0f;
    dpi.scaleFactorXY = NVec2f(dpi.scaleFactor);
#endif
}

void set_dpi(const NVec2f& value)
{
    dpi.scaleFactorXY = value;
    dpi.scaleFactor = value.x;
}

float dpi_pixel_height_from_point_size(float pointSize, float pixelScaleY)
{
    const auto fontDotsPerInch = 72.0f;
    auto inches = pointSize / fontDotsPerInch;
    return inches * (pixelScaleY * 96.0f);
}

} // namespace Zep
