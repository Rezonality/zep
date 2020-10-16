// ImGui - standalone example application for SDL2 + OpenGL
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)
// (GL3W is a helper library to access OpenGL functions since there is no standard header to access modern OpenGL functions easily. Alternatives are GLEW, Glad, etc.)

#define NOMINMAX

#include <SDL.h>
#include <stdio.h>
#include <thread>

#include <imgui/imgui.h>

#include <imgui/examples/imgui_impl_opengl3.h>
#include <imgui/examples/imgui_impl_sdl.h>
#include <imgui/misc/freetype/imgui_freetype.h>

#include <clip/clip.h>
#include <mutils/time/time_provider.h>

#include <zep/mcommon/animation/timer.h>
#include <mutils/chibi/chibi.h>
#include <mutils/file/file.h>
#include <mutils/profile/profile.h>
#include <mutils/ui/dpi.h>
#include <tclap/CmdLine.h>

#include "config_app.h"


// About OpenGL function loaders: modern OpenGL doesn't have a standard header file and requires individual function pointers to be loaded manually.
// Helper libraries are often used for this purpose! Here we are supporting a few common ones: gl3w, glew, glad.
// You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h> // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h> // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h> // Initialize with gladLoadGL()
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

#include "zep/mcommon/animation/timer.h"

#undef max

#include "zep/imgui/display_imgui.h"
#include "zep/imgui/editor_imgui.h"
#include "zep/mode_standard.h"
#include "zep/mode_vim.h"
#include "zep/tab_window.h"
#include "zep/theme.h"
#include "zep/window.h"
#include "zep/filesystem.h"

#include "orca/mode_orca.h"
#include "repl/mode_repl.h"

#include "zep/regress.h"

#include <tfd/tinyfiledialogs.h>

#ifndef __APPLE__
#include <FileWatcher/watcher.h>
#endif
#define _MATH_DEFINES_DEFINED
#include "chibi/eval.h"

#include "demo_common.h"

using namespace Zep;
using namespace MUtils;

namespace
{

Chibi scheme;

const std::string shader = R"R(
#version 330 core

uniform mat4 Projection;

// Coordinates  of the geometry
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_tex_coord;
layout(location = 2) in vec4 in_color;

// Outputs to the pixel shader
out vec2 frag_tex_coord;
out vec4 frag_color;

void main()
{
    gl_Position = Projection * vec4(in_position.xyz, 1.0);
    frag_tex_coord = in_tex_coord;
    frag_color = in_color;
}
)R";

std::string startupFile;

Zep::NVec2f GetPixelScale()
{
    float ddpi = 0.0f;
    float hdpi = 0.0f;
    float vdpi = 0.0f;

    auto window = SDL_GL_GetCurrentWindow();
    auto index = window ? SDL_GetWindowDisplayIndex(window) : 0;

    auto res = SDL_GetDisplayDPI(index, &ddpi, &hdpi, &vdpi);
    if (res == 0 && hdpi != 0)
    {
        return Zep::NVec2f(hdpi, vdpi) / 96.0f;
    }
    return Zep::NVec2f(1.0f);
}

} // namespace

