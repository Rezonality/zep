#pragma once

#include "zep_config.h"

#include <memory>
#include <set>
#include <string>
#include "mcommon/file/path.h"
#include <functional>

#ifdef ZEP_FEATURE_FILE_WATCHER
#include "mcommon/FileWatcher/FileWatcher.h"
#endif

namespace Zep
{

// Filesystem is used 
using fileCB = std::function<void(const ZepPath&)>;

struct IDirWatch
{
};

// Zep's view of the outside world in terms of files
// Below there is a version of this that will work on most platforms using std's <filesystem> for file operations
// If you want to expose your app's view of the world, you need to implement this minimal set of functions
class IZepFileSystem
{
public:
    virtual ~IZepFileSystem() {};
    virtual std::string Read(const ZepPath& filePath) = 0;
    virtual bool Write(const ZepPath& filePath, const void* pData, size_t size) = 0;

    // The working directory is set at start of day to the app's working parameter directory
    virtual const ZepPath& GetWorkingDirectory() const = 0;
    virtual void SetWorkingDirectory(const ZepPath& path) = 0;

    virtual bool IsDirectory(const ZepPath& path) const = 0;
    virtual bool Exists(const ZepPath& path) const = 0;

    // To enable searching for files, zep will query this single API.  It should return a list of files from the root folder
    // This enables a virtual file system representing, say a set of files in a project.  The searcher here would only return files relevent to the task
    // It is also a work item for zep to support CTRL+P functionality for quick find/launch of files
    virtual std::vector<ZepPath> FuzzySearch(const ZepPath& root, const std::string& strFuzzySearch = "", bool recursive = true) = 0;

    // Equivalent means 'the same file'
    virtual bool Equivalent(const ZepPath& path1, const ZepPath& path2) const = 0;
    virtual ZepPath Canonical(const ZepPath& path) const = 0;
   
    // Directory watcher, for file change notifications.  It's OK to return an empty structure and ignore these requests if not avaiable.
    // If you do, Zep won't know files have changed outside itself.
    virtual std::shared_ptr<IDirWatch> InitDirWatch(const ZepPath& dir, fileCB callback) = 0;
    virtual void DestroyDirWatch(std::shared_ptr<IDirWatch> handle) = 0;
    virtual void Update() = 0;
};

// CPP File system - part of the standard C++ libraries
#if defined(ZEP_FEATURE_CPP_FILE_SYSTEM)

// TODO: Fix file watcher on apple
#if defined(ZEP_FEATURE_FILE_WATCHER)

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
        (void)dir;
        (void)watchid;

        if (action == FW::Action::Modified)
        {
            callback(ZepPath(filename));
        }
    }

    fileCB callback;
};

struct WatchInfo : public IDirWatch
{
    std::shared_ptr<UpdateListener> runTreeListener;
    unsigned long watchID;
};
#endif // Filewatcher can be disabled

// A generic file system using cross platform fs:: and tinydir for searches
// This is typically the only one that is used for normal desktop usage.
// But you could make your own if your files were stored in a compressed folder, or the target system didn't have a traditional file system...
class ZepFileSystemCPP : public IZepFileSystem
{
public:
    ~ZepFileSystemCPP();
    ZepFileSystemCPP();
    virtual std::string Read(const ZepPath& filePath) override;
    virtual bool Write(const ZepPath& filePath, const void* pData, size_t size) override;
    virtual std::vector<ZepPath> FuzzySearch(const ZepPath& root, const std::string& strFuzzySearch = "", bool recursive = true) override;
    virtual std::shared_ptr<IDirWatch> InitDirWatch(const ZepPath& dir, fileCB callback) override;
    virtual void DestroyDirWatch(std::shared_ptr<IDirWatch> handle) override;
    virtual void Update() override;
    virtual void SetWorkingDirectory(const ZepPath& path) override;
    virtual const ZepPath& GetWorkingDirectory() const override;
    virtual bool IsDirectory(const ZepPath& path) const override;
    virtual bool Exists(const ZepPath& path) const override;
    virtual bool Equivalent(const ZepPath& path1, const ZepPath& path2) const override;
    virtual ZepPath Canonical(const ZepPath& path) const override;

private:
    ZepPath m_workingDirectory;
#if defined(ZEP_FEATURE_FILE_WATCHER)
    FW::FileWatcher m_fileWatcher;
    std::set<std::shared_ptr<WatchInfo>> m_watchers;
#endif
};
#endif // CPP File system

} // namespace Zep
