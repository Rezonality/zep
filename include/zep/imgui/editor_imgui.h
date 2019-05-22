#pragma once
#include <string>

#include "zep/editor.h"

namespace Zep
{

class ZepDisplay_ImGui;
class ZepTabWindow;
class ZepEditor_ImGui : public ZepEditor
{
public:
    ZepEditor_ImGui(const ZepPath& rootPath, uint32_t flags = 0, IZepFileSystem* pFileSystem = nullptr);
    void HandleInput();

private:
};

} // namespace Zep
