#pragma once

#include <cassert>
#include <functional>
#include <map>
#include <set>

namespace Mgfx
{

using ComponentId = uint64_t;
struct Component;

struct Entity
{
    uint64_t id = 0;
    uint64_t flags = 0; // flags
    std::string name; // Optional name
    std::unordered_map<uint64_t, Component*> components;
    uint64_t componentMask = 0;

    // Mapping from entity ID to index
    static std::unordered_map<uint64_t, Entity*> entityFromId;
};

// Entities
void entity_init();
void entity_destroy();
void entity_destroy(Entity* e);
Entity* entity_create(uint64_t flags = 0, const char* pszName = nullptr);
void entities_each(std::function<void(Entity*)> fn);
bool entity_valid(uint64_t id);
Entity* entity_from_id(uint64_t id);

enum class EntityEvent : uint32_t
{
    Edited
};
using tEntityCB = std::function<void(Entity*, EntityEvent)>;

// Components
struct Component 
{
    virtual bool ShowUI() = 0;
    virtual void Destroy() {}
    virtual uint64_t Id() const = 0;
    virtual const char* Name() const = 0;
    Entity* owner;
};

std::vector<Entity*> entities_with_components(ComponentId ids);
void entities_with_components(ComponentId ids, std::function<bool(Entity*)> fn);
void entities_with_flags(uint64_t ids, std::function<bool(Entity*)> fn);
bool entity_has_component(Entity* entity, ComponentId id);

void component_register(ComponentId id, const char* serialName, std::function<Component*(Entity*)> factory);
Component* component_get(Entity* e, ComponentId id);
Component* component_add(Entity* e, ComponentId id);
void components_all(Entity* entity, std::function<void(Component*)> fn);
void component_destroy(Entity* e, ComponentId id);

// Template helpers
template<class T> void component_register() { component_register(T::CID, T::StaticName(), T::FactoryFunction); }
template<class T> T* component_add(Entity* e) { return (T*)component_add(e, T::CID); }
template<class T> T* component_get(Entity* entity) { return (T*)component_get(entity, T::CID); }
template<class T> void component_destroy(Entity* entity) { component_destroy(entity, T::CID); }
template<class T> bool entity_has_component(Entity* entity) { return entity_has_component(entity, T::CID); }

using TimeDelta = float;

#define COMPONENT(C)                            \
virtual const char* Name() const override { return #C; };      \
virtual uint64_t Id() const override { return C::CID; } \
static uint64_t CID;                            \
static const char* StaticName() { return #C; }   \
static Component* FactoryFunction(Entity* entity) \
{                                                   \
    auto pComp = new C();                           \
    pComp->owner = entity;                          \
    entity->components[CID] = pComp;               \
    entity->componentMask |= CID;                   \
    return pComp;                                   \
}

#define COMPONENT_NO_SERIALIZE(Type)              \
    virtual void Serialize(IArchive& ar) override \
    {                                             \
        return;                                   \
    }
#define COMPONENT_NO_UI(Type)                   \
    virtual bool ShowUI() override \
    {                                           \
        return;                                 \
    }

#ifdef _DEBUG
#define ASSERT_COMPONENT(en, co) assert(entity_has_component<co>(en) && "Entity missing component: " #co);
#else
#define ASSERT_COMPONENT(e, c) 
#endif

} // namespace Mgfx
