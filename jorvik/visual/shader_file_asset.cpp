#include "threadpool/threadpool.h"

#include "utils/logger.h"

#include "visual/IDevice.h"
#include "visual/shader_file_asset.h"

namespace Mgfx
{

std::future<std::shared_ptr<CompileResult>> ShaderFileAsset::Compile()
{
    // Ask the editor for the text, return the compilation object
    return jorvik.spDevice->CompileShader(m_path, jorvik_get_text(m_path));
}

} // Mgfx
