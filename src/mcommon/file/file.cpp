#include <fstream>

// For logging events to file
#ifdef ZEP_FEATURE_FILE_WATCHER
#include "mcommon/FileWatcher/FileWatcher.h"
#endif

#include "mcommon/file/file.h"
#include "mcommon/file/tinydir.h"

#include <queue>
#include <set>
#include "mcommon/logger.h"
#include "mcommon/string/stringutils.h"

#undef ERROR

namespace Zep
{

#ifdef ZEP_FEATURE_FILE_WATCHER
// Listen for run_tree updates
class UpdateListener : public FW::FileWatchListener
{
public:
    UpdateListener(fileCB cb)
        : callback(cb)
    {
    }

    void handleFileAction(FW::WatchID watchid, const FW::String& dir, const FW::String& filename, FW::Action action)
    {
        if (action == FW::Action::Modified)
        {
            callback(fs::path(filename));
        }
    }

    fileCB callback;
};

std::shared_ptr<UpdateListener> runTreeListener;
FW::WatchID watchID;
FW::FileWatcher fileWatcher;

void file_init_dir_watch(const fs::path& dir, fileCB callback)
{
    runTreeListener = std::make_shared<UpdateListener>(callback);
    watchID = fileWatcher.addWatch(dir.string().c_str(), runTreeListener.get(), true);
}

void file_destroy_dir_watch()
{
    fileWatcher.removeWatch(watchID);
    runTreeListener.reset();
}

void file_update_dir_watch()
{
    fileWatcher.update();
}
#else
void file_init_dir_watch(const fs::path& dir, fileCB callback)
{
    dir;
    callback;
}

void file_destroy_dir_watch()
{
}

void file_update_dir_watch()
{
}
#endif

std::string file_read(const fs::path& fileName)
{
    std::ifstream in(fileName, std::ios::in | std::ios::binary);
    if (in)
    {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(size_t(in.tellg()));
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
        return (contents);
    }
    else
    {
        LOG(typelog::ERROR) << "File Not Found: " << fileName.string();
    }
    return std::string();
}

bool file_write(const fs::path& fileName, const void* pData, size_t size)
{
    FILE* pFile;
    pFile = fopen(fileName.string().c_str(), "wb");
    if (!pFile)
    {
        return false;
    }
    fwrite(pData, sizeof(uint8_t), size, pFile);
    fclose(pFile);
    return true;
}

// http://stackoverflow.com/a/29221546/18942
fs::path file_get_relative_path(fs::path from, fs::path to)
{
    // Start at the root path and while they are the same then do nothing then when they first
    // diverge take the remainder of the two path and replace the entire from path with ".."
    // segments.
    fs::path::const_iterator fromIter = from.begin();
    fs::path::const_iterator toIter = to.begin();

    // Loop through both
    while (fromIter != from.end() && toIter != to.end() && (*toIter) == (*fromIter))
    {
        ++toIter;
        ++fromIter;
    }

    fs::path finalPath;
    while (fromIter != from.end())
    {
        finalPath = finalPath / "..";
        ++fromIter;
    }

    while (toIter != to.end())
    {
        finalPath = finalPath / *toIter;
        ++toIter;
    }

    return finalPath;
}

std::vector<fs::path> file_gather_files(const fs::path& root)
{
    std::vector<fs::path> ret;

    tinydir_dir dir;
    if (tinydir_open(&dir, (const _tinydir_char_t *)root.string().c_str()) == -1)
    {
        LOG(typelog::ERROR) << "Gather Files, Start Path Invalid: " << root.string();
        return ret;
    }

    std::set<fs::path> checkedPaths;
    std::queue<tinydir_dir> dirs;
    dirs.push(dir);
    while (!dirs.empty())
    {
        tinydir_dir thisDir = dirs.front();
        dirs.pop();

        while (thisDir.has_next)
        {
            tinydir_file file;
            if (tinydir_readfile(&thisDir, &file) == -1)
            {
                LOG(typelog::ERROR) << "Couldn't read: " << thisDir.path;
                tinydir_next(&thisDir);
                continue;
            }

            try
            {
                fs::path filePath(file.path);

                // Ignore . and ..
                // Otherwise we walk forever.  Do this before absolute path!
                if (filePath.string().find("\\.") != std::string::npos || filePath.string().find("..") != std::string::npos)
                {
                    //LOG(INFO) << "Skipping: " << filePath.string();
                    tinydir_next(&thisDir);
                    continue;
                }

                // Keep paths nice and absolute/canonical
                filePath = fs::canonical(filePath);
                if (checkedPaths.find(filePath) != checkedPaths.end())
                {
                    LOG(INFO) << "Already checked: " << filePath.string();
                    tinydir_next(&thisDir);
                    continue;
                }
                checkedPaths.insert(filePath);

                if (fs::is_directory(filePath))
                {
                    tinydir_dir subDir;
                    if (tinydir_open(&subDir, (const _tinydir_char_t *)filePath.string().c_str()) != -1)
                    {
                        fs::path newPath(subDir.path);
                        newPath = fs::canonical(newPath);
                        dirs.push(subDir);
                    }
                }
                else
                {
                    ret.push_back(filePath);
                }
            }
            catch (fs::filesystem_error& err)
            {
                LOG(typelog::ERROR) << err.what();
            }

            tinydir_next(&thisDir);
        }
    }
    return ret;
}

} // namespace Zep
