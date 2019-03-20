#pragma once

#include "mcommon/math/math.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>
#include <ostream>

namespace Zep
{

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
    const char* pszName = nullptr;
    uint32_t flags = RegionFlags::Expanding;
    float ratio = 1.0f;
    NRectf rect;
    NVec2f min_size = NVec2f(0.0f, 0.0f);
    NVec2f fixed_size = NVec2f(0.0f, 0.0f);
    bool vertical = true;
    NVec2f margin = NVec2f(0.0f, 0.0f);

    std::shared_ptr<Region> pParent;
    std::vector<std::shared_ptr<Region>> children;
};

inline std::ostream& operator<<(std::ostream& str, const Region& region)
{
    static int indent = 0;
    auto do_indent = [&str](int sz) { for (int i = 0; i < sz; i++) str << " "; };

    do_indent(indent);
    if (region.pszName)
        str << region.pszName << " ";
    str << std::hex << &region << " -> ";

    str << "RC: " << region.rect << ", pParent: " << std::hex << region.pParent;
    if (!region.children.empty())
    {
        str << std::endl;
        for (auto& child : region.children)
        {
            indent++;
            str << *child;
            indent--;
        }
    }
    str << std::endl;

    return str;
}

void LayoutRegion(Region& region);


} // namespace Zep
