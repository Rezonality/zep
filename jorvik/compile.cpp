
#include <future>
#include <map>

#include "compile.h"
#include "logger.h"
#include "jorvik.h"
#include "visual/IDevice.h"
#include "opus.h"
#include "opus_asset_base.h"

namespace Mgfx
{

namespace
{

// An asset that is waiting for a compile result
struct PendingCompile
{
    std::future<std::shared_ptr<CompileResult>> compileFuture;
    ICompile* spAsset;
    bool pendingFuture = false;
};

std::map<ICompile*, std::shared_ptr<PendingCompile>> PendingCompiles;

}

// Remove an asset from our queue, or wait for it to finish and ignore the result
void compile_cancel(ICompile* pAsset)
{
    auto itr = PendingCompiles.find(pAsset);
    if (itr != PendingCompiles.end())
    {
        // If it was already compiling, then wait for it
        if (itr->second->pendingFuture)
        {
            itr->second->compileFuture.wait();
        }
        // And erase
        PendingCompiles.erase(itr);
    }
}

// Queue an asset for compile
void compile_queue(ICompile* asset)
{
    if (asset == nullptr)
    {
        return;
    }

    // If already compiling, let it finish 
    std::shared_ptr<PendingCompile> spPending;
    auto itrPending = PendingCompiles.find(asset);
    if (itrPending != PendingCompiles.end())
    {
        spPending = itrPending->second;
        if (itrPending->second->pendingFuture)
        {
            LOG(DEBUG) << "Compilation already in progress, waiting for: " << asset->GetSourcePath().string();
            itrPending->second->compileFuture.wait();
        }
    }
    else
    {
        spPending = std::make_shared<PendingCompile>();
        PendingCompiles[asset] = spPending;
    }
    spPending->spAsset = asset;
    spPending->pendingFuture = true;

    spPending->compileFuture = asset->Compile();
    assert(spPending->compileFuture.valid());
}

// Check pending compiles for any that are finished
bool compile_tick()
{
    bool didWork = false;
    std::set<ICompile*> victims;
    for (auto itrCompile : PendingCompiles)
    {
        auto pSched = itrCompile.second;
        if (pSched->pendingFuture && pSched->compileFuture.valid() &&
            pSched->compileFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            didWork = true;

            auto spResult = pSched->compileFuture.get();

            std::ostringstream str;
            //str << "Compilation complete: " << itrCompile.first->GetName() << "State: " << (spResult->state == CompileState::Valid ? "Valid" : "Invalid");
            if (!itrCompile.first->GetSourcePath().empty())
            {
                str << ", path: " << itrCompile.first->GetSourcePath().string();
            }
            LOG(DEBUG) << str.str();

            pSched->spAsset->ProcessCompileResult(spResult);

            // Tell everyone about the compile result.
            jorvik_send_message(std::dynamic_pointer_cast<JorvikMessage>(std::make_shared<CompilerMessage>(itrCompile.first)));

            victims.insert(itrCompile.first);

            pSched->pendingFuture = false;
        }
    }
    return didWork;
}

}
