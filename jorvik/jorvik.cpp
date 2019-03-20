#include "utils.h"
#include "utils/file/runtree.h"
#include "utils/animation/timer.h"
#include "utils/ui/ui_manager.h"

#include "m3rdparty/cpptoml/include/cpptoml.h"

#include "editor.h"
#include "jorvik.h"
#include "opus.h"

#include "visual/scene.h"
#include "visual/render_graph.h"
#include "visual/render_node.h"
#include "visual/shader_file_asset.h"
#include "visual/pass_renderstate.h"
#include "visual/dx12/device_dx12.h"

namespace Mgfx
{

Jorvik jorvik;

static void read_settings()
{
    try
    {
        auto spSettings = cpptoml::parse_file((jorvik.roamingPath / "settings" / "settings.toml").string());
        if (spSettings == nullptr)
            return;

        auto opusPath = spSettings->get_qualified_as<std::string>("startup.opus").value_or("");
        jorvik.spOpus = Opus::LoadOpus(opusPath);
        if (jorvik.spOpus != nullptr)
        {
            return;
        }
    }
    catch (std::exception& ex)
    {
        // TOOD: Handle
        LOG(DEBUG) << "Failed to read settings: " << ex.what();
    }
    jorvik.spOpus = Opus::MakeDefaultOpus();
}

static void get_roaming_path()
{
    // Try really hard to find somewhere to put stuff!
    jorvik.roamingPath = file_roaming_path();
    if (jorvik.roamingPath.empty())
    {
        jorvik.roamingPath = file_appdata_path();
        if (jorvik.roamingPath.empty())
        {
            jorvik.roamingPath = file_documents_path();
            if (jorvik.roamingPath.empty())
            {
                jorvik.roamingPath = fs::temp_directory_path();
            }
        }
    }

    // TOOD: Error message on no path
    jorvik.roamingPath = jorvik.roamingPath / "jorvik";
    if (!fs::exists(jorvik.roamingPath))
    {
        fs::create_directories(jorvik.roamingPath);
    }
}

static void copy_settings_to_roaming()
{
    auto flags = fs::copy_options::recursive;
    if (jorvik.forceReset)
    {
        flags |= fs::copy_options::overwrite_existing;
    }
    else
    {
        flags |= fs::copy_options::skip_existing;
    }

    fs::copy(runtree_path() / "settings", jorvik.roamingPath / "settings", flags);
}

void jorvik_init_settings()
{
    get_roaming_path();

    if (!jorvik.roamingPath.empty())
    {
        copy_settings_to_roaming();
        read_settings();
    }
}

void jorvik_add_listener(fnMessage fn)
{
    jorvik.listeners.push_back(fn);
}

void jorvik_destroy_globals()
{
    file_destroy_dir_watch();
}

void jorvik_init()
{
    TIME_SCOPE(jorvik_init);

    entity_init();
    editor_init();

    if (jorvik.spOpus)
    {
        jorvik.spOpus->Init();
    }
}

void jorvik_destroy()
{
    editor_destroy();
    runtree_destroy();
    entity_destroy();

    jorvik.spOpus.reset();

    jorvik.spDevice->Destroy();
    jorvik.spDevice.reset();

    jorvik_destroy_globals();
}

bool jorvik_refresh_required()
{
    return true;
}

void jorvik_tick(float delta)
{
    jorvik.time += delta;

    file_update_dir_watch();

    compile_tick();

    editor_tick();
}

void jorvik_render(float dt)
{
    // No more events, lets do some drawing
    if (jorvik.spDevice->pWindow)
    {
        jorvik.spDevice->RenderFrame(dt, [&]() {
            if (jorvik.spOpus == nullptr)
                return;

            if (jorvik.spOpus->GetScene() == nullptr ||
                jorvik.spOpus->GetScene()->GetGraph() == nullptr)
                return;

            jorvik.spOpus->GetScene()->GetGraph()->ForEachNode([](auto pNode)
            {
                jorvik.spDevice->DrawFSQuad(pNode->GetRenderState()->GetCompileResult());
            });
        });
    }
}

void jorvik_send_message(std::shared_ptr<JorvikMessage> msg)
{
    for (auto& listener : jorvik.listeners)
    {
        listener(msg);
    }
}

std::string jorvik_get_text(const fs::path& path)
{
    auto str = editor_get_text(path);
    if (str.empty())
    {
        str = file_read(path);
    }
    return str;
}

} // namespace Mgfx
