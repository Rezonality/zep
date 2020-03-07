#pragma once

#include "zep/syntax.h"

namespace Zep
{

class ZepSyntax_Orca : public ZepSyntax
{
public:
    ZepSyntax_Orca(ZepBuffer& buffer,
        const std::unordered_set<std::string>& keywords = std::unordered_set<std::string>{},
        const std::unordered_set<std::string>& identifiers = std::unordered_set<std::string>{},
        uint32_t flags = 0);

    virtual void UpdateSyntax() override;
    virtual SyntaxResult GetSyntaxAt(long index) const override;
};

} // namespace Zep
