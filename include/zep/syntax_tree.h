#pragma once

#include "syntax.h"

namespace Zep
{

class ZepSyntax_Tree : public ZepSyntax
{
public:
    ZepSyntax_Tree(ZepBuffer& buffer,
        const std::set<std::string>& keywords = std::set<std::string>{},
        const std::set<std::string>& identifiers = std::set<std::string>{},
        uint32_t flags = 0);

    virtual void UpdateSyntax() override;
};

} // namespace Zep
