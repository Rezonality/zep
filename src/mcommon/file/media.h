#pragma once

#include "file/file.h"

namespace Mgfx
{

void media_init(bool dev);
void media_destroy();
fs::path media_find_asset(const fs::path& p);
std::string media_load_asset(const fs::path& p);
fs::path media_run_tree_path();

}
