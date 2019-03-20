#pragma

#include <memory>

namespace Mgfx
{

class PassRenderState;
class RenderGraph;

// A Node in the rendergraph
class RenderNode
{
public:
    RenderNode(RenderGraph& graph, std::shared_ptr<PassRenderState> spState);
    virtual ~RenderNode();

    PassRenderState* GetRenderState()
    {
        return m_spState.get();
    }

private:
    std::shared_ptr<PassRenderState> m_spState;
    RenderGraph& m_graph;
};

} // Mgfx
