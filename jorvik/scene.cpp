#include "scene.h"
#include "cpptoml/include/cpptoml.h"
#include "utils/logger.h"
#include "utils/string/stringutils.h"
#include "threadpool/threadpool.h"
#include "device/IDevice.h"

#include "editor.h"

namespace Mgfx
{

std::future<std::shared_ptr<CompileResult>> SceneAsset::Compile()
{
    // Get editor text on main thread...
    auto p = path;
    auto text = editor_get_text(p);

    // Ensure a trailing '\n' for the text; since the cpptoml compiler is a bit picky
    text = string_right_trim(text, "\0");
    text += "\n";

    return jorvik.spThreadPool->enqueue([=]() -> std::shared_ptr<CompileResult>
    {
        auto spCompileResult = std::make_shared<SceneCompileResult>();
        spCompileResult->path = p;

        try
        {
            // Read the scene description and parse it on a thread
            std::istringstream is(text, std::ios_base::binary);

            cpptoml::parser r{is};
            auto spTable = r.parse();
            spCompileResult->spTable = spTable;

            LOG(INFO) << "Scene Table: \n" << *spTable;

        }
        catch (cpptoml::parse_exception& ex)
        {
            auto spMessage = std::make_shared<CompileMessage>(CompileMessageType::Error, p, ex.what(), extract_toml_error_line(ex.what()));
            spCompileResult->messages.push_back(spMessage);
        }
        catch (...)
        {
            auto spMessage = std::make_shared<CompileMessage>();
            spMessage->filePath = p;
            spMessage->msgType = CompileErrorType::Error;
            spMessage->text = "Unknown error compiling: " + p.string();
            spCompileResult->messages.push_back(spMessage);
            LOG(ERROR) << "Unknown exception during compile";
        }
    
        return spCompileResult;
    });
}

void SceneAsset::Apply(std::shared_ptr<CompileResult> spResult)
{
    LOG(INFO) << "SceneAsset::Apply: " << path.string();

    OpusAssetBase::Apply(spResult);

    spTable = std::static_pointer_cast<SceneCompileResult>(spCompileResult)->spTable;

    auto sceneParent = path.parent_path();

    // Scene results contain shaders, load and queue them up for compile
    if (spTable)
    {
        // TODO: Clear the dependency chain
        passes.clear();

        auto passTables = spTable->get_table_array("pass");
        for (auto& passTable : *passTables)
        {
            // TODO: Can the pass depend on the scene, and recompile after it is loaded.
            auto spPass = std::make_shared<Pass>();
            auto vertex_shader = passTable->get_as<std::string>("vertex_shader").value_or("");
            if (!vertex_shader.empty())
            {
                spPass->vertex_shader = std::make_shared<ShaderAsset>(sceneParent / vertex_shader);
                spPass->vertex_shader->DependsOn(shared_from_this());
            }

            auto pixel_shader = passTable->get_as<std::string>("pixel_shader").value_or("");
            if (!pixel_shader.empty())
            {
                spPass->pixel_shader = std::make_shared<ShaderAsset>(sceneParent / pixel_shader);
                spPass->pixel_shader->DependsOn(shared_from_this());
            }

            passes.push_back(spPass);
        }
    }
}

std::future<std::shared_ptr<CompileResult>> GeometryAsset::Compile()
{
    LOG(INFO) << "GeometryAsset::Compile: " << path.string();

    auto p = path;
    
    // Immediately return.  Nothing doing here yet
    return jorvik.spThreadPool->enqueue([p]()
    {
        auto spCompileResult = std::make_shared<CompileResult>();
        spCompileResult->path = p;
        return spCompileResult;
    });
}

void GeometryAsset::Apply(std::shared_ptr<CompileResult> spResult)
{
    OpusAssetBase::Apply(spResult);

    LOG(INFO) << "GeometryAsset::Apply: " << path.string();
}

std::future<std::shared_ptr<CompileResult>> ShaderAsset::Compile()
{
    LOG(INFO) << "ShaderAsset::Compile: " << path.string();
    return jorvik.spDevice->CompileShader(path, editor_get_text(path));
}

void ShaderAsset::Apply(std::shared_ptr<CompileResult> spResult)
{
    OpusAssetBase::Apply(spResult);

    LOG(INFO) << "ShaderAsset::Apply: " << path.string();
    spCompileResult = spResult;
}
};
