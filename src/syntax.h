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
    Whitespace = (1 << 4)
};
}

struct CommentEntry
{
    bool isStart;
    bool isMultiLine;
    uint32_t location;
    uint32_t entries;
};

class ZepSyntax : public ZepComponent
{
public:
    ZepSyntax(ZepBuffer& buffer);
    virtual ~ZepSyntax();

    virtual uint32_t GetColor(long index) const = 0;
    virtual uint32_t GetType(long index) const = 0;
    virtual void Interrupt() = 0;
    virtual void UpdateSyntax() = 0;

    virtual long GetProcessedChar() const { return m_processedChar; }
    virtual const std::vector<uint32_t>& GetText() const { return m_syntax; }
    virtual void Notify(std::shared_ptr<ZepMessage> payload) override;

private:
    virtual void QueueUpdateSyntax(BufferLocation startLocation, BufferLocation endLocation);

protected:
    ZepBuffer& m_buffer;
    std::vector<CommentEntry> m_commentEntries;
    std::vector<uint32_t> m_syntax;       // TODO: Use gap buffer - not sure why this is a vector?
    std::future<void> m_syntaxResult;
    std::atomic<long> m_processedChar = {0};
    std::atomic<long> m_targetChar = { 0 };
};
} // Zep
