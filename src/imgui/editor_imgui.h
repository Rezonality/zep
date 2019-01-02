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
    void HandleInput();

private:
};

} // namespace Zep
