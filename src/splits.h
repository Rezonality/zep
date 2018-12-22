#pragma once

#include <algorithm>
#include <limits>
#include <vector>
#include <memory>
#include "common_namespace.h"
#include "mcommon/math/math.h"

namespace Zep
{
using namespace COMMON_NAMESPACE;

struct Region
{
    NRectf region;
    NVec2f min_size = NVec2f(0.0f, 0.0f);
    NVec2f max_size = NVec2f(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());

    virtual NVec2f GetMinSize() { return min_size; }
    virtual NVec2f GetMaxSize() { return max_size; }

    std::vector<std::shared_ptr<Region>> children;
    bool vertical = true;
};

} // namespace Zep
