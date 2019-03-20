#include "render_graph.h"
#include "render_node.h"
#include "scene.h"
#include "pass_renderstate.h"

namespace Mgfx
{

RenderGraph::RenderGraph(Scene* scene)
{
}

RenderGraph::~RenderGraph()
{
}

void RenderGraph::ForEachNode(std::function<void(RenderNode*)> fnCb)
{
    for (auto& node : m_nodes)
    {
        fnCb(node.get());
    }
}

void RenderGraph::AddPass(std::shared_ptr<PassRenderState> spState)
{
    m_nodes.push_back(std::make_shared<RenderNode>(*this, spState));
}

} // Mgfx
