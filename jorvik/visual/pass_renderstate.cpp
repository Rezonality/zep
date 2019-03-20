#include "pass_renderstate.h"
#include "jorvik.h"
#include "visual/scene.h"
#include "visual/IDevice.h"

namespace Mgfx
{

PassRenderState::PassRenderState(PassState* p)
    : OpusAsset(),
    pPass(p)
{
    // Instantly queue this object for compile
    compile_queue(this);
}

PassRenderState::~PassRenderState()
{
}

// Compile a path to an OpusAsset data
std::future<std::shared_ptr<CompileResult>> PassRenderState::Compile()
{
    return jorvik.spDevice->CompilePass(pPass);
}

}