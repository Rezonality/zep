// ImGui - standalone example application for SDL2 + OpenGL
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
#include <imgui.h>
#include "imgui/examples/sdl_opengl3_example/imgui_impl_sdl_gl3.h"
#include <stdio.h>
#include <imgui/examples/libs/gl3w/GL/gl3w.h>    // This example is using gl3w to access OpenGL functions (because it is small). You may use glew/glad/glLoadGen/etc. whatever already works for you.
#include <SDL.h>
#include "utils/timer.h"

#undef max

#include "src/imgui/editor_imgui.h"
#include "src/imgui/display_imgui.h"

using namespace Zep;

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

int main(int, char**)
{
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Setup window
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
    SDL_Window *window = SDL_CreateWindow("Zep Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    SDL_GLContext glcontext = SDL_GL_CreateContext(window);
    gl3wInit();

    SDL_GL_SetSwapInterval(1);

    // Setup ImGui binding
    ImGui_ImplSdlGL3_Init(window);

    static const ImWchar ranges[] =
    {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0,
    };

    // Less packed font in X
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 1;
    config.DstFont = ImGui::GetFont();
    config.GlyphExtraSpacing.x = 1.0f;
    io.Fonts->AddFontFromFileTTF((std::string(SDL_GetBasePath()) + "/ProggyClean.ttf").c_str(), 13, &config, ranges);

    bool show_test_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Create an editor
    auto spEditor = std::make_unique<ZepEditor_ImGui>();
   
    // Add a shader
    ZepBuffer* pBuffer = spEditor->AddBuffer("shader.vert");
    pBuffer->SetText(shader.c_str());

    // Main loop
    bool done = false;
    Timer lastChange;
    lastChange.Restart();
    while (!done)
    {
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 50))
        {
            ImGui_ImplSdlGL3_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
        }
        else
        {
            // Save battery by skipping display if not required.
            // This will check for cursor flash, for example, to keep that updated.
            if (!spEditor->GetDisplay()->RefreshRequired())
            {
                continue;
            }
        }

        ImGui_ImplSdlGL3_NewFrame(window);

        ImGui::Begin("Zep", nullptr, ImVec2(500, 500));

        // Display the editor inside this window
        spEditor->Display(toNVec2f(ImGui::GetCursorScreenPos()), toNVec2f(ImGui::GetContentRegionAvail()));

        ImGui::End();

        // Rendering
        glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplSdlGL3_Shutdown();
    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
