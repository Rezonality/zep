#include "threadpool/threadpool.h"
#include "cpptoml/include/cpptoml.h"

#include "utils/logger.h"
#include "utils/file/file.h"

#include "editor.h"

#include "opus.h"
#include "opus_asset_base.h"

#include "visual/shader_file_asset.h"
#include "visual/pass_renderstate.h"
#include "visual/scene_description_file_asset.h"
#include "visual/render_graph.h"
#include "visual/scene.h"


namespace Mgfx
{

Scene::Scene(const fs::path& path, Opus* pOpus)
    : m_pOpus(pOpus)
{
    m_spSceneDescription = FindOrCreate<SceneDescriptionFileAsset>(path);
    m_spSceneDescription->RegisterCompiledMessage(this, [&](OpusAsset* pAsset) {
        BuildSceneStep();
    });
}

void Scene::BuildPassStep()
{
    for (auto& pass : m_passes)
    {
        if (!pass->vertex_shader || !pass->pixel_shader)
        {
            return;
        }

        auto spVertexResult = pass->vertex_shader->GetCompileResult();
        auto spPixelResult = pass->pixel_shader->GetCompileResult();
        if (spVertexResult == nullptr ||
            spVertexResult->state == CompileState::Invalid ||
            spPixelResult == nullptr ||
            spPixelResult->state == CompileState::Invalid)
        {
            return;
        }

        // OK, we are good to go
        pass->render_state = std::make_shared<PassRenderState>(pass.get());
        pass->render_state->RegisterCompiledMessage(this, [&](OpusAsset * pAsset)
        {
            BuildSceneGraphStep();
        });
    }
}

RenderGraph* Scene::GetGraph() const
{
    return m_spGraph.get();
}

void Scene::BuildSceneGraphStep()
{
    auto spGraph = std::make_shared<RenderGraph>(this);
    for (auto& pass : m_passes)
    {
        spGraph->AddPass(pass->render_state);
    }

    // Swap out the graph and keep the last one in case it is still in the pipe
    // This is necessary, because we can't throw away PSO's, etc. while the GPU is using them in DX12.
    // TODO: Is it OK just to keep the last iteration?  What if 2 compiles happen very quickly together?
    // There's a frame count in the graph which is designed to solve this problem if it exists...
    m_spLastGraph = m_spGraph;
    m_spGraph = spGraph;
}

void Scene::BuildSceneStep()
{
    auto spResult = m_spSceneDescription->GetCompileResult();
    if (!spResult || spResult->state == CompileState::Invalid)
    {
        return;
    }

    auto spSceneDescResult = std::static_pointer_cast<SceneDescCompileResult>(spResult);
    auto spTable = spSceneDescResult->spTable;
    auto passTables = spTable->get_table_array("pass");

    m_passes.clear();

    for (auto& passTable : *passTables)
    {
        auto spPass = std::make_shared<PassState>();

        auto sceneDir = spSceneDescResult->path.parent_path();

        // Pull out the text field for our shaders and build the pass
        auto vs_name = passTable->get_as<std::string>("vertex_shader").value_or("");
        if (!vs_name.empty())
        {
            spPass->vertex_shader = FindOrCreate<ShaderFileAsset>(sceneDir / vs_name);
            spPass->vertex_shader->RegisterCompiledMessage(this, [&](OpusAsset* pFile) {
                BuildPassStep();
            });
        }

        auto ps_name = passTable->get_as<std::string>("pixel_shader").value_or("");
        if (!ps_name.empty())
        {
            spPass->pixel_shader = FindOrCreate<ShaderFileAsset>(sceneDir / ps_name);
            spPass->pixel_shader->RegisterCompiledMessage(this, [&](OpusAsset* pFile) {
                BuildPassStep();
            });
        }

        //geometry = std::make_shared<GeometryAsset>(GeometryType::FSQuad, this);

        m_passes.push_back(spPass);
    }
}

}; // namespace Mgfx
