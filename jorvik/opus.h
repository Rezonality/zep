#pragma once

#include <memory>
#include <map>
#include "utils/file/file.h"
#include "jorvik.h"

namespace Mgfx
{

class Scene;
class OpusAsset;

// An opus.  Which is itself an asset
class Opus
{
public:
    Opus(const fs::path& path);

    static std::shared_ptr<Opus> LoadOpus(const fs::path& path);
    static std::shared_ptr<Opus> MakeDefaultOpus();

    virtual void Init();
    Scene* GetScene() const;

    void Register(const fs::path& path, std::shared_ptr<OpusAsset> pAsset);
    void UnRegister(const fs::path& path, std::shared_ptr<OpusAsset> pAsset);
    OpusAsset* GetFromPath(const fs::path& path);
    void ForEachFileAsset(std::function<void(OpusAsset*)> fnCB);

private:
    std::map<fs::path, std::shared_ptr<OpusAsset>> AssetFiles;
    std::shared_ptr<Scene> m_spScene;
    fs::path m_root;
};

template<class T>
T* FindOrCreate(const fs::path& path)
{
    auto pAsset = jorvik.spOpus->GetFromPath(path);
    if (pAsset)
    {
        return dynamic_cast<T*>(pAsset);
    }
    auto pNew = std::make_shared<T>(path);
    jorvik.spOpus->Register(path, pNew);
    return dynamic_cast<T*>(pNew.get());
}

}
