#pragma once

#include "file/file.h"

namespace Mgfx
{

void runtree_init(bool dev);
void runtree_destroy();
fs::path runtree_find_asset(const fs::path& p);
std::string runtree_load_asset(const fs::path& p);
fs::path runtree_path();

}
