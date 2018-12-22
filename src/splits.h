#pragma once

#include "common_namespace.h"
#include "mcommon/math/math.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>
#include <ostream>

namespace Zep
{
using namespace COMMON_NAMESPACE;

namespace RegionFlags
{
    enum Flags
    {
        FixedX = (1 << 0),
        FixedY = (1 << 1),
        ExpandingX = (1 << 2),
        ExpandingY = (1 << 3),
        Expanding = ExpandingX | ExpandingY,
        Fixed = FixedX | FixedY
    };
};

struct Region
{
    uint32_t flags = RegionFlags::Expanding;
    float ratio = 1.0f;
    NRectf rect;
    NVec2f min_size = NVec2f(0.0f, 0.0f);

    virtual NVec2f GetMinSize() const
    {
        return min_size;
    }
    virtual uint32_t GetFlags() const
    {
        return flags;
    }

    std::vector<std::shared_ptr<Region>> children;
    bool vertical = true;
};

inline std::ostream& operator << (std::ostream& str, const Region& region)
{
    str << region.rect;
    return str;
}


} // namespace Zep
