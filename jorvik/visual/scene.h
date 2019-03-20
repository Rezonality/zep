#pragma once
#include <future>

#include "opus.h"
#include "utils/file/file.h"
#include "compile.h"
#include "opus_asset_base.h"

namespace cpptoml
{
class table;
}

namespace Mgfx
{

class ShaderFileAsset;
class SceneDescriptionFileAsset;
class PassRenderState;
class RenderGraph;
class RenderNode;

// Gathered state required to setup for rendering
struct PassState
{
    ShaderFileAsset* vertex_shader;
    ShaderFileAsset* pixel_shader;
    std::shared_ptr<PassRenderState> render_state;
};

// A Scene class which is responsible for building up everything required to draw the scene
// and converting it to a SceneGraph, which does the actual drawing
// Some of the objects may be precompiled and progress through the steps required will be rapid
// This class is more of a coordinator, which collects finished assets.
class Scene : public IAssetWatch
{
public:
    Scene(const fs::path& path, Opus* pOpus);

    // As threaded compile steps complete, the scene progresses through these steps to build
    // the necessary state
    void BuildSceneStep();
    void BuildPassStep();
    void BuildSceneGraphStep();

    virtual RenderGraph* GetGraph() const;

private:
    // All the passes
    std::vector<std::shared_ptr<PassState>> m_passes;

    // The description asset, loaded from the toml file
    SceneDescriptionFileAsset* m_spSceneDescription;

    Opus* m_pOpus;

    std::shared_ptr<RenderGraph> m_spGraph;
    std::shared_ptr<RenderGraph> m_spLastGraph;
};

} // namespace Mgfx
