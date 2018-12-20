#pragma once

#include "buffer.h"

namespace Zep
{

namespace SyntaxType
{
enum
{
    Normal = (1 << 0),
    Keyword = (1 << 1),
    Integer = (1 << 2),
    Comment = (1 << 3),
    Whitespace = (1 << 4),
    HiddenChar = (1 << 5),
    Parenthesis = (1 << 6)
};
}

struct CommentEntry
{
    bool isStart;
    bool isMultiLine;
    uint32_t location;
    uint32_t entries;
};

class ZepSyntaxAdorn;
class ZepSyntax : public ZepComponent
{
public:
    ZepSyntax(ZepBuffer& buffer);
    virtual ~ZepSyntax();

    virtual uint32_t GetSyntaxAt(long index) const;
    virtual NVec4f GetSyntaxColorAt(long offset) const;
    virtual void UpdateSyntax();
    virtual void Interrupt();

    virtual long GetProcessedChar() const
    {
        return m_processedChar;
    }
    virtual const std::vector<uint32_t>& GetText() const
    {
        return m_syntax;
    }
    virtual void Notify(std::shared_ptr<ZepMessage> payload) override;

private:
    virtual void QueueUpdateSyntax(BufferLocation startLocation, BufferLocation endLocation);

protected:
    ZepBuffer& m_buffer;
    std::vector<CommentEntry> m_commentEntries;
    std::vector<uint32_t> m_syntax; // TODO: Use gap buffer - not sure why this is a vector?
    std::future<void> m_syntaxResult;
    std::atomic<long> m_processedChar = { 0 };
    std::atomic<long> m_targetChar = { 0 };
    std::vector<uint32_t> m_multiCommentStarts;
    std::vector<uint32_t> m_multiCommentEnds;
    std::set<std::string> keywords;
    std::atomic<bool> m_stop;
    std::vector<std::shared_ptr<ZepSyntaxAdorn>> m_adornments;
};

class ZepSyntaxAdorn : public ZepComponent
{
public:
    ZepSyntaxAdorn(ZepSyntax& syntax, ZepBuffer& buffer)
        : ZepComponent(syntax.GetEditor()),
        m_buffer(buffer)
    {}

    virtual NVec4f GetSyntaxColorAt(long offset, bool& found) const = 0;

protected:
    ZepBuffer& m_buffer;
};

} // namespace Zep
