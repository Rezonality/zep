#pragma once

#include "opus_asset_base.h"

namespace Mgfx
{

struct PassState;

// Represents the state required to render a pass; including targets, shaders, geometry, etc.
// The data returned in the compile result is what the device needs to draw
class PassRenderState : public OpusAsset
{
public:
    PassRenderState(PassState* pass);
    virtual ~PassRenderState();

    // Compile a path to an OpusAsset data
    virtual std::future<std::shared_ptr<CompileResult>> Compile() override;

private:
    PassState* pPass;
};

}
