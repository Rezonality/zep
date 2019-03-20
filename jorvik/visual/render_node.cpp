#include "render_node.h"
#include "render_graph.h"

namespace Mgfx
{

RenderNode::RenderNode(RenderGraph& graph, std::shared_ptr<PassRenderState> spState)
    : m_graph(graph),
    m_spState(spState)
{
}
    
RenderNode::~RenderNode()
{
}

} // Mgfx
