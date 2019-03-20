#include "threadpool/threadpool.h"
#include "cpptoml/include/cpptoml.h"

#include "utils/logger.h"
#include "utils/string/tomlutils.h"

#include "visual/scene_description_file_asset.h"

namespace Mgfx
{

std::future<std::shared_ptr<CompileResult>> SceneDescriptionFileAsset::Compile()
{
    auto text = jorvik_get_text(m_path);
    sanitize_for_toml(text);
    auto p = m_path;

    // Immediately return.  Nothing doing here yet
    return jorvik.spThreadPool->enqueue([=]() -> std::shared_ptr<CompileResult>
    {
        auto spCompileResult = std::make_shared<SceneDescCompileResult>();
        spCompileResult->path = p;

        try
        {
            std::istringstream is(text, std::ios_base::binary);
            cpptoml::parser r{is};
            auto spTable = r.parse();
            spCompileResult->spTable = spTable;
            spCompileResult->state = CompileState::Valid;
        }
        catch (cpptoml::parse_exception& ex)
        {
            auto spMessage = std::make_shared<CompileMessage>(CompileMessageType::Error, p, ex.what(), extract_toml_error_line(ex.what()));
            spCompileResult->messages.push_back(spMessage);
            spCompileResult->state = CompileState::Invalid;
        }
        catch (...)
        {
            auto spMessage = std::make_shared<CompileMessage>(CompileMessageType::Error, p);
            spCompileResult->messages.push_back(spMessage);
            spCompileResult->state = CompileState::Invalid;
        }
    
        return spCompileResult;
    });
}

} // Mgfx
