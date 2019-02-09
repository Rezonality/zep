#pragma once

// File watcher implementation used by the CPP file system here
#include "FileWatcher.cpp"
#if defined(_WIN32)
#include "FileWatcherWin32.cpp"
#elif defined(__APPLE__)
#include "FileWatcherOSX.cpp"
#elif defined(__linux__)
#include "FileWatcherLinux.cpp"
#endif

namespace MUtils
{

class Watcher : public FW::FileWatchListener
{
public:
    using fileCB = std::function<void(std::string&)>;

    static Watcher& Instance()
    {
        static Watcher watch;
        return watch;
    }

    void handleFileAction(FW::WatchID watchid, const FW::String& dir, const FW::String& filename, FW::Action action)
    {
        (void)dir;
        if (action == FW::Action::Modified)
        {
            auto itrFound = m_mapCallbacks.find(watchid);
            if (itrFound != m_mapCallbacks.end())
            {
                std::string str = filename.c_str();
                itrFound->second(str);
            }
        }
    }

    void AddWatch(const std::string& path, fileCB callback, bool recursive = true)
    {
        m_mapCallbacks[m_watcher.addWatch(path.c_str(), this, recursive)] = callback;
    }

    void RemoveWatch(FW::WatchID id)
    {
        m_watcher.removeWatch(id);
        m_mapCallbacks.erase(id);
    }

    ~Watcher()
    {
        while (!m_mapCallbacks.empty())
        {
            RemoveWatch(m_mapCallbacks.begin()->first);
        }
    }

    void Update()
    {
        m_watcher.update();
    }

private:
    struct WatchData
    {
        FW::WatchID id;
        fileCB callback;
    };
    std::map<FW::WatchID, fileCB> m_mapCallbacks;
    FW::FileWatcher m_watcher;
};

}; 
