#pragma once

#include "utils.h"

#include <functional>

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem::v1;

namespace Mgfx
{

std::string file_read(const fs::path& fileName);
bool file_write(const fs::path& fileName, const void* pData, size_t size);
fs::path file_get_relative_path(fs::path from, fs::path to);
std::vector<fs::path> file_gather_files(const fs::path& root);

// File watcher
using fileCB = std::function<void(const fs::path&)>;
void file_init_dir_watch(const fs::path& dir, fileCB callback);
void file_destroy_dir_watch();
void file_update_dir_watch();
fs::path file_documents_path();
fs::path file_roaming_path();
fs::path file_appdata_path();

} // namespace Mgfx
