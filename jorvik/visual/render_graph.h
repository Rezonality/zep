#pragma

#include <vector>
#include <memory>
#include <functional>

namespace Mgfx
{

class Scene;
class RenderNode;
class PassRenderState;

// A Rendergraph is a descriptionn of how the scene should be drawn
// Work in progress.
// Ideally it will manage target dependencies and do other nice things with the knowledge of the scene
// it has
// Graphs are generated clean each time - essentially read only; and backed up in case the current graph is 
// still in flight
class RenderGraph
{
public:
    RenderGraph(Scene* scene);
    virtual ~RenderGraph();

    // Add a pass as a node
    virtual void AddPass(std::shared_ptr<PassRenderState> node);

    // Scene methods
    virtual void ForEachNode(std::function<void(RenderNode*)>);

private:
    std::vector<std::shared_ptr<RenderNode>> m_nodes;
    Scene* m_pScene = nullptr;
    uint64_t m_currentFrame = 0;
};

} // Mgfx