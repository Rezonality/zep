#include "mode_search.h"
#include "tab_window.h"
#include "window.h"
#include "mcommon/logger.h"
#include "mcommon/threadutils.h"
#include "filesystem.h"

namespace Zep
{

ZepMode_Search::ZepMode_Search(ZepEditor& editor)
    : ZepMode(editor)
{
    // Pattern regex
    auto patterns = std::vector<std::string>({".git", "\\.xcodeproj",
        "build", "xcuserdata", "\\.obj", "\\.pch", "\\.lib", "\\.ttf", "\\.bmp", "\\.filters", "\\.vcproj", "\\.vcxproj"});
    for (auto& pattern : patterns)
    {
        m_ignorePatterns.push_back(std::regex(pattern, std::regex_constants::optimize));
    }
}

ZepMode_Search::~ZepMode_Search()
{
    // Ensure threads have finished
    if (m_indexResult.valid())
    {
        m_indexResult.wait();
    }

    if (m_searchResult.valid())
    {
        m_searchResult.wait();
    }
}

void ZepMode_Search::AddKeyPress(uint32_t key, uint32_t modifiers)
{
    (void)modifiers;
    if (key == ExtKeys::ESCAPE)
    {
        GetEditor().GetActiveTabWindow()->RemoveWindow(m_pSearchWindow);
        GetEditor().GetActiveTabWindow()->SetActiveWindow(m_pLaunchWindow);
        GetEditor().RemoveBuffer(m_pSearchBuffer);
        m_pSearchBuffer = nullptr;
        GetEditor().EndSecondaryMode();
        return;
    }
    else if (key == ExtKeys::RETURN)
    {
        OpenSelection(OpenType::Replace);
        return;
    }
    else if (key == ExtKeys::BACKSPACE)
    {
        if (m_searchTerm.length() > 0)
        {
            m_searchTerm = m_searchTerm.substr(0, m_searchTerm.length() - 1);
            UpdateTree();
        }
    }
    else
    {
        if (modifiers & ModifierKey::Ctrl)
        {
            if (key == 'j')
            {
                m_pSearchWindow->MoveCursorY(1);
            }
            else if (key == 'k')
            {
                m_pSearchWindow->MoveCursorY(-1);
            }
            else if (key == 'v')
            {
                OpenSelection(OpenType::VSplit);
                return;
            }
            else if (key == 'x')
            {
                OpenSelection(OpenType::HSplit);
                return;
            }
            else if (key == 't')
            {
                OpenSelection(OpenType::Tab);
                return;
            }
        }
        else if (key > 0 && key < 127)
        {
            m_searchTerm += char(key);
            UpdateTree();
        }
    }

    std::ostringstream str;
    str << ">>> " << m_searchTerm;

    if (!m_indexTree.empty())
    {
        str << " (" << m_indexTree[m_indexTree.size() - 1]->indices.size() << " / " << m_indexTree[0]->indices.size() << ")";
    }

    GetEditor().SetCommandText(str.str());
}

void ZepMode_Search::Begin()
{
    m_searchTerm = "";
    GetEditor().SetCommandText(">>> ");

    auto findStartPath = [&](const ZepPath& startPath)
    {
        if (!startPath.empty())
        {
            auto testPath = startPath;
            if (!GetEditor().GetFileSystem().IsDirectory(testPath))
            {
                testPath = testPath.parent_path();
            }

            while (!testPath.empty() && GetEditor().GetFileSystem().IsDirectory(testPath))
            {
                bool foundDir = false;

                // Look in this dir
                GetEditor().GetFileSystem().ScanDirectory(testPath, [&](const ZepPath& p, bool& recurse)
                    -> bool
                {
                    // Not looking at sub folders
                    recurse = false;

                    // Found the .git repo
                    if (p.extension() == ".git" && GetEditor().GetFileSystem().IsDirectory(p))
                    {
                        foundDir = true;
                    
                        // Quit search
                        return false;
                    }
                    return true;
                });

                // If found,  return it as the path we need
                if (foundDir)
                {
                    return testPath;
                }

                testPath = testPath.parent_path();
            }
        }
        return startPath;
    };

    m_pLaunchWindow = GetEditor().GetActiveTabWindow()->GetActiveWindow();

    ZepPath startPath = m_pLaunchWindow->GetBuffer().GetFilePath();
    ZepPath workingDir = GetEditor().GetFileSystem().GetWorkingDirectory();

    auto found = findStartPath(startPath);
    if (found.empty())
    {
        found = findStartPath(workingDir);
    }

    if (!found.empty())
    {
        startPath = found;
    }
    else
    {
        startPath = workingDir;
    }

    m_pSearchBuffer = GetEditor().GetEmptyBuffer("Search", FileFlags::Locked | FileFlags::ReadOnly);
    m_pSearchBuffer->SetBufferType(BufferType::Search);
    m_pSearchBuffer->SetText(std::string("Indexing: ") + startPath.string());

    m_pSearchWindow = GetEditor().GetActiveTabWindow()->AddWindow(m_pSearchBuffer, nullptr, false);
    m_pSearchWindow->SetCursorType(CursorType::LineMarker);

    LOG(INFO) << "StartPath: " << startPath.string();

    fileSearchActive = true;
    m_indexResult = GetEditor().GetThreadPool().enqueue([&](const ZepPath& root)
    {
        auto spResult = std::make_shared<FileSearchResult>();
        spResult->root = ZepPath(root.string());

        try
        {
            // Index the whole subtree, ignoring any patterns supplied to us
            GetEditor().GetFileSystem().ScanDirectory(root, [&](const ZepPath& p, bool& recurse)
                -> bool
            {
                recurse = true;
                auto str = p.string();
                auto stem = p.stem();
                auto ext = p.extension();

                if (ext == ".git")
                {
                    recurse = false;
                    return true;
                }

                // Hack to make debug fast - ignore regex expressions!
#ifdef _DEBUG
                if (ext == ".xcodeproj" ||
                    ext == ".filters" ||
                    ext == ".vcxproj" ||
                    ext == ".vcproj" ||
                    ext == ".pdb" ||
                    stem == "build" ||
                    stem == "Debug" ||
                    stem == "Release")
                {
                    recurse = false;
                    return true;
                }

#else
                std::smatch results;
                for (auto& r : m_ignorePatterns)
                {
                    if (std::regex_search(str, results, r))
                    {
                        recurse = false;
                        return true;
                    }
                }
#endif

                // Not adding directories to the search list
                if (GetEditor().GetFileSystem().IsDirectory(p))
                {
                    return true;
                }

                // Add this one to our list
                auto targetZep = GetEditor().GetFileSystem().Canonical(p);
                auto rel = path_get_relative(root, targetZep);

                spResult->paths.push_back(rel);
                spResult->lowerPaths.push_back(string_tolower(rel.string()));

                return true;
            });
        }
        catch (std::exception&)
        {
        }
        return spResult;
    }, startPath);
}

void ZepMode_Search::Notify(std::shared_ptr<ZepMessage> message)
{
    ZepMode::Notify(message);
    if (message->messageId == Msg::Tick)
    {
        if (fileSearchActive)
        {
            if (!is_future_ready(m_indexResult))
            {
                return;
            }
            fileSearchActive = false;

            m_spFilePaths = m_indexResult.get();

            InitSearchTree();
            ShowTreeResult();
            UpdateTree();

            GetEditor().RequestRefresh();
        }

        if (treeSearchActive)
        {
            UpdateTree();
        }
    }
}

void ZepMode_Search::InitSearchTree()
{
    m_indexTree.clear();
    auto pInitSet = std::make_shared<IndexSet>();
    for (uint32_t i = 0; i < (uint32_t)m_spFilePaths->paths.size(); i++)
    {
        pInitSet->indices.insert(std::make_pair(0, SearchResult{i, 0}));
    }
    m_indexTree.push_back(pInitSet);
}

void ZepMode_Search::ShowTreeResult()
{
    std::ostringstream str;
    bool start = true;
    for (auto& index : m_indexTree.back()->indices)
    {
        if (!start)
        {
            str << std::endl;
        }
        str << m_spFilePaths->paths[index.second.index].string();
        start = false;
    }
    m_pSearchBuffer->SetText(str.str());
    m_pSearchWindow->SetBufferCursor(0);
}

void ZepMode_Search::OpenSelection(OpenType type)
{
    if (m_indexTree.empty())
        return;

    auto cursor = m_pSearchWindow->GetBufferCursor();
    auto line = m_pSearchBuffer->GetBufferLine(cursor);
    auto paths = m_indexTree[m_indexTree.size() - 1];

    GetEditor().GetActiveTabWindow()->RemoveWindow(m_pSearchWindow);
    GetEditor().GetActiveTabWindow()->SetActiveWindow(m_pLaunchWindow);
    GetEditor().RemoveBuffer(m_pSearchBuffer);
    m_pSearchBuffer = nullptr;

    BufferLocation count = 0;
    for (auto& index : m_indexTree.back()->indices)
    {
        if (count == line)
        {
            auto path = m_spFilePaths->paths[index.second.index];
            auto full_path = m_spFilePaths->root / path;
            auto pBuffer = GetEditor().GetFileBuffer(full_path, 0, true);
            if (pBuffer != nullptr)
            {
                switch (type)
                {
                case OpenType::Replace:
                    m_pLaunchWindow->SetBuffer(pBuffer);
                    break;
                case OpenType::VSplit:
                    GetEditor().GetActiveTabWindow()->AddWindow(pBuffer, m_pLaunchWindow, true);
                    break;
                case OpenType::HSplit:
                    GetEditor().GetActiveTabWindow()->AddWindow(pBuffer, m_pLaunchWindow, false);
                    break;
                case OpenType::Tab:
                    GetEditor().AddTabWindow()->AddWindow(pBuffer, nullptr, false);
                    break;
                }
            }
        }
        count++;
    }
    GetEditor().EndSecondaryMode();
}

void ZepMode_Search::UpdateTree()
{
    if (fileSearchActive)
    {
        return;
    }

    if (treeSearchActive)
    {
        if (!is_future_ready(m_searchResult))
        {
            return;
        }

        m_indexTree.push_back(m_searchResult.get());
        treeSearchActive = false;
    }

    // If the user is typing capitals, he cares about them in the search!
    m_caseImportant = string_tolower(m_searchTerm) != m_searchTerm;

    assert(!m_indexTree.empty());

    uint32_t treeDepth = uint32_t(m_indexTree.size() - 1);
    if (m_searchTerm.size() < treeDepth)
    {
        while (m_searchTerm.size() < treeDepth)
        {
            m_indexTree.pop_back();
            treeDepth--;
        };
    }
    else if (m_searchTerm.size() >  treeDepth)
    {
        std::shared_ptr<IndexSet> spStartSet;
        spStartSet = m_indexTree[m_indexTree.size() - 1];
        char startChar = m_searchTerm[m_indexTree.size() - 1];

        // Search for a match at the next level of the search tree
        m_searchResult = GetEditor().GetThreadPool().enqueue([&](std::shared_ptr<IndexSet> spStartSet, const char startChar)
        {
            auto spResult = std::make_shared<IndexSet>();
            for (auto& searchPair : spStartSet->indices)
            {
                auto index = searchPair.second.index;
                auto loc = searchPair.second.location;
                auto dist = searchPair.first;

                size_t pos = 0;
                if (m_caseImportant)
                {
                    auto str = m_spFilePaths->paths[index].string();
                    pos = str.find_first_of(startChar, loc);
                }
                else
                {
                    auto str = m_spFilePaths->lowerPaths[index];
                    pos = str.find_first_of(startChar, loc);
                }

                if (pos != std::string::npos)
                {
                    // this approach 'clumps things together'
                    // It rewards more for strings of subsequent characters
                    uint32_t newDist = ((uint32_t)pos - loc);
                    if (dist == 0)
                    {
                        newDist = 1;
                    }
                    else if (newDist == 1)
                    {
                        newDist = dist;
                    }
                    else
                    {
                        newDist = dist + 1;
                    }

                    spResult->indices.insert(std::make_pair(newDist, SearchResult{index, (uint32_t)pos}));
                }
            }
            return spResult;
        }, spStartSet, startChar);

        treeSearchActive = true;
    }

    ShowTreeResult();
    GetEditor().RequestRefresh();
}

} // namespace Zep
