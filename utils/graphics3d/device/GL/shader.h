#pragma once

#include "file/file.h"
namespace Mgfx
{

// A simple shader loading/linking function
uint32_t LoadShaders(const fs::path& vertex_file_path, const fs::path& fragment_file_path);

} // namespace Mgfx

