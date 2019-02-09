#include "filesystem.h"

#include <fstream>

#include "mcommon/logger.h"
#include "mcommon/string/stringutils.h"

#undef ERROR

#if defined(ZEP_FEATURE_CPP_FILE_SYSTEM)

#if !defined(__APPLE__)
#include <experimental/filesystem>
namespace cpp_fs = std::experimental::filesystem::v1;
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace Zep
{

ZepFileSystemCPP::ZepFileSystemCPP()
{
#if defined(__APPLE__)
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        m_workingDirectory = ZepPath(std::string(cwd));
    }
#else
    m_workingDirectory = ZepPath(cpp_fs::current_path().string());
#endif
}

ZepFileSystemCPP::~ZepFileSystemCPP()
{
#if defined(ZEP_FEATURE_FILE_WATCHER)
    while (!m_watchers.empty())
    {
        DestroyDirWatch(std::static_pointer_cast<IDirWatch>(*m_watchers.begin()));
    }
#endif
}

void ZepFileSystemCPP::SetWorkingDirectory(const ZepPath& path)
{
    m_workingDirectory = path;
}

const ZepPath& ZepFileSystemCPP::GetWorkingDirectory() const
{
    return m_workingDirectory;
}

bool ZepFileSystemCPP::IsDirectory(const ZepPath& path) const
{
#if defined(__APPLE__)
    struct stat s;
    auto strPath = path.string();
    if (stat(strPath.c_str(), &s) == 0)
    {
        if (s.st_mode & S_IFDIR)
        {
            //it's a directory
            return true;
        }
    }
    return false;
#else
    return cpp_fs::is_directory(path.string());
#endif
}

std::string ZepFileSystemCPP::Read(const ZepPath& fileName)
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

bool ZepFileSystemCPP::Write(const ZepPath& fileName, const void* pData, size_t size)
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

std::vector<ZepPath> ZepFileSystemCPP::FuzzySearch(const ZepPath& root, const std::string& strFuzzySearch, bool recursive)
{
    (void)strFuzzySearch;

    std::vector<ZepPath> ret;

#if !defined(__APPLE__)
    LOG(INFO) << "Search: " << root.string();

    auto addFile = [&ret](const cpp_fs::path& p) {
        try
        {
            if (!cpp_fs::is_directory(p) && cpp_fs::exists(p))
            {
                auto c = cpp_fs::canonical(p);
                LOG(INFO) << c.string();
                ret.push_back(c.string());
            }
        }
        catch (cpp_fs::filesystem_error& err)
        {
            LOG(ERROR) << err.what();
        }
    };

    if (recursive)
    {
        for (auto& p : cpp_fs::recursive_directory_iterator(root.string()))
        {
            addFile(p);
        }
    }
    else
    {
        for (auto& p : cpp_fs::directory_iterator(root.string()))
        {
            addFile(p);
        }
    }
#else
    (void)recursive;
    (void)root;
#endif
    return ret;
}

bool ZepFileSystemCPP::Exists(const ZepPath& path) const
{
#if defined(__APPLE__)
    try
    {
        std::ifstream ifile(path.string());
        return (bool)ifile;
    }
    catch (std::exception&)
    {
    }
    return false;
#else
    try
    {
        return cpp_fs::exists(path.string());
    }
    catch (cpp_fs::filesystem_error& err)
    {
        throw std::runtime_error(err.what());
    }
#endif
}

bool ZepFileSystemCPP::Equivalent(const ZepPath& path1, const ZepPath& path2) const
{
#if defined(__APPLE__)
    return Canonical(path1).string() == Canonical(path2).string();
#else
    try
    {
        return cpp_fs::equivalent(path1.string(), path2.string());
    }
    catch (cpp_fs::filesystem_error& err)
    {
        throw std::runtime_error(err.what());
    }
#endif
}

ZepPath ZepFileSystemCPP::Canonical(const ZepPath& path) const
{
#if defined(__APPLE__)
    return ZepPath(string_replace(path.string(), "\\", "/"));
#else
    try
    {
        return ZepPath(cpp_fs::canonical(path.string()).string());
    }
    catch (cpp_fs::filesystem_error& err)
    {
        throw std::runtime_error(err.what());
    }
#endif
}

#if defined(ZEP_FEATURE_FILE_WATCHER)
std::shared_ptr<IDirWatch> ZepFileSystemCPP::InitDirWatch(const ZepPath& dir, fileCB callback)
{
    auto pWatch = std::make_shared<WatchInfo>();

    pWatch->runTreeListener = std::make_shared<UpdateListener>(callback);
    pWatch->watchID = m_fileWatcher.addWatch(dir.string().c_str(), pWatch->runTreeListener.get(), true);
    m_watchers.insert(pWatch);
    return pWatch;
}

void ZepFileSystemCPP::DestroyDirWatch(std::shared_ptr<IDirWatch> spWatch)
{
    auto pWatchInfo = std::static_pointer_cast<WatchInfo>(spWatch);
    m_fileWatcher.removeWatch(pWatchInfo->watchID);
    pWatchInfo->runTreeListener.reset();
    m_watchers.erase(pWatchInfo);
}

void ZepFileSystemCPP::Update()
{
    for (auto& spWatch : m_watchers)
    {
        auto pWatchInfo = std::static_pointer_cast<WatchInfo>(spWatch);
        m_fileWatcher.update();
    }
}
#else
std::shared_ptr<IDirWatch> ZepFileSystemCPP::InitDirWatch(const ZepPath& dir, fileCB callback)
{
    (void)dir;
    (void)callback;

    auto spRet = std::shared_ptr<IDirWatch>();
    return spRet;
}

void ZepFileSystemCPP::DestroyDirWatch(std::shared_ptr<IDirWatch> spWatch)
{
    (void)spWatch;
}

void ZepFileSystemCPP::Update()
{
}
#endif // Filewatcher not on mac

} // namespace Zep

#endif // CPP_FILESYSTEM
