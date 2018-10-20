#pragma once

#if TARGET_MAC == 1
namespace fs
{

// NOTE:
// This is a very simple implementation of the <filesystem> functionality in CPP 14/17.
// It is not meant to be production code, but is enough to get this code working on a Mac, on which I can't get a 
// working version of <filesystem>.
// I will remove this when I figure it out, or the compiler catches up ;)
// I'll also try to make this code more tested/functional until that time

class filesystem_error : public std::system_error
{

};
typedef std::chrono::system_clock::time_point file_time_type;
class path
{
public:
    typedef std::vector<std::string>::const_iterator const_iterator;
    path(const std::string& strPath = std::string())
        : m_strPath(strPath)
    {

    }

    path(const char* pszPath)
        : m_strPath(pszPath)
    {

    }

    bool empty() const
    {
        return m_strPath.empty();
    }

    path stem() const
    {
        std::string name, ext;
        split(name, ext);
        return name;
    }

    path filename() const
    {
        std::string name, ext;
        split(name, ext);
        return path(name + ext);
    }

    bool is_relative() const
    {
	return !is_absolute();
    }
    
    bool is_absolute() const
    {
        return false;
    }

    path extension() const
    {
        std::string name, ext;
        split(name, ext);
        return ext;
    }

    path parent_path() const
    {
        std::string strSplit;
        size_t sep = m_strPath.find_last_of("\\/");
        if (sep != std::string::npos)
        {
            return m_strPath.substr(0, sep);
        }
        return path("");
    }


    path& replace_extension(const std::string& extension)
    {
        size_t dot = m_strPath.find_last_of(".");
        if (dot != std::string::npos)
        {
            m_strPath = m_strPath.substr(0, dot - 1) + extension;
        }
        else
        {
            m_strPath += extension;
        }
        return *this;
    }

    void split(std::string& name, std::string& extension) const
    {
        std::string strSplit;
        size_t sep = m_strPath.find_last_of("\\/");
        if (sep != std::string::npos)
        {
            strSplit = m_strPath.substr(sep + 1, m_strPath.size() - sep - 1);
        }

        size_t dot = strSplit.find_last_of(".");
        if (dot != std::string::npos)
        {
            name = strSplit.substr(0, dot);
            extension = strSplit.substr(dot, strSplit.size() - dot);
        }
        else
        {
            name = strSplit;
            extension = "";
        }
    }

    const char* c_str() const { return m_strPath.c_str(); }
    std::string string() const { return m_strPath; }

    bool operator == (const path& rhs) const { return m_strPath == rhs.string(); }

    path operator / (const path& rhs) const
    {
        std::string temp = m_strPath;
        StringUtils::RTrim(temp, "\\/");
        return path(temp + "/" + rhs.string());
    }

    operator std::string() const { return m_strPath; }

    bool operator < (const path& rhs) const
    {
        return m_strPath < rhs.string();
    }

    std::vector<std::string>::const_iterator begin()
    {
        std::string can = StringUtils::ReplaceString(m_strPath, "\\", "/");
        m_components = StringUtils::Split(can, "/");
        return m_components.begin();
    }
    
    std::vector<std::string>::const_iterator end()
    {
        return m_components.end();
    }
private:
    std::vector<std::string> m_components;
    std::string m_strPath;
};

inline bool exists(const path& path)
{
    struct stat buffer;
    if (path.string().empty())
    {
        return false;
    }
    return stat(path.c_str(), &buffer) == 0;
}

inline path canonical(const path& input)
{
    return path(StringUtils::ReplaceString(input.string(), "\\", "/"));
}

inline path absolute(const path& input)
{
    // Read the comments at the top of this file; this is certainly incorrect, and doesn't handle ../
    // It is sufficient for what we need though
    auto p = canonical(input);
    auto strAbs = StringUtils::ReplaceString(p.string(), "/.", "");
    return path(strAbs);
}

inline std::ostream& operator << (std::ostream& rhs, const path& p)
{
    rhs << p.string();
    return rhs;
}

namespace copy_options
{
enum
{
    overwrite_existing = 1
};
};

inline bool copy_file(const path& source, const path& dest, uint32_t options)
{
    std::ifstream  src(source.string(), std::ios::binary);
    std::ofstream  dst(dest.string(), std::ios::binary);
    if (!src.is_open() || !dst.is_open())
    {
        return false;
    }
    dst << src.rdbuf();
    return true;
}

inline bool isDirExist(const std::string& path)
{
#if defined(_WIN32)
    struct _stat info;
    if (_stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & _S_IFDIR) != 0;
#else 
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
#endif
}

inline bool makePath(const std::string& path)
{
#if defined(_WIN32)
    int ret = _mkdir(path.c_str());
#else
    mode_t mode = 0755;
    int ret = mkdir(path.c_str(), mode);
#endif
    if (ret == 0)
        return true;

    switch (errno)
    {
    case ENOENT:
        // parent didn't exist, try to create it
        {
            auto pos = path.find_last_of('/');
            if (pos == std::string::npos)
#if defined(_WIN32)
                pos = path.find_last_of('\\');
            if (pos == std::string::npos)
#endif
                return false;
            if (!makePath( path.substr(0, pos) ))
                return false;
        }
        // now, try to create again
#if defined(_WIN32)
        return 0 == _mkdir(path.c_str());
#else 
        return 0 == mkdir(path.c_str(), mode);
#endif

    case EEXIST:
        // done!
        return isDirExist(path);

    default:
        return false;
    }
}
inline bool create_directories(const path& source)
{
    return makePath(source.string());
}

inline file_time_type last_write_time(const path& source)
{
    struct stat attr;
    std::string strSource = source.string();
    stat(strSource.c_str(), &attr);
    return std::chrono::system_clock::from_time_t(attr.st_mtime);
}

inline bool is_directory(const path& source)
{
    struct stat s;
    auto strPath = source.string();
    if (stat(strPath.c_str(), &s) == 0)
    {
        if (s.st_mode & S_IFDIR)
        {
            //it's a directory
            return true;
        }
    }
    return false;
}

#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem::v1;
#endif

#include <functional>

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

