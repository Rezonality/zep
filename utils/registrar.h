#pragma once

#include <unordered_map>
#include <typeinfo>
#include <typeindex>
#include <memory>

namespace Mgfx
{

    /*
template<class T>
class Registrar
{
public:
    using Collection = std::unordered_map<std::type_index, std::shared_ptr<T>>;

    T* Get()
    {
        return dynamic_cast<P*>(items[typeid(T)].get());
    };

    template <typename P, typename... Args>
    void Register(Args&&... args)
    {
        items[typeid(P)] = std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <typename P>
    void each(std::function<void(P* pSystem)> f)
    {
        for (auto& sys : items)
        {
            T* pSys = dynamic_cast<P*>(sys.second.get());
            if (pSys)
            {
                f(pSys);
            }
        }
    }

    const Collection& Getitems() const
    {
        return items;
    }

private:
    Collection items;
};
*/

} // Mgfx
