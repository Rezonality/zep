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

    virtual void UpdateSyntax() override;

    virtual uint32_t GetColor(long i) const override;
    virtual SyntaxType GetType(long i) const override;
    virtual void Interrupt() override;

    std::set<std::string> keywords;
    std::atomic<bool> m_stop;
};

} // Zep
