#include "zep/mode_search.h"
#include "zep/filesystem.h"
#include "zep/tab_window.h"
#include "zep/window.h"

#include "zep/mcommon/logger.h"
#include "zep/mcommon/threadutils.h"

#include "mcommon/file/fnmatch.h"

namespace Zep
{

ZepMode_Search::ZepMode_Search(ZepEditor& editor)
    : ZepMode(editor)
{
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

// TODO: Later we will have a project manager for tags, search, etc.
void ZepMode_Search::GetSearchPaths(const ZepPath& path, std::vector<std::string>& ignore_patterns, std::vector<std::string>& include_patterns) const
{
    ZepPath config = path / ".zep" / "project.cfg";

    if (GetEditor().GetFileSystem().Exists(config))
    {
        try
        {
            auto spConfig = cpptoml::parse_file(config.string());
            if (spConfig != nullptr)
            {
                ignore_patterns = spConfig->get_qualified_array_of<std::string>("search.ignore").value_or(std::vector<std::string>{});
                include_patterns = spConfig->get_qualified_array_of<std::string>("search.include").value_or(std::vector<std::string>{});
            }
        }
        catch (cpptoml::parse_exception& ex)
        {
            std::ostringstream str;
            str << config.filename().string() << " : Failed to parse. " << ex.what();
            GetEditor().SetCommandText(str.str());
        }
        catch (...)
        {
            std::ostringstream str;
            str << config.filename().string() << " : Failed to parse. ";
            GetEditor().SetCommandText(str.str());
        }
    }

    if (ignore_patterns.empty())
    {
        ignore_patterns = {
            "[Bb]uild/*",
            "**/[Oo]bj/**",
            "**/[Bb]in/**",
            "[Bb]uilt*"
        };
    }
    if (include_patterns.empty())
    {
        include_patterns = {
            "*.cpp",
            "*.c",
            "*.hpp",
            "*.h",
            "*.lsp",
            "*.scm",
            "*.cs",
            "*.cfg"
        };
    }
}

void ZepMode_Search::Begin()
{
    m_searchTerm = "";
    GetEditor().SetCommandText(">>> ");

    m_pLaunchWindow = GetEditor().GetActiveTabWindow()->GetActiveWindow();

    auto startPath = GetEditor().GetFileSystem().GetSearchRoot(m_pLaunchWindow->GetBuffer().GetFilePath());

    std::vector<std::string> ignorePaths;
    std::vector<std::string> includePaths;
    GetSearchPaths(startPath, ignorePaths, includePaths);

    m_pSearchBuffer = GetEditor().GetEmptyBuffer("Search", FileFlags::Locked | FileFlags::ReadOnly);
    m_pSearchBuffer->SetBufferType(BufferType::Search);
    m_pSearchBuffer->SetText(std::string("Indexing: ") + startPath.string());

    m_pSearchWindow = GetEditor().GetActiveTabWindow()->AddWindow(m_pSearchBuffer, nullptr, false);
    m_pSearchWindow->SetCursorType(CursorType::LineMarker);
    m_pSearchWindow->SetWindowFlags(m_pSearchWindow->GetWindowFlags() & WindowFlags::Modal);

    LOG(INFO) << "StartPath: " << startPath.string();

    fileSearchActive = true;
    m_indexResult = GetEditor().GetThreadPool().enqueue([&, ignorePaths, includePaths](const ZepPath& root) {
        auto spResult = std::make_shared<FileSearchResult>();
        spResult->root = ZepPath(root.string());

        try
        {
            // Index the whole subtree, ignoring any patterns supplied to us
            GetEditor().GetFileSystem().ScanDirectory(root, [&](const ZepPath& p, bool& recurse) -> bool {
                recurse = true;

                auto bDir = GetEditor().GetFileSystem().IsDirectory(p);

                // Add this one to our list
                auto targetZep = GetEditor().GetFileSystem().Canonical(p);
                auto rel = path_get_relative(root, targetZep);

                bool matched = true;
                for (auto& proj : ignorePaths)
                {
                    auto res = fnmatch(proj.c_str(), rel.string().c_str(), 0);
                    if (res == 0)
                    {
                        matched = false;
                        break;
                    }
                }

                if (!matched)
                {
                    if (bDir)
                    {
                        recurse = false;
                    }
                    return true;
                }

                matched = false;
                for (auto& proj : includePaths)
                {
                    auto res = fnmatch(proj.c_str(), rel.string().c_str(), 0);
                    if (res == 0)
                    {
                        matched = true;
                        break;
                    }
                }

                if (!matched)
                {
                    return true;
                }

                // Not adding directories to the search list
                if (bDir)
                {
                    return true;
                }

                spResult->paths.push_back(rel);
                spResult->lowerPaths.push_back(string_tolower(rel.string()));

                return true;
            });
        }
        catch (std::exception&)
        {
        }
        return spResult;
    },
        startPath);
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
        pInitSet->indices.insert(std::make_pair(0, SearchResult{ i, 0 }));
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
    else if (m_searchTerm.size() > treeDepth)
    {
        std::shared_ptr<IndexSet> spStartSet;
        spStartSet = m_indexTree[m_indexTree.size() - 1];
        char startChar = m_searchTerm[m_indexTree.size() - 1];

        // Search for a match at the next level of the search tree
        m_searchResult = GetEditor().GetThreadPool().enqueue([&](std::shared_ptr<IndexSet> spStartSet, const char startChar) {
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

                    spResult->indices.insert(std::make_pair(newDist, SearchResult{ index, (uint32_t)pos }));
                }
            }
            return spResult;
        },
            spStartSet, startChar);

        treeSearchActive = true;
    }

    ShowTreeResult();
    GetEditor().RequestRefresh();
}

} // namespace Zep
