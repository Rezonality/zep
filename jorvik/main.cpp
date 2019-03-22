#define ZEP_SINGLE_HEADER_BUILD
#define ZEP_USE_SDL
#define ZEP_FEATURE_CPP_FILE_SYSTEM
#define ZEP_FEATURE_FILE_WATCHER
#include <zep/src/zep.h>

#include "utils.h"
#include "utils/ui/ui_manager.h"
#include "utils/file/runtree.h"
#include "utils/animation/timer.h"
#include "utils/logger.h"
#include "utils/ui/dpi.h"

#include <imgui/imgui.h>
#include <tclap/CmdLine.h>
#include "sdl.h"

#include "jorvik.h"
#include "editor.h"
#include "visual/dx12/device_dx12.h"
#include "visual/vulkan/device_vulkan.h"

#include "config_app.h"

using namespace Mgfx;

namespace Mgfx
{
#undef ERROR
#ifdef _DEBUG
Logger logger = {true, DEBUG};
#else
Logger logger = {true, WARNING};
#endif
bool LOG::disabled = false;
}

bool ReadCommandLine(int argc, char** argv, int& exitCode)
{
    try
    {
        TCLAP::CmdLine cmd(APPLICATION_NAME, ' ', APPLICATION_VERSION);
        TCLAP::SwitchArg gl("", "gl", "Enable OpenGL", cmd, false);
        TCLAP::SwitchArg d3d("", "d3d", "Enable DX12", cmd, false);
        TCLAP::SwitchArg console("c", "console", "Enable Console", cmd, false);
        TCLAP::SwitchArg reset("", "reset", "Reset configuration", cmd, false);

        cmd.setExceptionHandling(false);
        cmd.ignoreUnmatched(false);

        if (argc != 0)
        {
            cmd.parse(argc, argv);

            jorvik.forceReset = reset.getValue();

#ifdef WIN32
            // Show the console if the user supplied args
            // On a Win32 app, this isn't available by default
            if (console.getValue())
            {
                AllocConsole();
                freopen("CONIN$", "r", stdin);
                freopen("CONOUT$", "w", stdout);
                freopen("CONOUT$", "w", stderr);
            }
#endif
        }
    }
    catch (TCLAP::ArgException& e) // catch any exceptions
    {
        // Report argument exceptions to the message box
        std::ostringstream strError;
        strError << e.argId() << " : " << e.error();
        UIManager::Instance().AddMessage(MessageType::Error | MessageType::System, strError.str());
        exitCode = 1;
        return false;
    }
    catch (TCLAP::ExitException& e)
    {
        // Allow PC app to continue, and ignore exit due to help/version
        // This avoids the danger that the user passed --help on the command line and wondered why the app just exited
        exitCode = e.getExitStatus();
        return true;
    }
    return true;
}

void set_debug_crt()
{
    // CRT Debug flags on windows
#if defined(_MSC_VER)
    int tmpFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    tmpFlag |= /*_CRTDBG_LEAK_CHECK_DF |*/ _CRTDBG_CHECK_ALWAYS_DF;
    _CrtSetDbgFlag(tmpFlag);
    //_CrtSetBreakAlloc(3277);
    //_CrtMemState s1;
    //_CrtMemCheckpoint( &s1 );
#endif
}

void setup_imgui()
{
    // Font for editor
    static const ImWchar ranges[] = {
        0x0020,
        0x00FF, // Basic Latin + Latin Supplement
        0,
    };
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 1;
    config.DstFont = ImGui::GetFont();
    io.Fonts->AddFontFromFileTTF(runtree_find_asset("fonts/ProggyClean.ttf").string().c_str(), 13 * dpi.scaleFactor, &config, ranges);
}

int main(int argc, char** argv)
{
    check_dpi();

    set_debug_crt();

    // Parse the command line
    int exitCode = 0;
    if (!ReadCommandLine(argc, argv, exitCode))
    {
        return exitCode;
    }

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        LOG(ERROR) << SDL_GetError();
        return -1;
    }

    // Setup app runtree
    runtree_init(true);

    // Initialize jorvik
    jorvik_init_settings();

    // Create device
    jorvik.spDevice = std::make_unique<DeviceDX12>();
    //jorvik.spDevice = std::make_unique<DeviceVulkan>();
    if (!jorvik.spDevice->Init("Jorvik"))
    {
        UIManager::Instance().AddMessage(MessageType::Error | MessageType::System,
            std::string("Couldn't create device: ") + std::string(jorvik.spDevice->GetName()));
        return 0;
    }

    setup_imgui();

    // TODO: Better name
    jorvik_init();

    // Main loop
    Mgfx::timer frameTimer;
    timer_start(frameTimer);
    bool done = false;
    while (!done)
    {
        SDL_Event event;
        bool redraw = false;
        while (SDL_PollEvent(&event))
        {
            redraw = true;

            if (jorvik.spDevice->Initialized)
            {
                jorvik.spDevice->ProcessEvent(event);
                editor_process_event(event);
            }

            if (event.type == SDL_QUIT || jorvik.done)
            {
                done = true;
                break;
            }
        }

        auto frameDelta = (float)Mgfx::timer_to_seconds(timer_get_elapsed(frameTimer));
        timer_restart(frameTimer);

        if (jorvik.spDevice->pWindow)
        {
            jorvik_tick(frameDelta);
            jorvik_render(frameDelta);
        }
    }

    jorvik_destroy();

    SDL_Quit();

    ImGui::DestroyContext();

    return 0;
}
