#pragma once

namespace Mgfx
{

struct Dpi
{
    float scaleFactor = 1.0;
};
extern Dpi dpi;

void check_dpi();

} // Mgfx
