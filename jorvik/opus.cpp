#include "threadpool/threadpool.h"

#include "utils/file/runtree.h"
#include "utils/logger.h"
#include "utils/callback.h"
#include "utils/threadutils.h"

#include "jorvik.h"
#include "opus.h"
#include "compile.h"
#include "editor.h"

#include "visual/scene.h"
#include "visual/IDevice.h"

namespace Mgfx
{

Opus::Opus(const fs::path& root)
{
    // Now we have an opus, compile the scene
    m_root = root;
}

std::shared_ptr<Opus> Opus::LoadOpus(const fs::path& path)
{
    if (!fs::exists(path))
    {
        return nullptr;
    }
    return std::make_shared<Opus>(path);
}

void Opus::Init()
{
    m_spScene = std::make_shared<Scene>(m_root / "visual" / "scene.toml", this);
}

std::shared_ptr<Opus> Opus::MakeDefaultOpus()
{
    // If not found, copy our template into a folder in documents
    auto source = runtree_path() / "opus" / "default_dx12";
    auto target = file_documents_path() / "jorvik" / "opus";

    fs::create_directories(target);

    uint32_t opus_count = 0;
    std::string target_file = "opus_";
    while (fs::exists(target / (target_file + std::to_string(opus_count))))
    {
        opus_count++;
        if (opus_count > 10000)
            break;
    }

    auto target_folder = target / (target_file + std::to_string(opus_count));
    fs::create_directories(target_folder);

    fs::copy(source, target_folder, fs::copy_options::recursive);

    return LoadOpus(target_folder);
}

Scene* Opus::GetScene() const
{
    return m_spScene.get();
}

void Opus::Register(const fs::path& path, std::shared_ptr<OpusAsset> pAsset)
{
    AssetFiles[path] = pAsset;

    // TODO: Send message
    editor_update_assets();
}

void Opus::UnRegister(const fs::path& path, std::shared_ptr<OpusAsset> pAsset)
{
    AssetFiles.erase(path);
}

void Opus::ForEachFileAsset(std::function<void(OpusAsset*)> fnCB)
{
    for (auto& file : AssetFiles)
    {
        fnCB(file.second.get());
    }
}

OpusAsset* Opus::GetFromPath(const fs::path& path)
{
    auto itr = AssetFiles.find(path);
    if (itr != AssetFiles.end())
    {
        return itr->second.get();
    }
    return nullptr;
}
} // namespace Mgfx
