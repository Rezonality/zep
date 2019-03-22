#include "utils.h"
#include "utils/logger.h"

#include "file/runtree.h"
#include "file/file.h"
#include "config_app.h"

#include "SDL.h"
namespace Mgfx
{
std::string g_AppFriendlyName = APPLICATION_NAME;
fs::path runtreePath;

void runtree_init(bool dev)
{
    fs::path basePath;

    // Choose the root of the source tree as the base path
    if (dev)
        basePath = JORVIK_ROOT;
    else
        basePath = SDL_GetBasePath();

    basePath = basePath / "run_tree";
    runtreePath = fs::canonical(fs::absolute(basePath));
    LOG(DEBUG) << "runtree Path: " << runtreePath.string();
}

void runtree_destroy()
{
    runtreePath.clear();
}

fs::path runtree_find_asset_internal(const fs::path& searchPath)
{
    fs::path found(runtreePath / searchPath);
    if (fs::exists(found))
    {
        //LOG(DEBUG) << "Found file: " << found.string();
        return fs::canonical(fs::absolute(found));
    }

    LOG(DEBUG) << "** File not found: " << searchPath.string();
    return fs::path();
}

fs::path runtree_find_asset(const fs::path& p)
{
    return runtree_find_asset_internal(p);
}

std::string runtree_load_asset(const fs::path& p)
{
    auto path = runtree_find_asset(p);
    return file_read(path);
}

fs::path runtree_path()
{
    return runtreePath;
}

} // Mgfx
