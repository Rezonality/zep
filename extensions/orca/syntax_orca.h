#pragma once

#include "zep/syntax.h"

namespace Zep
{

class ZepSyntax_Orca : public ZepSyntax
{
public:
    ZepSyntax_Orca(ZepBuffer& buffer,
        const std::set<std::string>& keywords = std::set<std::string>{},
        const std::set<std::string>& identifiers = std::set<std::string>{},
        uint32_t flags = 0);

    virtual void UpdateSyntax() override;
    virtual SyntaxData GetSyntaxAt(long index) const override;
};

} // namespace Zep
