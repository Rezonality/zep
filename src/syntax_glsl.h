#pragma once
#include <string>
#include "syntax.h"

namespace Zep
{

class ZepSyntaxGlsl : public ZepSyntax
{
public:
    using TParent = ZepSyntax;
    ZepSyntaxGlsl(ZepBuffer& buffer);
    virtual ~ZepSyntaxGlsl();
};

} // Zep
