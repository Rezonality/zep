#include "utils.h"

#include <cassert>
#include <string>
#include <unordered_set>

#include "entities/entities.h"
#include "registrar.h"
#include "string/stringutils.h"

namespace Mgfx
{

// The next Entity to be created will have this ID
uint64_t currentEntityId = 1;

// All Entities
std::unordered_set<Entity*> entities;
 
// All Components
std::map<uint64_t, std::unordered_set<Component*>> AllComponents;

// Decl
std::unordered_map<uint64_t, Entity*> Entity::entityFromId;

// Component Data 
struct StaticComponentData
{
    std::string serialName;
    uint64_t componentId;
    std::function<Component*(Entity*)> factory;
};
std::unordered_map<uint64_t, std::shared_ptr<StaticComponentData>> mapComponentIdToStaticData;

// Initialize the entity library
void entity_init()
{
    assert(entities.empty());

    // 0 entity is 'invalid'
    auto pEntity = new Entity();
    entities.insert(pEntity);
    pEntity->id = currentEntityId++;
    pEntity->name = "nullEntity";

    // Fast lookup from ID
    Entity::entityFromId[pEntity->id] = pEntity;
}

void entity_destroy()
{
    while (!entities.empty())
    {
        entity_destroy(*entities.begin());
    }
    assert(entities.empty());
}

// Does it have a component by ID?
bool entity_has_component(Entity* e, ComponentId componentId)
{
    return (e->componentMask & componentId) != 0ull ? true : false;
}

void entity_destroy(Entity* e)
{
    // Ignore the null
    if (e == nullptr)
    {
        return;
    }

    while (!e->components.empty())
    {
        component_destroy(e, e->components.begin()->first);
    }
    assert(e->componentMask == 0);
    Entity::entityFromId.erase(e->id);
    entities.erase(e);
    delete e;
}

std::string Entity_MakeName(Entity* pEntity, const char* name)
{
    if (name == nullptr)
    {
        return std::string("entity_") + std::to_string(pEntity->id);
    }
    return std::string(name);
}

bool entity_valid(uint64_t id)
{
    return Entity::entityFromId.find(id) != Entity::entityFromId.end();
}

Entity* entity_from_id(uint64_t id)
{
    auto itr = Entity::entityFromId.find(id);
    if (itr != Entity::entityFromId.end())
    {
        return itr->second;
    }
    return nullptr;
}

// Create an entity
Entity* entity_create(uint64_t flags, const char* name)
{
    // Make a new ID....
    while (entity_valid(currentEntityId))
    {
        currentEntityId++;
    }

    auto pEntity = new Entity();
    entities.insert(pEntity);
    pEntity->id = currentEntityId++;
    pEntity->flags = flags;
    pEntity->name = Entity_MakeName(pEntity, name);

    return pEntity;
}

void entities_each(std::function<void(Entity*)> fn)
{
    for (auto& e : entities)
    {
        fn(e);
    }
}

std::vector<Entity*> entities_with_components(ComponentId ids)
{
    // Find the first component that we have
    std::vector<Entity*> ret;
    for (int64_t c = 0; c < 63; c++)
    {
        auto componentID = 1ull << c;
        if (ids & componentID)
        {
            // Now look at all component of this type
            auto& all = AllComponents[componentID];
            for (auto& pComponent : all)
            {
                // Check all entities to see if they have all the required components
                auto& entity = pComponent->owner;
                if ((entity->componentMask & ids) == ids)
                {
                    ret.push_back(entity);
                }
            }
            break;
        }
    }
    return ret;
}

void entities_with_components(ComponentId ids, std::function<bool(Entity*)> fn)
{
    std::vector<Entity*> ret;
    for (int64_t c = 0; c < 63; c++)
    {
        auto componentID = 1ull << c;
        if (ids & componentID)
        {
            // All components of this found type
            auto& all = AllComponents[componentID];
            for (auto& pComponent : all)
            {
                // Check all entities to see if they have all the required components
                auto& entity = pComponent->owner;
                if ((entity->componentMask & ids) == ids)
                {
                    if (!fn(entity))
                    {
                        return;
                    }
                }
            }
        }
    }
}

void entities_with_flags(uint64_t ids, std::function<bool(Entity*)> fn)
{
    for (auto& entity : entities)
    {
        if ((entity->flags & ids) == ids)
        {
            if (!fn(entity))
            {
                return;
            }
        }
    }
}

void components_all(Entity* entity, std::function<void(Component*)> fn)
{
    for (int64_t c = 0; c < 63; c++)
    {
        auto pComponent = component_get(entity, 1ull << c);
        if (pComponent)
        {
            fn(pComponent);
        }
    }
}

// Component Register
void component_register(ComponentId id, const char* serialName, std::function<Component*(Entity*)> factory)
{
    auto pData = std::make_shared<StaticComponentData>();
    pData->serialName = string_tolower(std::string(serialName));
    pData->componentId = id;
    pData->factory = factory;
    mapComponentIdToStaticData[id] = pData;
}

const StaticComponentData* component_static_from_id(ComponentId id)
{
    auto itr = mapComponentIdToStaticData.find(id);
    if (itr != mapComponentIdToStaticData.end())
    {
        return itr->second.get();
    }
    return nullptr;
}

const StaticComponentData* component_static_from_name(const std::string& serialName)
{
    for (auto& c : mapComponentIdToStaticData)
    {
        if (c.second->serialName == serialName)
        {
            return c.second.get();
        }
    }
    return nullptr;
}

Component* component_add(Entity* e, ComponentId id)
{
    auto pStatic = component_static_from_id(id);
    if (!pStatic)
        return nullptr;

    if (entity_has_component(e, id))
    {
        assert(!"Already has component?");
        return component_get(e, id);
    }

    auto pComponent = pStatic->factory(e);

    AllComponents[id].insert(pComponent);
    return pComponent;
}

void component_destroy(Entity* e, ComponentId id)
{
    auto itr = e->components.find(id);
    if (itr != e->components.end())
    {
        AllComponents[id].erase(itr->second);
        itr->second->Destroy();
        delete itr->second;
        e->components.erase(itr);
        e->componentMask &= ~id;
    }
}

Component* component_get(Entity* e, ComponentId Id)
{
    auto itr = e->components.find(Id);
    if (itr != e->components.end())
    {
        return itr->second;
    }
    return nullptr;
}

} // namespace Mgfx