bool ReadCommandLine(int argc, char** argv, int& exitCode)
{
    try
    {
        TCLAP::CmdLine cmd("Zep", ' ', "0.1");
        TCLAP::UnlabeledValueArg<std::string> fileArg("file", "filename", false, "", "string");
        cmd.setExceptionHandling(false);
        cmd.ignoreUnmatched(false);

        cmd.add(fileArg);
        if (argc != 0)
        {
            cmd.parse(argc, argv);
            startupFile = fileArg.getValue();
        }
        exitCode = 0;
    }
    catch (TCLAP::ArgException& e) // catch any exceptions
    {
        // Report argument exceptions to the message box
        std::ostringstream strError;
        strError << e.argId() << " : " << e.error();
        //UIManager::Instance().AddMessage(MessageType::Error | MessageType::System, strError.str());
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

// A helper struct to init the editor and handle callbacks
struct ZepContainerImGui : public IZepComponent, public IZepReplProvider
{
    ZepContainerImGui(const std::string& startupFilePath, const std::string& configPath)
        : spEditor(std::make_unique<ZepEditor_ImGui>(configPath, GetPixelScale()))
    {
        chibi_init(scheme, SDL_GetBasePath());

        // ZepEditor_ImGui will have created the fonts for us; but we need to build
        // the font atlas
        auto fontPath = std::string(SDL_GetBasePath()) + "Cousine-Regular.ttf";
        auto& display = static_cast<ZepDisplay_ImGui&>(spEditor->GetDisplay());

        int fontPixelHeight = (int)dpi_pixel_height_from_point_size(DemoFontPtSize, GetPixelScale().y);

        auto& io = ImGui::GetIO();
        ImVector<ImWchar> ranges;
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesDefault()); // Add one of the default ranges
        builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic()); // Add one of the default ranges
        builder.AddRanges(greek_range);
        builder.BuildRanges(&ranges); // Build the final result (ordered ranges with all the unique characters submitted)
    
        ImFontConfig cfg;
        cfg.OversampleH = 4;
        cfg.OversampleV = 4;

        auto pImFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath.c_str(), float(fontPixelHeight), &cfg, ranges.Data);

        display.SetFont(ZepTextType::UI, std::make_shared<ZepFont_ImGui>(display, pImFont, fontPixelHeight));
        display.SetFont(ZepTextType::Text, std::make_shared<ZepFont_ImGui>(display, pImFont, fontPixelHeight));
        display.SetFont(ZepTextType::Heading1, std::make_shared<ZepFont_ImGui>(display, pImFont, int(fontPixelHeight * 1.75)));
        display.SetFont(ZepTextType::Heading2, std::make_shared<ZepFont_ImGui>(display, pImFont, int(fontPixelHeight * 1.5)));
        display.SetFont(ZepTextType::Heading3, std::make_shared<ZepFont_ImGui>(display, pImFont, int(fontPixelHeight * 1.25)));

        unsigned int flags = 0; // ImGuiFreeType::NoHinting;
        ImGuiFreeType::BuildFontAtlas(ImGui::GetIO().Fonts, flags);

        spEditor->RegisterCallback(this);

        ZepMode_Orca::Register(*spEditor);

        ZepRegressExCommand::Register(*spEditor);

        // Repl
        ZepReplExCommand::Register(*spEditor, this);
        ZepReplEvaluateOuterCommand::Register(*spEditor, this);
        ZepReplEvaluateInnerCommand::Register(*spEditor, this);
        ZepReplEvaluateCommand::Register(*spEditor, this);

        if (!startupFilePath.empty())
        {
            spEditor->InitWithFileOrDir(startupFilePath);
        }
        else
        {
            spEditor->InitWithText("Shader.vert", shader);
        }
        
        // File watcher not used on apple yet ; needs investigating as to why it doesn't compile/run
        // The watcher is being used currently to update the config path, but clients may want to do more interesting things
        // by setting up watches for the current dir, etc.
#ifndef __APPLE__
        MUtils::Watcher::Instance().AddWatch(
            spEditor->GetFileSystem().GetConfigPath(), [&](const ZepPath& path) {
                if (spEditor)
                {
                    ZLOG(DBG, "Config File Change: " << path.string());
                    spEditor->OnFileChanged(spEditor->GetFileSystem().GetConfigPath() / path);
                }
            },
            false);
#endif
    }

    ZepContainerImGui()
    {
        spEditor->UnRegisterCallback(this);
    }

    void Destroy()
    {
        spEditor.reset();
    }

    virtual std::string ReplParse(ZepBuffer& buffer, const GlyphIterator& cursorOffset, ReplParseType type) override
    {
        ZEP_UNUSED(cursorOffset);
        ZEP_UNUSED(type);

        GlyphRange range;
        if (type == ReplParseType::OuterExpression)
        {
            range = buffer.GetExpression(ExpressionType::Outer, cursorOffset, { '(' }, { ')' });
        }
        else if (type == ReplParseType::SubExpression)
        {
            range = buffer.GetExpression(ExpressionType::Inner, cursorOffset, { '(' }, { ')' });
        }
        else
        {
            range = GlyphRange(buffer.Begin(), buffer.End());
        }

        if (range.first >= range.second)
            return "<No Expression>";

        const auto& text = buffer.GetWorkingBuffer();
        auto eval = std::string(text.begin() + range.first.Index(), text.begin() + range.second.Index());

        // Flash the evaluated expression
        FlashType flashType = FlashType::Flash;
        float time = 1.0f;
        buffer.BeginFlash(time, flashType, range);

        auto ret = chibi_repl(scheme, NULL, eval);
        ret = RTrim(ret);

        GetEditor().SetCommandText(ret);
        return ret;
    }

    virtual std::string ReplParse(const std::string& str) override
    {
        auto ret = chibi_repl(scheme, NULL, str);
        ret = RTrim(ret);
        return ret;
    }

    virtual bool ReplIsFormComplete(const std::string& str, int& indent) override
    {
        int count = 0;
        for (auto& ch : str)
        {
            if (ch == '(')
                count++;
            if (ch == ')')
                count--;
        }

        if (count < 0)
        {
            indent = -1;
            return false;
        }
        else if (count == 0)
        {
            return true;
        }

        int count2 = 0;
        indent = 1;
        for (auto& ch : str)
        {
            if (ch == '(')
                count2++;
            if (ch == ')')
                count2--;
            if (count2 == count)
            {
                break;
            }
            indent++;
        }
        return false;
    }

    // Inherited via IZepComponent
    virtual void Notify(std::shared_ptr<ZepMessage> message) override
    {
        if (message->messageId == Msg::Tick)
        {
#ifndef __APPLE__
            MUtils::Watcher::Instance().Update();
#endif
        }
        else if (message->messageId == Msg::GetClipBoard)
        {
            clip::get_text(message->str);
            message->handled = true;
        }
        else if (message->messageId == Msg::SetClipBoard)
        {
            clip::set_text(message->str);
            message->handled = true;
        }
        else if (message->messageId == Msg::RequestQuit)
        {
            quit = true;
        }
        else if (message->messageId == Msg::ToolTip)
        {
            auto spTipMsg = std::static_pointer_cast<ToolTipMessage>(message);
            if (spTipMsg->location.Valid() && spTipMsg->pBuffer)
            {
                auto pSyntax = spTipMsg->pBuffer->GetSyntax();
                if (pSyntax)
                {
                    if (pSyntax->GetSyntaxAt(spTipMsg->location).foreground == ThemeColor::Identifier)
                    {
                        auto spMarker = std::make_shared<RangeMarker>(*spTipMsg->pBuffer);
                        spMarker->SetDescription("This is an identifier");
                        spMarker->SetHighlightColor(ThemeColor::Identifier);
                        spMarker->SetTextColor(ThemeColor::Text);
                        spTipMsg->spMarker = spMarker;
                        spTipMsg->handled = true;
                    }
                    else if (pSyntax->GetSyntaxAt(spTipMsg->location).foreground == ThemeColor::Keyword)
                    {
                        auto spMarker = std::make_shared<RangeMarker>(*spTipMsg->pBuffer);
                        spMarker->SetDescription("This is a keyword");
                        spMarker->SetHighlightColor(ThemeColor::Keyword);
                        spMarker->SetTextColor(ThemeColor::Text);
                        spTipMsg->spMarker = spMarker;
                        spTipMsg->handled = true;
                    }
                }
            }
        }
    }

    virtual ZepEditor& GetEditor() const override
    {
        return *spEditor;
    }

    bool quit = false;
    std::unique_ptr<ZepEditor_ImGui> spEditor;
    //malEnvPtr spEnv;
};

