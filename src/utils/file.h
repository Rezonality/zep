#pragma once

#include <experimental/filesystem>
#include <functional>

namespace fs = std::experimental::filesystem::v1;
namespace Zep
{
std::string file_read(const fs::path& fileName);
bool file_write(const fs::path& fileName, const void* pData, size_t size);
fs::path file_get_relative_path(fs::path from, fs::path to);
fs::path file_get_documents_path();
std::vector<fs::path> file_gather_files(const fs::path& root);

// File watcher
using fileCB = std::function<void(const fs::path&)>;
void file_init_dir_watch(const fs::path& dir, fileCB callback);
void file_destroy_dir_watch();
void file_update_dir_watch();

}

