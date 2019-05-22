#pragma once

#include "mode.h"
#include <future>
#include <memory>
#include <regex>

namespace Zep
{

class ZepMode_Search : public ZepMode
{
public:
    ZepMode_Search(ZepEditor& editor);
    ~ZepMode_Search();

    virtual void AddKeyPress(uint32_t key, uint32_t modifiers = 0) override;
    virtual void Begin() override;
    virtual void Notify(std::shared_ptr<ZepMessage> message) override;
    
    static const char* StaticName()
    {
        return "Search";
    }
    virtual const char* Name() const override
    {
        return StaticName();
    }

private:
    void InitSearchTree();
    void ShowTreeResult();
    void UpdateTree();

    enum class OpenType
    {
        Replace,
        VSplit,
        HSplit,
        Tab
    };
    void OpenSelection(OpenType type);

private:

    // List of files found in the directory search
    struct FileSearchResult
    {
        ZepPath root;
        std::vector<ZepPath> paths;
        std::vector<std::string> lowerPaths;
    };

    // List of lines in the file result, with last found char
    struct SearchResult
    {
        uint32_t index = 0;
        uint32_t location = 0;
    };

    // A mapping from character distance to a list of lines
    struct IndexSet
    {
        std::multimap<uint32_t, SearchResult> indices;
    };

    bool fileSearchActive = false;
    bool treeSearchActive = false;

    // Results of the file search and the indexing threads
    std::future<std::shared_ptr<FileSearchResult>> m_indexResult;
    std::future<std::shared_ptr<IndexSet>> m_searchResult;

    // All files that can potentially match
    std::shared_ptr<FileSearchResult> m_spFilePaths;

    // A hierarchy of index results.
    // The 'top' of the tree is the most narrow finding from a set of 'n' characters
    // index a,b,c -> index b,c -> index c
    std::vector<std::shared_ptr<IndexSet>> m_indexTree;
   
    // What we are searching for 
    std::string m_searchTerm;
    bool m_caseImportant = false;

    // Our temporary window and buffer
    ZepBuffer* m_pSearchBuffer = nullptr;
    ZepWindow* m_pSearchWindow = nullptr;

    // The winndow we were launched with
    ZepWindow* m_pLaunchWindow = nullptr;

    std::vector<std::regex> m_ignorePatterns;
};

} // namespace Zep
