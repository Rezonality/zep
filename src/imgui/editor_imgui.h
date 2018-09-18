#pragma once
#include <string>
#include "editor.h"

namespace Zep
{

class ZepDisplay_ImGui;
class ZepTabWindow;
class ZepEditor_ImGui : public ZepEditor
{
public:
    ZepEditor_ImGui();
    void Display(const NVec2f& pos, const NVec2f& size);

    ZepDisplay* GetDisplay() const { return (ZepDisplay*)m_spDisplay.get(); }

private: 
    void HandleInput();

    std::shared_ptr<ZepDisplay_ImGui> m_spDisplay;
};

} // Zep
