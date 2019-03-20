#include "syntax_rainbow_brackets.h"
#include "mcommon/string/stringutils.h"
#include "theme.h"
#include "mcommon/logger.h"

// A Simple adornment to add rainbow brackets to the syntax
namespace Zep
{

ZepSyntaxAdorn_RainbowBrackets::ZepSyntaxAdorn_RainbowBrackets(ZepSyntax& syntax, ZepBuffer& buffer)
    : ZepSyntaxAdorn(syntax, buffer)
{
    syntax.GetEditor().RegisterCallback(this);
}

ZepSyntaxAdorn_RainbowBrackets::~ZepSyntaxAdorn_RainbowBrackets()
{
}

void ZepSyntaxAdorn_RainbowBrackets::Notify(std::shared_ptr<ZepMessage> spMsg)
{
    // Handle any interesting buffer messages
    if (spMsg->messageId == Msg::Buffer)
    {
        auto spBufferMsg = std::static_pointer_cast<BufferMessage>(spMsg);
        if (spBufferMsg->pBuffer != &m_buffer)
        {
            return;
        }
        else if (spBufferMsg->type == BufferMessageType::TextDeleted)
        {
            Clear(spBufferMsg->startLocation, spBufferMsg->endLocation);
        }
        else if (spBufferMsg->type == BufferMessageType::TextAdded)
        {
            Insert(spBufferMsg->startLocation, spBufferMsg->endLocation);
            Update(spBufferMsg->startLocation, spBufferMsg->endLocation);
        }
        else if (spBufferMsg->type == BufferMessageType::TextChanged)
        {
            Update(spBufferMsg->startLocation, spBufferMsg->endLocation);
        }
    }
}

NVec4f ZepSyntaxAdorn_RainbowBrackets::GetSyntaxColorAt(long offset, bool& found) const
{
    auto itr = m_brackets.find(offset);
    if (itr == m_brackets.end())
    {
        found = false;
        return NVec4f(1.0f);
    }

    found = true;
    if (itr->second.indent < 0)
    {
        return m_buffer.GetTheme().GetColor(ThemeColor::HiddenText);
    }
    return m_buffer.GetTheme().GetUniqueColor(itr->second.indent);
}

void ZepSyntaxAdorn_RainbowBrackets::Insert(long start, long end)
{
    // Adjust all the brackets after us by the same distance
    auto diff = end - start;
    std::map<BufferLocation, Bracket> replace;
    for (auto& b : m_brackets)
    {
        if (b.first < start)
            replace[b.first] = b.second;
        else
            replace[b.first + diff] = b.second;
    }
    std::swap(m_brackets, replace);

    RefreshBrackets();
}

void ZepSyntaxAdorn_RainbowBrackets::Clear(long start, long end)
{
    // Remove brackets in the erased section
    for (auto current = start; current < end; current++)
    {
        m_brackets.erase(BufferLocation(current));
    }

    // Adjust remaining brackets by the difference
    auto diff = end - start;
    std::map<BufferLocation, Bracket> replace;
    for (auto& b : m_brackets)
    {
        if (b.first < start)
            replace[b.first] = b.second;
        else
            replace[b.first - diff] = b.second;
    }
    std::swap(m_brackets, replace);

    RefreshBrackets();
}

void ZepSyntaxAdorn_RainbowBrackets::Update(long start, long end)
{
    auto& buffer = m_buffer.GetText();
    auto itrStart = buffer.begin() + start;
    auto itrEnd = buffer.begin() + end;

    for (auto itrBracket = itrStart; itrBracket != itrEnd; itrBracket++)
    {
        auto offset = itrBracket - buffer.begin();
        if (*itrBracket == '(')
        {
            m_brackets[BufferLocation(offset)] = Bracket{0, BracketType::Bracket, true};
        }
        else if (*itrBracket == ')')
        {
            m_brackets[BufferLocation(offset)] = Bracket{0, BracketType::Bracket, false};
        }
        else if (*itrBracket == '[')
        {
            m_brackets[BufferLocation(offset)] = Bracket{0, BracketType::Group, true};
        }
        else if (*itrBracket == ']')
        {
            m_brackets[BufferLocation(offset)] = Bracket{0, BracketType::Group, false};
        }
        else if (*itrBracket == '{')
        {
            m_brackets[BufferLocation(offset)] = Bracket{0, BracketType::Brace, true};
        }
        else if (*itrBracket == '}')
        {
            m_brackets[BufferLocation(offset)] = Bracket{0, BracketType::Brace, false};
        }
        else
        {
            auto itr = m_brackets.find(BufferLocation(offset));
            if (itr != std::end(m_brackets))
            {
                m_brackets.erase(itr);
            }
        }
    }
    RefreshBrackets();
}

void ZepSyntaxAdorn_RainbowBrackets::RefreshBrackets()
{
    std::vector<int32_t> indents((int)BracketType::Max, 0);
    for (auto& b : m_brackets)
    {
        auto& bracket = b.second;
        if (!bracket.is_open)
        {
            indents[int(bracket.type)]--;
        }
        bracket.indent = indents[int(bracket.type)];
        // Allow one bracket error, before going back to normal
        if (indents[int(bracket.type)] < 0)
        {
            indents[int(bracket.type)] = 0;
        }
        if (bracket.is_open)
        {
            indents[int(bracket.type)]++;
        }
    }
}

} // namespace Zep
