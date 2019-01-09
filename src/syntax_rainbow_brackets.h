#pragma once
#include "syntax.h"
#include <list>
#include <string>
#include <unordered_map>

namespace Zep
{

class ZepSyntaxAdorn_RainbowBrackets : public ZepSyntaxAdorn
{
public:
    using TParent = ZepSyntaxAdorn;
    ZepSyntaxAdorn_RainbowBrackets(ZepSyntax& syntax, ZepBuffer& buffer);
    virtual ~ZepSyntaxAdorn_RainbowBrackets();

    void Notify(std::shared_ptr<ZepMessage> payload) override;
    virtual NVec4f GetSyntaxColorAt(long offset, bool& found) const override;

    virtual void Clear(long start, long end);
    virtual void Insert(long start, long end);
    virtual void Update(long start, long end);

private:
    void RefreshBrackets();
    enum class BracketType
    {
        Bracket = 0,
        Brace = 1,
        Group = 2,
        Max = 3
    };

    struct Bracket
    {
        int32_t indent;
        BracketType type;
        bool is_open;
    };
    std::map<BufferLocation, Bracket> m_brackets;
};

} // namespace Zep
