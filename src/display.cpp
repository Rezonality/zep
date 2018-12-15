#include "display.h"
#include "syntax.h"
#include "buffer.h"
#include "window.h"
#include "tab_window.h"
#include "editor.h"

#include "utils/stringutils.h"
#include "utils/timer.h"

namespace Zep
{

namespace
{
const uint32_t Color_CursorNormal = 0xEEF35FBC;
const uint32_t Color_CursorInsert = 0xFFFFFFFF;
//const float TabSize = 20.0f;
}


} // Zep