#pragma once

#include "zep/mcommon/string/stringutils.h"

#include <system_error>
#include <chrono>
#include <sstream>
#include <fstream>
#include <filesystem>

namespace Zep
{

namespace fs = std::filesystem;
fs::path path_get_relative(const fs::path& from, const fs::path&to);

} // namespace Zep
