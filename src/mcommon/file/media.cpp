#include "mcommon.h"

#include <easylogging/src/easylogging++.h>

#include "file/media.h"
#include "file/file.h"
#include "config_app.h"

#include "sdl.h"
namespace Mgfx
{
std::string g_AppFriendlyName = APPLICATION_NAME;
fs::path mediaPath;

void media_init(bool dev)
{
    fs::path basePath;

    // Choose the root of the source tree as the base path
    if (dev)
        basePath = GAME_ROOT;
    else
        basePath = SDL_GetBasePath();

    basePath = basePath / "run_tree";
    mediaPath = fs::canonical(fs::absolute(basePath));
    LOG(INFO) << "Media Path: " << mediaPath.string();
}

void media_destroy()
{
    mediaPath.clear();
}

fs::path media_find_asset_internal(fs::path& searchPath)
{
    fs::path found(mediaPath / searchPath);
    if (fs::exists(found))
    {
        //LOG(DEBUG) << "Found file: " << found.string();
        return fs::canonical(fs::absolute(found));
    }

    LOG(DEBUG) << "** File not found: " << searchPath.string();
    return fs::path();
}

fs::path media_find_asset(const fs::path& p)
{
    return media_find_asset_internal(fs::path(p));
}

std::string media_load_asset(const fs::path& p)
{
    auto path = media_find_asset(p);
    return file_read(path);
}

fs::path media_run_tree_path()
{
    return mediaPath;
}

} // Mgfx
