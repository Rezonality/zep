#pragma once
#include "utils/file/file.h"
#include "opus.h"
#include "compile.h"
#include "opus_asset_base.h"

namespace cpptoml
{
class table;
}

namespace Mgfx
{

struct SceneDescCompileResult : CompileResult
{
    std::shared_ptr<cpptoml::table> spTable;
};

// Represents the scene description file
class SceneDescriptionFileAsset : public OpusAsset
{
public:
    SceneDescriptionFileAsset(const fs::path& path)
        : OpusAsset(path)
    {
        compile_queue(this);
    }
    
    // Compile a path to an OpusAsset data
    virtual std::future<std::shared_ptr<CompileResult>> Compile() override;
};

}
