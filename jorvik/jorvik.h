#pragma once

#include <memory>
#include <file/file.h>
#include <functional>
#include <set>
#include "m3rdparty/threadpool/threadpool.h"

namespace Mgfx
{

class Opus;
struct IDevice;

// Anyone can send messages like this
struct JorvikMessage
{
    virtual ~JorvikMessage() {};
};
using fnMessage = std::function<void(std::shared_ptr<JorvikMessage> msg)>;

struct Jorvik
{
    bool done = false;
    std::unique_ptr<IDevice> spDevice;
    std::unique_ptr<ThreadPool> spThreadPool = std::make_unique<ThreadPool>();

    fs::path roamingPath;
    bool forceReset = false;
    bool vulkan = false;

    std::shared_ptr<Opus> spOpus;
    std::vector<fnMessage> listeners;
    
    uint32_t startWidth;
    uint32_t startHeight;

    float time;
};
extern Jorvik jorvik;

void jorvik_destroy();
void jorvik_init();
void jorvik_init_settings();
void jorvik_tick(float dt);
void jorvik_render(float dt);
bool jorvik_refresh_required();
void jorvik_add_listener(fnMessage fn);
void jorvik_send_message(std::shared_ptr<JorvikMessage> msg);
std::string jorvik_get_text(const fs::path& path);

} // Mgfx