int main(int argc, char** argv)
{
    //::AllocConsole();
    int code;
    ReadCommandLine(argc, argv, code);
    if (code != 0)
        return code;

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);

    float ratio = current.w / (float)current.h;
    int startWidth = uint32_t(current.w * .6666);
    int startHeight = uint32_t(startWidth / ratio);

    ZLOG(INFO, "Start Size: " << Zep::NVec2i(startWidth, startHeight));

    SDL_Window* window = SDL_CreateWindow("Zep", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, startWidth, startHeight, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(0); // Enable vsync

    MUtils::NVec2i winSize;
    MUtils::NVec2i targetSize;
    SDL_GetWindowSize(window, &winSize.x, &winSize.y);
    SDL_GL_GetDrawableSize(window, &targetSize.x, &targetSize.y);
    
    ZLOG(INFO, "Screen Window Size: " << winSize);
    ZLOG(INFO, "Drawable Size: " << targetSize);

    // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#endif
    if (err)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // ** Zep specific code, before Initializing font map
    ZepContainerImGui zep(startupFile, SDL_GetBasePath());

    // Setup style
    ImGui::StyleColorsDark();

    ZLOG(INFO, "DPI Scale: " << MUtils::NVec2f(GetPixelScale().x, GetPixelScale().y));

    bool show_demo_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    MUtils::TimeProvider::Instance().StartThread();

    // Main loop
    bool done = false;
    while (!done && !zep.quit)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 10))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;

            // Keep consuming events if they are stacked up
            // Bug #39.
            // This stops keyboard events filling up the queue and replaying after you release the key.
            // It also makes things more snappy
            if (SDL_PollEvent(nullptr) == 1)
            {
                continue;
            }
        }
        else
        {
            // Save battery by skipping display if not required.
            // This will check for cursor flash, for example, to keep that updated.
            if (!zep.spEditor->RefreshRequired())
            {
                continue;
            }
        }

        TIME_SCOPE(Frame);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open"))
                {
                    auto openFileName = tinyfd_openFileDialog(
                        "Choose a file",
                        "",
                        0,
                        nullptr,
                        nullptr,
                        0);
                    if (openFileName != nullptr)
                    {
                        auto pBuffer = zep.GetEditor().GetFileBuffer(openFileName);
                        zep.GetEditor().GetActiveTabWindow()->GetActiveWindow()->SetBuffer(pBuffer);
                    }
                }
                ImGui::EndMenu();
            }

            const auto& buffer = zep.GetEditor().GetActiveTabWindow()->GetActiveWindow()->GetBuffer();

            if (ImGui::BeginMenu("Settings"))
            {
                if (ImGui::BeginMenu("Editor Mode"))
                {
                    bool enabledVim = strcmp(buffer.GetMode()->Name(), Zep::ZepMode_Vim::StaticName()) == 0;
                    bool enabledNormal = !enabledVim;
                    if (ImGui::MenuItem("Vim", "CTRL+2", &enabledVim))
                    {
                        zep.GetEditor().SetGlobalMode(Zep::ZepMode_Vim::StaticName());
                    }
                    else if (ImGui::MenuItem("Standard", "CTRL+1", &enabledNormal))
                    {
                        zep.GetEditor().SetGlobalMode(Zep::ZepMode_Standard::StaticName());
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Theme"))
                {
                    bool enabledDark = zep.GetEditor().GetTheme().GetThemeType() == ThemeType::Dark ? true : false;
                    bool enabledLight = !enabledDark;

                    if (ImGui::MenuItem("Dark", "", &enabledDark))
                    {
                        zep.GetEditor().GetTheme().SetThemeType(ThemeType::Dark);
                    }
                    else if (ImGui::MenuItem("Light", "", &enabledLight))
                    {
                        zep.GetEditor().GetTheme().SetThemeType(ThemeType::Light);
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                auto pTabWindow = zep.GetEditor().GetActiveTabWindow();
                if (ImGui::MenuItem("Horizontal Split"))
                {
                    pTabWindow->AddWindow(&pTabWindow->GetActiveWindow()->GetBuffer(), pTabWindow->GetActiveWindow(), RegionLayoutType::VBox);
                }
                else if (ImGui::MenuItem("Vertical Split"))
                {
                    pTabWindow->AddWindow(&pTabWindow->GetActiveWindow()->GetBuffer(), pTabWindow->GetActiveWindow(), RegionLayoutType::HBox);
                }
                ImGui::EndMenu();
            }

            // Helpful for diagnostics
            // Make sure you run a release build; iterator debugging makes the debug build much slower
            // Currently on a typical file, editor display time is < 1ms, and editor editor time is < 2ms
            if (ImGui::BeginMenu("Timings"))
            {
                for (auto& p : globalProfiler.timerData)
                {
                    std::ostringstream strval;
                    strval << p.first << " : " << p.second.current / 1000.0 << "ms"; // << " Last: " << p.second.current / 1000.0 << "ms";
                    ImGui::MenuItem(strval.str().c_str());
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        // This is a bit messy; and I have no idea why I don't need to remove the menu fixed_size from the calculation!
        // The point of this code is to fill the main window with the Zep window
        // It is only done once so the user can play with the window if they want to for testing
        auto menuSize = ImGui::GetStyle().FramePadding.y * 2 + ImGui::GetFontSize();
        ImGui::SetNextWindowPos(ImVec2(0, menuSize), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(float(w), float(h - menuSize)), ImGuiCond_Always);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::Begin("Zep", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

        auto min = ImGui::GetCursorScreenPos();
        auto max = ImGui::GetContentRegionAvail();
        max.x = std::max(1.0f, max.x);
        max.y = std::max(1.0f, max.y);

        // Fill the window
        max.x = min.x + max.x;
        max.y = min.y + max.y;
        zep.spEditor->SetDisplayRegion(Zep::NVec2f(min.x, min.y), Zep::NVec2f(max.x, max.y));

        // Display the editor inside this window
        zep.spEditor->Display();
        zep.spEditor->HandleInput();

        ImGui::End();
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(1);

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // Rendering
        ImGui::Render();
        SDL_GL_MakeCurrent(window, gl_context);
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);

        MUtilsFrameMark;
    }

    // Quit the ticker
    MUtils::TimeProvider::Instance().Free();

    zep.Destroy();
    chibi_destroy(scheme);
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
