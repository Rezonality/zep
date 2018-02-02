#pragma once

#include "buffer.h"

namespace Zep
{

enum class SyntaxType
{
    Normal,
    Keyword,
    Integer,
    Comment,
    Whitespace
};

class ZepSyntax : public ZepComponent
{
public:
    ZepSyntax(ZepBuffer& buffer);
    virtual ~ZepSyntax();

    virtual uint32_t GetColor(long index) const = 0;
    virtual SyntaxType GetType(long index) const = 0;
    virtual void Interrupt() = 0;
    virtual void UpdateSyntax() = 0;

    virtual long GetProcessedChar() const { return m_processedChar; }
    virtual const std::vector<SyntaxType>& GetText() const { return m_syntax; }
    virtual void Notify(std::shared_ptr<ZepMessage> payload) override;

private:
    virtual void QueueUpdateSyntax(BufferLocation startLocation, BufferLocation endLocation);

protected:
    ZepBuffer& m_buffer;
    std::vector<SyntaxType> m_syntax;       // TODO: Use gap buffer - not sure why this is a vector?
    std::future<void> m_syntaxResult;
    std::atomic<long> m_processedChar = {0};
    std::atomic<long> m_targetChar = { 0 };
};
} // Zep
