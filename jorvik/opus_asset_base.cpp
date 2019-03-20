#include "utils/file/runtree.h"
#include "utils/callback.h"

#include "opus_asset_base.h"
#include "compile.h"

namespace Mgfx
{

OpusAsset::OpusAsset()
    : m_path(fs::path())
{
}

OpusAsset::OpusAsset(const fs::path& path)
    : m_path(path)
{
}

OpusAsset::~OpusAsset()
{
    compile_cancel(this);
}


fs::path OpusAsset::GetSourcePath()
{
    return m_path;
}

void OpusAsset::ProcessCompileResult(std::shared_ptr<CompileResult> spResult)
{
    m_spResult = spResult;
    NotifyCompiled();
}

std::shared_ptr<CompileResult> OpusAsset::GetCompileResult() const
{
    return m_spResult;
}

void OpusAsset::NotifyCompiled()
{
    for (auto& watch : m_watchers)
    {
        watch.second(this);
    }
}

void OpusAsset::RegisterCompiledMessage(IAssetWatch* pOwner, fnChanged fn)
{
    auto itrWatcher = m_watchers.find(pOwner);
    if (itrWatcher == m_watchers.end())
    {
        m_watchers[pOwner] = fn;
    }
    else
    {
        itrWatcher->second = fn;
    }
}

/*
void OpusAsset::NotifyChanged(const fs::path& path)
{
    auto itr = AssetFiles.find(path);
    if (itr == AssetFiles.end())
    {
        return;
    }

    for (auto pCB : FileCallbacks[path])
    {
        pCB->OnAssetChanged(itr->second.get());
    }
}
*/

} // namespace Mgfx
