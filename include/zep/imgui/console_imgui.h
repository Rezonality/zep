#pragma once

#include "zep.h"

namespace ImGui
{

struct ZepConsole : Zep::IZepComponent
{
    std::function<bool(const std::string&)> Callback;
    Zep::ZepEditor_ImGui zepEditor;
    bool pendingScroll = true;

    // Intercept messages from the editor command line and relay them
    virtual void Notify(std::shared_ptr<Zep::ZepMessage> message)
    {
        if (message->messageId == Zep::Msg::HandleCommand)
        {
            message->handled = Callback(message->str);
            return;
        }
        message->handled = false;
        return;
    }

    virtual Zep::ZepEditor& GetEditor() const
    {
        return (Zep::ZepEditor&)zepEditor;
    }

    ZepConsole(Zep::ZepPath& p)
        : zepEditor(p)
    {
        zepEditor.RegisterCallback(this);
        auto pBuffer = zepEditor.GetEmptyBuffer("Log");
        pBuffer->SetFlags(Zep::FileFlags::ReadOnly);
    }

    void AddLog(const char* fmt, ...) IM_FMTARGS(2)
    {
        // FIXME-OPT
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
        buf[IM_ARRAYSIZE(buf) - 1] = 0;
        va_end(args);

        auto pBuffer = zepEditor.GetMRUBuffer();

        pBuffer->Insert(pBuffer->EndLocation(), buf);
        pBuffer->Insert(pBuffer->EndLocation(), "\n");
     
        pendingScroll = true;
    }

    void Draw(const char* title, bool* p_open, const ImVec4& targetRect, float blend)
    {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.13f, 0.1f, 0.12f, 0.95f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::SetNextWindowSize(ImVec2(targetRect.z, targetRect.w), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(targetRect.x, (targetRect.y - targetRect.w) + (targetRect.w * blend)), ImGuiCond_Always);

        if (!ImGui::Begin(title, p_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::PopStyleVar(1);
            ImGui::PopStyleColor(1);
            ImGui::End();
            return;
        }

        auto size = ImGui::GetWindowContentRegionMax();
        auto cursor = ImGui::GetCursorScreenPos();
        
        zepEditor.SetDisplayRegion(Zep::NVec2f(cursor.x, cursor.y), Zep::NVec2f(size.x, size.y - cursor.y));
        zepEditor.Display();
        zepEditor.HandleInput();

        if (pendingScroll)
        {
            zepEditor.GetActiveTabWindow()->GetActiveWindow()->MoveCursorY(Zep::MaxCursorMove);
            pendingScroll = false;
        }

        if (blend < 1.0f)
        {
            // TODO: This looks like a hack: investigate why it is needed for the drop down console.
            // I think the intention here is to ensure the mode is reset while it is dropping down. I don't recall.
            zepEditor.GetActiveTabWindow()->GetActiveWindow()->GetBuffer().GetMode()->Begin();
        }

        ImGui::End();
        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(1);
    }

};

} // namespace ImGui
