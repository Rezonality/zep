#pragma once
// ImGui SDL2 binding with OpenGL3
// In this binding, ImTextureID is used to store an OpenGL 'GLuint' texture identifier. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

struct SDL_Window;
typedef union SDL_Event SDL_Event;
#include <cstdint>

#include "imgui.h"
namespace Mgfx
{

// Data
class ImGuiSDL_GL3
{
public:
    bool        Init(SDL_Window* window);
    void        Shutdown();
    void        NewFrame(SDL_Window* window);

    // Use if you want to reset your rendering device without losing ImGui state.
    void        InvalidateDeviceObjects();

    void        RenderDrawLists(ImDrawData* draw_data);

private:
    void        CreateFontsTexture();
    bool        CreateDeviceObjects();

private:
    ImGuiContext* m_pContext = nullptr;
    uint32_t     m_fontTexture = 0;
    int          m_shaderHandle = 0;
    int          m_vertHandle = 0;
    int          m_fragHandle = 0;
    int          m_attribLocationTex = 0;
    int          m_attribLocationProjMtx = 0;
    int          m_attribLocationPosition = 0;
    int          m_attribLocationUV = 0;
    int          m_attribLocationColor = 0;
    unsigned int m_vboHandle = 0;
    unsigned int m_vaoHandle = 0;
    unsigned int m_elementsHandle = 0;
};

} // namespace Mgfx