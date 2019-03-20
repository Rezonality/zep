#include "threadpool/threadpool.h"

#include "utils/logger.h"
#include "utils/threadutils.h"

#include "visual/IDevice.h"
#include "visual/geometry_file_asset.h"

#include "editor.h"

namespace Mgfx
{

std::future<std::shared_ptr<CompileResult>> GeometryFileAsset::Compile()
{
    return make_ready_future<std::shared_ptr<CompileResult>>(std::make_shared<CompileResult>());
}

} // Mgfx
