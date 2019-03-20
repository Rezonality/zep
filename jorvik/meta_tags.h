#pragma once
#include <string>
#include <memory>

namespace Mgfx
{

struct TagValue
{
    std::string value;
    int32_t line = -1;
};
    
struct MetaTags
{
    TagValue shader_type;
    TagValue entry_point;
};

std::shared_ptr<MetaTags> parse_meta_tags(const std::string& text);

};
