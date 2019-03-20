//#include "file/runtree.h"

#include "utils.h"
#include "utils/math/mathutils.h" // For IMGui overrides
#include "utils/logger.h"

#include "visual/dx12/device_dx12.h"
#include "visual/scene.h"

#include "compile.h"
#include "editor.h"
#include "jorvik.h"
#include "opus.h"
#include "config_app.h"

#include <imgui/imgui.h>


using namespace Zep;

namespace Mgfx
{

// One pending compiler for each file seen.
// Note: These are kept around so that duplicate compiles of the same file can be finished and requeued
std::map<ZepPath, std::shared_ptr<scheduler>> PendingTextProcessing;

GEditor editor;

void editor_init()
{
    // Initialize the editor and watch for changes 
    editor.spZep = std::make_shared<ZepWrapper>(JORVIK_ROOT, [](std::shared_ptr<ZepMessage> spMessage) -> void {
        if (spMessage->messageId == Zep::Msg::Buffer)
        {
            auto spBufferMsg = std::static_pointer_cast<BufferMessage>(spMessage);
            if (spBufferMsg->type == BufferMessageType::TextAdded || spBufferMsg->type == BufferMessageType::TextDeleted || spBufferMsg->type == BufferMessageType::TextChanged)
            {
                auto path = spBufferMsg->pBuffer->GetFilePath();

                if (!path.empty())
                {

                    // Schedule a recompile
                    std::shared_ptr<scheduler> spPending;
                    auto itrCompile = PendingTextProcessing.find(path);
                    if (itrCompile == PendingTextProcessing.end())
                    {
                        spPending = PendingTextProcessing.insert(std::make_pair(path, std::make_shared<scheduler>())).first->second;
                    }
                    else
                    {
                        spPending = itrCompile->second;
                    }

                    // Note, scheduler called on main thread always during the tick.
                    // TODO: Cleaner way to share coroutines
                    scheduler_start(*spPending, 1.0f, [path]() {

                        auto itrFound = PendingTextProcessing.find(path);
                        if (itrFound == PendingTextProcessing.end())
                        {
                            return;
                        }

                        // Execute the coroutine by finding the file, and telling the opus to recompile it
                        auto pBuffer = editor.spZep->GetEditor().GetFileBuffer(path, 0, false);
                        if (pBuffer)
                        {
                            LOG(DEBUG) << "Editor submitting for compile: " << path.string();

                            auto pAsset = jorvik.spOpus->GetFromPath(fs::path(path.string()));
                            auto pCompile = dynamic_cast<ICompile*>(pAsset);
                            if (pCompile)
                            {
                                compile_queue(pCompile);
                            }
                        }
                    });
                }
            }
        }
    });

    // TODO: Handle switching OpusAsset!
    jorvik_add_listener([&](std::shared_ptr<JorvikMessage> msg) {
        auto pCompilerMsg = std::dynamic_pointer_cast<CompilerMessage>(msg);
        if (pCompilerMsg && pCompilerMsg->pCompiled)
        {
            // Update the display of the compile result in the editor
            editor_process_compile_result(pCompilerMsg->pCompiled);
        }
    });
}

void editor_update_assets()
{
    if (!editor.spZep)
    {
        return;
    }
    jorvik.spOpus->ForEachFileAsset([](OpusAsset * pAsset)
    {
        auto pBuffer = editor.spZep->GetEditor().GetFileBuffer(ZepPath(pAsset->GetSourcePath().string()), 0, true);

        bool found = false;
        for (auto& tabWindow : editor.spZep->GetEditor().GetTabWindows())
        {
            for (auto& win : tabWindow->GetWindows())
            {
                if (&win->GetBuffer() == pBuffer)
                {
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            auto pTabWindow = editor.spZep->GetEditor().AddTabWindow();
            pTabWindow->AddWindow(pBuffer, nullptr, false);
        }
    });
}

std::string editor_get_text(const fs::path& path)
{
    if (editor.spZep == nullptr)
    {
        return std::string();
    }

    auto pBuffer = editor.spZep->GetEditor().GetFileBuffer(ZepPath(path.string()), 0, false);
    if (!pBuffer)
    {
        return std::string();
    }

    bool found = false;
    for (auto& tabWindow : editor.spZep->GetEditor().GetTabWindows())
    {
        for (auto& win : tabWindow->GetWindows())
        {
            if (&win->GetBuffer() == pBuffer)
            {
                found = true;
                break;
            }
        }
    }

    if (!found)
    {
        auto pTabWindow = editor.spZep->GetEditor().AddTabWindow();
        pTabWindow->AddWindow(pBuffer, nullptr, false);
    }

    return pBuffer->GetText().string();
}

void editor_tick()
{
    // Update any coroutines
    // TODO: Generalize the coroutine system more
    for (auto itrCompile : PendingTextProcessing)
    {
        scheduler_update(*itrCompile.second);
    }

    // Cleanup
    auto itrRemove = PendingTextProcessing.begin();
    while (itrRemove != PendingTextProcessing.end())
    {
        if (itrRemove->second->state == SchedulerState::Triggered)
        {
            itrRemove = PendingTextProcessing.erase(itrRemove);
        }
        else
        {
            itrRemove++;
        }
    }
}

void editor_process_compile_result(ICompile* pObject)
{
    std::shared_ptr<CompileResult> pResult = pObject->GetCompileResult();
    if (pResult == nullptr || pObject->GetSourcePath().empty())
    {
        return;
    }

    // Editor already created when we process this message
    LOG(DEBUG) << "Finding buffer: " << pObject->GetSourcePath().string();
    auto pBuffer = editor.spZep->GetEditor().GetFileBuffer(pObject->GetSourcePath().string(), 0, false);
    if (!pBuffer)
    {
        assert(!"Not found file");
        return;
    }

    pBuffer->ClearRangeMarkers();

    for (auto& msg : pResult->messages)
    {
        auto spMarker = std::make_shared<RangeMarker>();
        spMarker->description = msg->text;
        spMarker->name = msg->text;
        spMarker->bufferLine = msg->line;
        spMarker->displayType = RangeMarkerDisplayType::All;

        if (msg->msgType == CompileMessageType::Error)
        {
            spMarker->highlightColor = ThemeColor::Error;
        }
        else
        {
            spMarker->highlightColor = ThemeColor::Info;
        }

        long start, end;
        pBuffer->GetLineOffsets(spMarker->bufferLine >= 0 ? spMarker->bufferLine : 0, start, end);
        spMarker->range = BufferRange{start, start};
        if (msg->range.first != -1)
        {
            spMarker->range.first += msg->range.first;
            if (msg->range.second != -1)
                spMarker->range.second += msg->range.second;
        }
        else
        {
            spMarker->range.second = end - 1;
        }
        pBuffer->AddRangeMarker(spMarker);
    }
}

void editor_destroy()
{
}

void editor_process_event(SDL_Event& event)
{
    if (editor.zep_focused)
    {
        return;
    }

    switch (event.type)
    {
    case SDL_KEYDOWN:
    {
        SDL_Keymod km = SDL_GetModState();
        int ctrl = (km & KMOD_LCTRL) || (km & KMOD_RCTRL);
        int key = event.key.keysym.sym;
        if (key == SDLK_g && ctrl != 0)
        {
            editor.show_zep = !editor.show_zep;
        }

        if (key == SDLK_e && ctrl != 0)
        {
            editor.show_editor = !editor.show_editor;
        }
    }
    break;
    default:
        break;
    }
}

void editor_show_zep()
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.13f, 0.1f, 0.12f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));

    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1024, 768), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Zep", &editor.show_zep, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar))
    {
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(1);
        ImGui::End();
        return;
    }

    // Simple menu options for switching mode and splitting
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Settings"))
        {
            if (ImGui::BeginMenu("Editor Mode"))
            {
                bool enabledVim = strcmp(editor.spZep->GetEditor().GetCurrentMode()->Name(), Zep::ZepMode_Vim::StaticName()) == 0;
                bool enabledNormal = !enabledVim;
                if (ImGui::MenuItem("Vim", "CTRL+2", &enabledVim))
                {
                    editor.spZep->GetEditor().SetMode(Zep::ZepMode_Vim::StaticName());
                }
                else if (ImGui::MenuItem("Standard", "CTRL+1", &enabledNormal))
                {
                    editor.spZep->GetEditor().SetMode(Zep::ZepMode_Standard::StaticName());
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Theme"))
            {
                bool enabledDark = editor.spZep->GetEditor().GetTheme().GetThemeType() == ThemeType::Dark ? true : false;
                bool enabledLight = !enabledDark;

                if (ImGui::MenuItem("Dark", "", &enabledDark))
                {
                    editor.spZep->GetEditor().GetTheme().SetThemeType(ThemeType::Dark);
                }
                else if (ImGui::MenuItem("Light", "", &enabledLight))
                {
                    editor.spZep->GetEditor().GetTheme().SetThemeType(ThemeType::Light);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            auto pTabWindow = editor.spZep->GetEditor().GetActiveTabWindow();
            if (ImGui::MenuItem("Horizontal Split"))
            {
                pTabWindow->AddWindow(&pTabWindow->GetActiveWindow()->GetBuffer(), pTabWindow->GetActiveWindow(), false);
            }
            else if (ImGui::MenuItem("Vertical Split"))
            {
                pTabWindow->AddWindow(&pTabWindow->GetActiveWindow()->GetBuffer(), pTabWindow->GetActiveWindow(), true);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    auto min = ImGui::GetCursorScreenPos();
    auto max = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("ZepContainer", max);

    // Fill the window
    max.x = min.x + max.x;
    max.y = min.y + max.y;

    editor.spZep->zepEditor.SetDisplayRegion(NVec2f(min.x, min.y), NVec2f(max.x, max.y));
    editor.spZep->zepEditor.Display();
    editor.zep_focused = ImGui::IsWindowFocused();
    if (editor.zep_focused)
    {
        editor.spZep->zepEditor.HandleInput();
    }

    // Make the zep window focused on start of the demo - just so the user doesn't start typing without it;
    // not sure why I need to do it twice; something else is stealing the focus the first time round
    static int focus_count = 0;
    if (focus_count++ < 2)
    {
        ImGui::SetWindowFocus();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(1);
}

void editor_draw_ui(TimeDelta dt)
{
    /*
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    */
    glm::ivec2 windowSize;
    SDL_GL_GetDrawableSize(jorvik.spDevice->pWindow, &windowSize.x, &windowSize.y);

    editor_show_zep();

    if (!editor.show_editor)
        return;

};

} // namespace Mgfx
