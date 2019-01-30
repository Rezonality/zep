// ImGui - standalone example application for SDL2 + OpenGL
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)
// (GL3W is a helper library to access OpenGL functions since there is no standard header to access modern OpenGL functions easily. Alternatives are GLEW, Glad, etc.)

#include "imgui.h"

#include "examples/imgui_impl_opengl3.h"
#include "examples/imgui_impl_sdl.h"
#include <SDL.h>
#include <stdio.h>
#include <thread>

#include <m3rdparty/tclap/include/tclap/CmdLine.h>

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

#include "mcommon/animation/timer.h"

#undef max

#include "src/imgui/display_imgui.h"
#include "src/imgui/editor_imgui.h"
#include "src/mode_standard.h"
#include "src/mode_vim.h"
#include "src/theme.h"
#include "src/tab_window.h"
#include "src/window.h"

#include "m3rdparty/tfd/tinyfiledialogs.h"

using namespace Zep;

#include "src/tests/longtext.tt"

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
struct ZepContainer : public IZepComponent
{
    ZepContainer(const std::string& startupFile)
        : spEditor(std::make_unique<ZepEditor_ImGui>(ZEP_ROOT))
    {
        spEditor->RegisterCallback(this);

        float ddpi = 0.0f;
        float hdpi = 0.0f;
        float vdpi = 0.0f;
        auto res = SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi);
        if (res == 0 && hdpi != 0)
        {
            spEditor->SetPixelScale(hdpi / 96.0f);
        }

        if (!startupFile.empty())
        {
            spEditor->InitWithFileOrDir(startupFile);
        }

        // Add a shader, as a default when no file - for the demo
        if (spEditor->GetBuffers().size() == 0)
        {
            ZepBuffer* pBuffer = spEditor->GetEmptyBuffer("shader.vert");
            pBuffer->SetText(shader.c_str());

        }
    }

    ZepContainer()
    {
        spEditor->UnRegisterCallback(this);
    }

    // Inherited via IZepComponent
    virtual void Notify(std::shared_ptr<ZepMessage> message) override
    {
        if (message->messageId == Msg::Quit)
        {
            quit = true;
        }
        else if (message->messageId == Msg::ToolTip)
        {
            auto spTipMsg = std::static_pointer_cast<ToolTipMessage>(message);
            if (spTipMsg->location != -1l && spTipMsg->pBuffer)
            {
                auto pSyntax = spTipMsg->pBuffer->GetSyntax();
                if (pSyntax)
                {
                    if (pSyntax->GetSyntaxAt(spTipMsg->location) == ThemeColor::Identifier)
                    {
                        auto spMarker = std::make_shared<RangeMarker>();
                        spMarker->description = "This is an identifier";
                        spMarker->highlightColor = ThemeColor::Identifier;
                        spMarker->textColor = ThemeColor::Text;
                        spTipMsg->spMarker = spMarker;
                        spTipMsg->handled = true;
                    }
                    else if (pSyntax->GetSyntaxAt(spTipMsg->location) == ThemeColor::Keyword)
                    {
                        auto spMarker = std::make_shared<RangeMarker>();
                        spMarker->description = "This is a keyword";
                        spMarker->highlightColor = ThemeColor::Keyword;
                        spMarker->textColor = ThemeColor::Text;
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
};

int main(int argc, char** argv)
{
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
    SDL_Window* window = SDL_CreateWindow("Zep", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(0); // Enable vsync

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

    // Setup style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given fixed_size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);

    //io.Fonts->AddFontDefault();
    /*ImFontConfig cfg;
    cfg.OversampleH = 3;
    cfg.OversampleV= 3;
    cfg.SizePixels = 16;
    cfg.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF((std::string(SDL_GetBasePath()) + "ProggyClean.ttf").c_str(), 16.0f, &cfg );
    */
    bool show_demo_window = false;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // ** Zep specific code
    ZepContainer zep(startupFile);

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
            if (ImGui::BeginMenu("Settings"))
            {
                if (ImGui::BeginMenu("Editor Mode"))
                {
                    bool enabledVim = strcmp(zep.GetEditor().GetCurrentMode()->Name(), Zep::ZepMode_Vim::StaticName()) == 0;
                    bool enabledNormal = !enabledVim;
                    if (ImGui::MenuItem("Vim", "CTRL+2", &enabledVim))
                    {
                        zep.GetEditor().SetMode(Zep::ZepMode_Vim::StaticName());
                    }
                    else if (ImGui::MenuItem("Standard", "CTRL+1", &enabledNormal))
                    {
                        zep.GetEditor().SetMode(Zep::ZepMode_Standard::StaticName());
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
                    pTabWindow->AddWindow(&pTabWindow->GetActiveWindow()->GetBuffer(), pTabWindow->GetActiveWindow(), false);
                }
                else if (ImGui::MenuItem("Vertical Split"))
                {
                    pTabWindow->AddWindow(&pTabWindow->GetActiveWindow()->GetBuffer(), pTabWindow->GetActiveWindow(), true);
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
                    strval << p.first << " : " << p.second.current / 1000.0 << "ms";// << " Last: " << p.second.current / 1000.0 << "ms";
                    ImGui::MenuItem(strval.str().c_str());
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        // This is a bit messy; and I have no idea why I don't need to remove the menu fixed_size from the calculation!
        auto menuSize = ImGui::GetStyle().FramePadding.y * 2 + ImGui::GetFontSize();
        ImGui::SetNextWindowPos(ImVec2(0, menuSize));
        ImGui::SetNextWindowSize(ImVec2(float(w), float(h))); // -menuSize)));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::Begin("Zep", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar /*| ImGuiWindowFlags_NoScrollbar*/);
        ImGui::PopStyleVar(4);

        // TODO: Change only when necessray
        zep.spEditor->SetDisplayRegion(toNVec2f(ImGui::GetWindowPos()), toNVec2f(ImGui::GetWindowSize()));

        // Display the editor inside this window
        zep.spEditor->Display();
        zep.spEditor->HandleInput();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        ImGui::End();

        // Rendering
        ImGui::Render();
        SDL_GL_MakeCurrent(window, gl_context);
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
