#pragma once

#include "compile.h"
#include "utils/file/file.h"
#include <future>
#include <map>
#include <memory>

namespace Mgfx
{

struct IAssetWatch
{
};

class OpusAsset : public ICompile
{
public:
    OpusAsset(const fs::path& path);
    OpusAsset();
    virtual ~OpusAsset();
    virtual fs::path GetSourcePath() override;
    virtual void ProcessCompileResult(std::shared_ptr<CompileResult> spResult) override;
    virtual std::shared_ptr<CompileResult> GetCompileResult() const override;

    using fnChanged = std::function<void(OpusAsset*)>;
    void RegisterCompiledMessage(IAssetWatch* pOwner, fnChanged fn);
    void NotifyCompiled();

protected:
    fs::path m_path;
    std::shared_ptr<CompileResult> m_spResult;
    std::map<IAssetWatch*, fnChanged> m_watchers;
};

} // namespace Mgfx
