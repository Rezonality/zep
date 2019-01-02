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
    Fixed = (1 << 0),
    Expanding = (1 << 1)
};
};

struct Region
{
    uint32_t flags = RegionFlags::Expanding;
    float ratio = 1.0f;
    NRectf rect;
    NVec2f min_size = NVec2f(0.0f, 0.0f);
    NVec2f fixed_size = NVec2f(0.0f, 0.0f);
    bool vertical = true;

    Region* pParent = nullptr;
    std::vector<std::shared_ptr<Region>> children;
};

inline std::ostream& operator<<(std::ostream& str, const Region& region)
{
    str << region.rect;
    return str;
}

void LayoutRegion(Region& region);

} // namespace Zep
