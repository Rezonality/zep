#pragma once

#include "entities/entities.h"
#include <glm/glm.hpp>
#include <vector>
#include <zep/src/zep.h>

#include <functional>
#include "utils/callback.h"

union SDL_Event;

namespace Mgfx
{

class VkWindow;
class CompileResult;

struct ICompile;
struct ZepWrapper : Zep::IZepComponent
{
    ZepWrapper(const fs::path& root_path, std::function<void(std::shared_ptr<Zep::ZepMessage>)> fnCommandCB)
        : zepEditor(Zep::ZepPath(root_path.string()))
        , Callback(fnCommandCB)
    {
        zepEditor.RegisterCallback(this);
    }

    virtual Zep::ZepEditor& GetEditor() const override
    {
        return (Zep::ZepEditor&)zepEditor;
    }

    virtual void Notify(std::shared_ptr<Zep::ZepMessage> message) override
    {
        Callback(message);
        return;
    }

    virtual void HandleInput()
    {
        zepEditor.HandleInput();
    }

    Zep::ZepEditor_ImGui zepEditor;
    std::function<void(std::shared_ptr<Zep::ZepMessage>)> Callback;
};

struct GEditor
{
    bool show_editor = false;
    bool show_zep = false;
    bool show_debug = false;

    std::shared_ptr<ZepWrapper> spZep;
    bool zep_focused = false;
};

extern GEditor editor;

void editor_init();
void editor_destroy();
void editor_tick();
void editor_show_zep();
void editor_draw_ui(TimeDelta dt);
void editor_process_event(SDL_Event& event);
void editor_process_compile_result(ICompile* pResult);
std::string editor_get_text(const fs::path& path);
void editor_update_assets();

} // namespace Mgfx
