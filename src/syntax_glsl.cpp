#include "syntax_glsl.h"
#include "utils/stringutils.h"

namespace
{
}

namespace Zep
{

ZepSyntaxGlsl::ZepSyntaxGlsl(ZepBuffer& buffer)
    : TParent(buffer),
    m_stop(false)
{
    keywords.insert({ "float", "vec2", "vec3", "vec4", "int", "uint", "mat2", "mat3", "mat4", "mat" });
    keywords.insert({ "uniform", "layout", "location", "void", "out", "in" });
    keywords.insert({ "#version", "core" });
    keywords.insert({ "sampler1D", "sampler2D", "sampler3D" });
    keywords.insert({ "pow", "sin", "cos", "mul", "abs", "floor", "ceil" });
}

ZepSyntaxGlsl::~ZepSyntaxGlsl()
{
    Interrupt();
}

uint32_t ZepSyntaxGlsl::GetType(long offset) const
{
    if (m_processedChar < offset ||
        m_syntax.size() <= offset)
    {
        return SyntaxType::Normal;
    }

    return m_syntax[offset];
}

uint32_t ZepSyntaxGlsl::GetColor(long offset) const
{
    if (m_processedChar < offset ||
        m_syntax.size() <= offset)
    {
        return 0xFFFFFFFF;
    }

    switch (m_syntax[offset])
    {
    default:
    case SyntaxType::Normal:
        return 0xFFFFFFFF;
    case SyntaxType::Comment:
        return 0xFF00FF11;
    case SyntaxType::Keyword:
        return 0xFFFFFF11;
    case SyntaxType::Integer:
        return 0xFF11FFFF;
    case SyntaxType::Whitespace:
        return 0xFF223322;
    }
}

void ZepSyntaxGlsl::Interrupt()
{
    // Stop the thread, wait for it
    m_stop = true;
    if (m_syntaxResult.valid())
    {
        m_syntaxResult.get();
    }
    m_stop = false;
}


// TODO: We can be more optimal about redoing syntax highlighting if we know we have done it before
// and we have changed some text 'in the middle'.  When we get to a stable syntax state, we don't have to keep going
void ZepSyntaxGlsl::UpdateSyntax()
{
    auto& buffer = m_buffer.GetText();
    auto itrCurrent = buffer.begin() + m_processedChar;
    auto itrEnd = buffer.begin() + m_targetChar;

    assert(std::distance(itrCurrent, itrEnd) < int(m_syntax.size()));

    std::string delim(" \t.\n;(){}=");
    std::string lineEnd("\n");

    // Walk backwards to previous delimiter
    while (itrCurrent > buffer.begin())
    {
        if (std::find(delim.begin(), delim.end(), *itrCurrent) == delim.end())
        {
            itrCurrent--;
        }
        else
        {
            break;
        }
    }

    itrEnd = buffer.find_first_of(itrEnd, buffer.end(), lineEnd.begin(), lineEnd.end());

    // Mark a region of the syntax buffer with the correct marker
    auto mark = [&](GapBuffer<utf8>::const_iterator itrA, GapBuffer<utf8>::const_iterator itrB, uint32_t type)
    {
        std::fill(m_syntax.begin() + (itrA - buffer.begin()), m_syntax.begin() + (itrB - buffer.begin()), type);
    };

    auto markSingle = [&](GapBuffer<utf8>::const_iterator itrA, uint32_t type)
    {
        *(m_syntax.begin() + (itrA - buffer.begin())) = type;
    };

    // Update start location
    m_processedChar = long(itrCurrent - buffer.begin());

    // Walk the buffer updating information about syntax coloring
    while (itrCurrent != itrEnd)
    {
        if (m_stop == true)
        {
            return;
        }

        // Find a token, skipping delim <itrFirst, itrLast>
        auto itrFirst = buffer.find_first_not_of(itrCurrent, buffer.end(), delim.begin(), delim.end());
        if (itrFirst == buffer.end())
            break;

        auto itrLast = buffer.find_first_of(itrFirst, buffer.end(), delim.begin(), delim.end());

        // Ensure we found a token
        assert(itrLast >= itrFirst);

        // Do I need to make a string here?
        auto token = std::string(itrFirst, itrLast);
        if (keywords.find(token) != keywords.end())
        {
            mark(itrFirst, itrLast, SyntaxType::Keyword);
        }
        else if (token.find_first_not_of("0123456789") == std::string::npos)
        {
            mark(itrFirst, itrLast, SyntaxType::Integer);
        }
        else
        {
            mark(itrFirst, itrLast, SyntaxType::Normal);
        }

        std::string commentStr = "/";
        auto itrComment = buffer.find_first_of(itrFirst, itrLast, commentStr.begin(), commentStr.end());
        if (itrComment != buffer.end())
        {
            auto itrCommentStart = itrComment++;
            if (itrComment < buffer.end())
            {
                if (*itrComment == '/')
                {
                    itrLast = buffer.find_first_of(itrCommentStart, buffer.end(), lineEnd.begin(), lineEnd.end());
                    mark(itrCommentStart, itrLast, SyntaxType::Comment);
                }
            }
        }
        itrCurrent = itrLast;
    }

    // If we got here, we sucessfully completed
    // Reset the target to the beginning
    m_targetChar = long(0);
    m_processedChar = long(buffer.size() - 1);
}

} // Zep
