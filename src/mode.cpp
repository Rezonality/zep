#include "zep/mode.h"
#include "zep/buffer.h"
#include "zep/commands.h"
#include "zep/editor.h"
#include "zep/filesystem.h"
#include "zep/mode_search.h"
#include "zep/tab_window.h"
#include "zep/window.h"

#include "zep/mcommon/logger.h"

namespace Zep
{

ZepMode::ZepMode(ZepEditor& editor)
    : ZepComponent(editor)
{
}

ZepMode::~ZepMode()
{
}

ZepWindow* ZepMode::GetCurrentWindow() const
{
    if (GetEditor().GetActiveTabWindow())
    {
        return GetEditor().GetActiveTabWindow()->GetActiveWindow();
    }
    return nullptr;
}

EditorMode ZepMode::GetEditorMode() const
{
    return m_currentMode;
}

void ZepMode::AddCommandText(std::string strText)
{
    for (auto& ch : strText)
    {
        AddKeyPress(ch);
    }
}

void ZepMode::AddCommand(std::shared_ptr<ZepCommand> spCmd)
{
    if (GetCurrentWindow() && GetCurrentWindow()->GetBuffer().TestFlags(FileFlags::Locked))
    {
        // Ignore commands on buffers because we are view only,
        // and all commands currently modify the buffer!
        return;
    }

    spCmd->Redo();
    m_undoStack.push(spCmd);

    // Can't redo anything beyond this point
    std::stack<std::shared_ptr<ZepCommand>> empty;
    m_redoStack.swap(empty);

    if (spCmd->GetCursorAfter() != -1)
    {
        GetCurrentWindow()->SetBufferCursor(spCmd->GetCursorAfter());
    }
}

void ZepMode::Redo()
{
    bool inGroup = false;
    do
    {
        if (!m_redoStack.empty())
        {
            auto& spCommand = m_redoStack.top();
            spCommand->Redo();

            if (spCommand->GetFlags() & CommandFlags::GroupBoundary)
            {
                inGroup = !inGroup;
            }

            if (spCommand->GetCursorAfter() != -1)
            {
                GetCurrentWindow()->SetBufferCursor(spCommand->GetCursorAfter());
            }

            m_undoStack.push(spCommand);
            m_redoStack.pop();
        }
        else
        {
            break;
        }
    } while (inGroup);
}

void ZepMode::Undo()
{
    bool inGroup = false;
    do
    {
        if (!m_undoStack.empty())
        {
            auto& spCommand = m_undoStack.top();
            spCommand->Undo();

            if (spCommand->GetFlags() & CommandFlags::GroupBoundary)
            {
                inGroup = !inGroup;
            }

            if (spCommand->GetCursorBefore() != -1)
            {
                GetCurrentWindow()->SetBufferCursor(spCommand->GetCursorBefore());
            }

            m_redoStack.push(spCommand);
            m_undoStack.pop();
        }
        else
        {
            break;
        }
    } while (inGroup);
}

NVec2i ZepMode::GetVisualRange() const
{
    return NVec2i(m_visualBegin, m_visualEnd);
}

bool ZepMode::HandleGlobalCtrlCommand(const std::string& cmd, uint32_t modifiers, bool& needMoreChars) const
{
    needMoreChars = false;

    // TODO: I prefer 'ko' but I need to put in a keymapper which can see when the user hasn't pressed a second key in a given time
    // otherwise, this hides 'ctrl+k' for pane navigation!
    if (cmd[0] == 'i')
    {
        if (cmd == "i")
        {
            needMoreChars = true;
        }
        else if (cmd == "io")
        {
            // This is a quick and easy 'alternate file swap'.
            // It searches a preset list of useful folder targets around the current file.
            // A better alternative might be a wildcard list of relations, but this works well enough
            // It also only looks for a file with the same name and different extension!
            // it is good enough for my current needs...
            auto& buffer = GetCurrentWindow()->GetBuffer();
            auto path = buffer.GetFilePath();
            if (!path.empty() && GetEditor().GetFileSystem().Exists(path))
            {
                auto ext = path.extension();
                auto searchPaths = std::vector<ZepPath>{
                    path.parent_path(),
                    path.parent_path().parent_path(),
                    path.parent_path().parent_path().parent_path()
                };

                auto ignoreFolders = std::vector<std::string>{ "build", ".git", "obj", "debug", "release" };

                auto priorityFolders = std::vector<std::string>{ "source", "include", "src", "inc", "lib" };

                // Search the paths, starting near and widening
                for (auto& p : searchPaths)
                {
                    if (p.empty())
                        continue;

                    bool found = false;

                    // Look for the priority folder locations
                    std::vector<ZepPath> searchFolders{ path.parent_path() };
                    for (auto& priorityFolder : priorityFolders)
                    {
                        GetEditor().GetFileSystem().ScanDirectory(p, [&](const ZepPath& currentPath, bool& recurse) {
                            recurse = false;
                            if (GetEditor().GetFileSystem().IsDirectory(currentPath))
                            {
                                auto lower = string_tolower(currentPath.filename().string());
                                if (std::find(ignoreFolders.begin(), ignoreFolders.end(), lower) != ignoreFolders.end())
                                {
                                    return true;
                                }

                                if (priorityFolder == lower)
                                {
                                    searchFolders.push_back(currentPath);
                                }
                            }
                            return true;
                        });
                    }

                    for (auto& folder : searchFolders)
                    {
                        LOG(INFO) << "Searching: " << folder.string();

                        GetEditor().GetFileSystem().ScanDirectory(folder, [&](const ZepPath& currentPath, bool& recurse) {
                            recurse = true;
                            if (path.stem() == currentPath.stem() && !(currentPath.extension() == path.extension()))
                            {
                                auto load = GetEditor().GetFileBuffer(currentPath, 0, true);
                                if (load != nullptr)
                                {
                                    GetCurrentWindow()->SetBuffer(load);
                                    found = true;
                                    return false;
                                }
                            }
                            return true;
                        });
                        if (found)
                            return true;
                    }
                }
            }
        }
        return true;
    }
    else if (cmd == "=" || ((cmd == "+") && (modifiers & ModifierKey::Shift)))
    {
        GetEditor().GetDisplay().SetFontPointSize(std::min(GetEditor().GetDisplay().GetFontPointSize() + 1.0f, 20.0f));
        return true;
    }
    else if (cmd == "-" || ((cmd == "_") && (modifiers & ModifierKey::Shift)))
    {
        GetEditor().GetDisplay().SetFontPointSize(std::max(10.0f, GetEditor().GetDisplay().GetFontPointSize() - 1.0f));
        return true;
    }
    // Moving between splits
    else if (cmd == "j")
    {
        GetCurrentWindow()->GetTabWindow().DoMotion(WindowMotion::Down);
        return true;
    }
    else if (cmd == "k")
    {
        GetCurrentWindow()->GetTabWindow().DoMotion(WindowMotion::Up);
        return true;
    }
    else if (cmd == "h")
    {
        GetCurrentWindow()->GetTabWindow().DoMotion(WindowMotion::Left);
        return true;
    }
    else if (cmd == "l")
    {
        GetCurrentWindow()->GetTabWindow().DoMotion(WindowMotion::Right);
        return true;
    }
    // global search
    else if (cmd == "p" || cmd == ",")
    {
        GetEditor().BeginSecondaryMode(std::make_shared<ZepMode_Search>(GetEditor()));
        return true;
    }
    return false;
}

} // namespace Zep
