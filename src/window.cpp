#include <cctype>
#include <cmath>
#include <sstream>

#include "buffer.h"
#include "display.h"
#include "mode.h"
#include "syntax.h"
#include "tab_window.h"
#include "theme.h"
#include "window.h"
#include "scroller.h"

#include "mcommon/logger.h"
#include "mcommon/string/stringutils.h"

// A 'window' is like a vim window; i.e. a region inside a tab
namespace Zep
{

// UTF8 Not supported yet.
//#define UTF8_CHAR_LEN(byte) ((0xE5000000 >> ((byte >> 3) & 0x1e)) & 3) + 1
#define UTF8_CHAR_LEN(byte) 1

const float ScrollBarSize = 17.0f;

ZepWindow::ZepWindow(ZepTabWindow& window, ZepBuffer* buffer)
    : ZepComponent(window.GetEditor())
    , m_pBuffer(buffer)
    , m_tabWindow(window)
{
    m_bufferRegion = std::make_shared<Region>();
    m_numberRegion = std::make_shared<Region>();
    m_indicatorRegion = std::make_shared<Region>();
    m_textRegion = std::make_shared<Region>();
    m_airlineRegion = std::make_shared<Region>();
    m_vScrollRegion = std::make_shared<Region>();

    m_bufferRegion->flags = RegionFlags::Expanding;
    m_bufferRegion->vertical = false;

    m_numberRegion->flags = RegionFlags::Fixed;
    m_indicatorRegion->flags = RegionFlags::Fixed;
    m_vScrollRegion->flags = RegionFlags::Fixed;
    m_textRegion->flags = RegionFlags::Expanding;
    m_airlineRegion->flags = RegionFlags::Fixed;

    auto pHorzRegion = std::make_shared<Region>();
    pHorzRegion->flags = RegionFlags::Expanding;
    pHorzRegion->vertical = true;

    m_bufferRegion->children.push_back(pHorzRegion);
    pHorzRegion->children.push_back(m_numberRegion);
    pHorzRegion->children.push_back(m_indicatorRegion);
    pHorzRegion->children.push_back(m_textRegion);
    pHorzRegion->children.push_back(m_vScrollRegion);

    m_bufferRegion->children.push_back(m_airlineRegion);

    m_vScroller = std::make_shared<Scroller>();
    m_vScroller->vertical = false;
    Scroller_Init(*m_vScroller, GetEditor(), *m_vScrollRegion);
}

ZepWindow::~ZepWindow()
{
}

void ZepWindow::UpdateScrollers()
{
    m_scrollVisibilityChanged = false;

    // For now, scrollers are either on or off; and don't disappear
    auto old_percent = m_vScroller->vScrollVisiblePercent;
    if (m_maxDisplayLines == 0)
    {
        m_vScroller->vScrollVisiblePercent = 1.0f;
        m_scrollVisibilityChanged = (old_percent != m_vScroller->vScrollVisiblePercent);
        return;
    }
    m_vScroller->vScrollVisiblePercent = std::min(float(m_maxDisplayLines) / float(m_windowLines.size()), 1.0f);
    m_vScroller->vScrollPosition = std::abs(m_bufferOffsetYPx) / m_bufferSizeYPx;
      
    if (m_vScroller->vScrollVisiblePercent >= 1.0f)
    {
        m_vScrollRegion->fixed_size = NVec2f(0.0f, 0.0f);
    }
    else
    {
        m_vScrollRegion->fixed_size = NVec2f(ScrollBarSize * GetEditor().GetPixelScale(), 0.0f);
    }

    if (m_vScrollRegion->rect.Width() != m_vScrollRegion->fixed_size.x)
    {
        m_scrollVisibilityChanged = true;
    }
}

void ZepWindow::UpdateAirline()
{
    m_airline.leftBoxes.clear();
    m_airline.rightBoxes.clear();
    m_airline.leftBoxes.push_back(AirBox{GetEditor().GetCurrentMode()->Name(), m_pBuffer->GetTheme().GetColor(ThemeColor::Mode)});
    switch (GetEditor().GetCurrentMode()->GetEditorMode())
    {
        /*case EditorMode::Hidden:
        m_airline.leftBoxes.push_back(AirBox{ "HIDDEN", m_pBuffer->GetTheme().GetColor(ThemeColor::HiddenText) });
        break;
        */
        case EditorMode::Insert:
            m_airline.leftBoxes.push_back(AirBox{"INSERT", m_pBuffer->GetTheme().GetColor(ThemeColor::CursorInsert)});
            break;
        case EditorMode::Normal:
            m_airline.leftBoxes.push_back(AirBox{"NORMAL", m_pBuffer->GetTheme().GetColor(ThemeColor::CursorNormal)});
            break;
        case EditorMode::Visual:
            m_airline.leftBoxes.push_back(AirBox{"VISUAL", m_pBuffer->GetTheme().GetColor(ThemeColor::VisualSelectBackground)});
            break;
    };
    m_airline.leftBoxes.push_back(AirBox{m_pBuffer->GetDisplayName(), m_pBuffer->GetTheme().GetColor(ThemeColor::AirlineBackground)});
    m_airline.rightBoxes.push_back(AirBox{std::to_string(m_pBuffer->GetLineEnds().size()) + " Lines", m_pBuffer->GetTheme().GetColor(ThemeColor::LineNumberBackground)});
}

void ZepWindow::SetCursorType(CursorType mode)
{
    m_cursorType = mode;
    GetEditor().ResetCursorTimer();
}

void ZepWindow::Notify(std::shared_ptr<ZepMessage> payload)
{
    if (payload->messageId == Msg_Buffer)
    {
        auto pMsg = std::static_pointer_cast<BufferMessage>(payload);

        m_layoutDirty = true;
        if (pMsg->pBuffer != m_pBuffer)
        {
            return;
        }

        if (pMsg->type != BufferMessageType::PreBufferChange)
        {
            // Put the cursor where the replaced text was added
            GetEditor().ResetCursorTimer();
        }
    }
}

NVec2f ZepWindow::GetTextSize(const utf8* pCh, const utf8* pEnd)
{
    if (pEnd == nullptr)
    {
        pEnd = pCh + strlen((const char*)pCh);
    }
    if ((pEnd - pCh) == 1)
    {
        auto itr = m_mapCharSizes.find(*pCh);
        if (itr == end(m_mapCharSizes))
        {
            auto sz = GetEditor().GetDisplay().GetTextSize(pCh, pEnd);
            m_mapCharSizes[*pCh] = sz;
            return sz;
        }
        return itr->second;
    }
    return GetEditor().GetDisplay().GetTextSize(pCh, pEnd);
}

void ZepWindow::SetDisplayRegion(const NRectf& region)
{
    if (m_bufferRegion->rect == region)
    {
        return;
    }

    m_layoutDirty = true;
    m_bufferRegion->rect = region;

    m_airlineRegion->fixed_size = NVec2f(0.0f, GetEditor().GetDisplay().GetFontSize() + textBorder * 2.0f);

    // Border, and move the text across a bit
    auto textSize = GetEditor().GetDisplay().GetTextSize((utf8*)"A");
    m_numberRegion->fixed_size = NVec2f(float(leftBorderChars) * textSize.x, 0);

    m_indicatorRegion->fixed_size = NVec2f(textSize.x, 0.0f);

    m_defaultLineSize = GetEditor().GetDisplay().GetFontSize();
    m_bufferOffsetYPx = 0;
}

void ZepWindow::ScrollToCursor()
{
    auto old_offset = m_bufferOffsetYPx;
    auto two_lines = (GetEditor().GetDisplay().GetFontSize() * 2);
    auto& cursorLine = GetCursorLineInfo(BufferToDisplay().y);

    if (m_bufferOffsetYPx > (cursorLine.spanYPx - two_lines))
    {
        m_bufferOffsetYPx -= (m_bufferOffsetYPx - (cursorLine.spanYPx - two_lines));
    }
    else if ((m_bufferOffsetYPx + m_textRegion->rect.Height() - two_lines) < cursorLine.spanYPx)
    {
        m_bufferOffsetYPx += cursorLine.spanYPx - (m_bufferOffsetYPx + m_textRegion->rect.Height() - two_lines);
    }

    m_bufferOffsetYPx = std::min(m_bufferOffsetYPx, m_bufferSizeYPx - float(m_maxDisplayLines) * (two_lines * .5f));
    m_bufferOffsetYPx = std::max(0.f, m_bufferOffsetYPx);

    if (old_offset != m_bufferOffsetYPx)
    {
        UpdateVisibleLineRange();
    }
}

void ZepWindow::UpdateLineSpans()
{
    m_maxDisplayLines = (long)std::max(0.0f, std::floor(m_textRegion->rect.Height() / m_defaultLineSize));

    float screenPosX = m_textRegion->rect.topLeftPx.x + textBorder;

    BufferLocation loc = 0;

    // For now, we are compromising on ASCII; so don't query font fixed_size each time
    // The performance of the editor currently comes down to this function
    // It can be improved by incrementally updating the line spans, potentially threading, etc.
    // but it isn't necessary yet.
    const auto& textBuffer = m_pBuffer->GetText();

    long bufferLine = 0;
    long spanLine = 0;
    float bufferPosYPx = 0;

    auto ensureSpanLines = [&](long lines) {
        if (m_windowLines.size() <= lines)
        {
            m_windowLines.resize(lines + 1);
        }
    };

    // Process every buffer line
    for (;;)
    {
        // We haven't processed this line yet, so we can't display anything
        // else
        if (m_pBuffer->GetLineCount() <= bufferLine)
            break;

        NVec2i columnOffsets;
        if (!m_pBuffer->GetLineOffsets(bufferLine, columnOffsets.x, columnOffsets.y))
            break;

        SpanInfo* lineInfo = nullptr;

        // These offsets are 0 -> n + 1, i.e. the last offset the buffer returns is 1 beyond the current
        for (auto ch = columnOffsets.x; ch < columnOffsets.y; ch++)
        {
            const utf8* pCh = &textBuffer[ch];
            const auto textSize = GetTextSize(pCh, pCh + 1);

            // Wrap if we have displayed at least one char, and we have to
            if (m_wrap && ch != columnOffsets.x)
            {
                if (((screenPosX + textSize.x) + textSize.x) >= (m_textRegion->rect.bottomRightPx.x))
                {
                    // Remember the offset beyond the end of the line
                    lineInfo->columnOffsets.y = ch;

                    // Next line
                    spanLine++;
                    bufferPosYPx += GetEditor().GetDisplay().GetFontSize();

                    ensureSpanLines(spanLine);

                    lineInfo = &m_windowLines[spanLine];
                    lineInfo->charInfo.clear();

                    // Now jump to the next 'screen line' for the rest of this 'buffer line'
                    lineInfo->columnOffsets = NVec2i(ch, ch + 1);
                    lineInfo->lastNonCROffset = 0;
                    lineInfo->lineIndex = spanLine;
                    lineInfo->bufferLineNumber = bufferLine;
                    screenPosX = m_textRegion->rect.topLeftPx.x + textBorder;
                }
                else
                {
                    screenPosX += textSize.x;
                }
            }

            // Collect all display lines.
            // Lines before the first active line will have screen line number -1
            ensureSpanLines(spanLine);

            if (lineInfo == nullptr)
            {
                lineInfo = &m_windowLines[spanLine];
                lineInfo->bufferLineNumber = bufferLine;
                lineInfo->lineIndex = spanLine;
                lineInfo->columnOffsets.x = ch;
                lineInfo->charInfo.clear();
            }

            lineInfo->spanYPx = bufferPosYPx;
            lineInfo->columnOffsets.y = ch + 1;

            lineInfo->charInfo.push_back(CharInfo{});

            auto charIndex = lineInfo->charInfo.size() - 1;
            lineInfo->charInfo[charIndex].bufferLocation = ch;
            lineInfo->charInfo[charIndex].screenPosXPx = screenPosX;
            lineInfo->lastNonCROffset = std::max(ch, 0l);

            const utf8* pEnd = pCh + UTF8_CHAR_LEN(*pCh);
            lineInfo->charInfo[charIndex].bufferLocationEnd = UTF8_CHAR_LEN(*pCh);
            lineInfo->charInfo[charIndex].textSize = textSize;
        }

        // Next time round - down a buffer line, down a span line
        bufferLine++;
        spanLine++;
        screenPosX = m_textRegion->rect.topLeftPx.x + textBorder;
        bufferPosYPx += GetEditor().GetDisplay().GetFontSize();
        lineInfo = nullptr;
    }

    // Ensure we aren't too big
    m_windowLines.resize(spanLine);

    // Sanity
    if (m_windowLines.empty())
    {
        SpanInfo lineInfo;
        lineInfo.columnOffsets.x = 0;
        lineInfo.columnOffsets.y = 0;
        lineInfo.lastNonCROffset = 0;
        lineInfo.bufferLineNumber = 0;
        m_windowLines.push_back(lineInfo);
    }

    m_bufferSizeYPx = m_windowLines[m_windowLines.size() - 1].spanYPx + GetEditor().GetDisplay().GetFontSize();

    UpdateVisibleLineRange();
    m_layoutDirty = true;
}

void ZepWindow::UpdateVisibleLineRange()
{
    m_visibleLineRange.x = (long)m_windowLines.size();
    m_visibleLineRange.y = 0;
    for (long line = 0; line < long(m_windowLines.size()); line++)
    {
        auto& windowLine = m_windowLines[line];
        if ((windowLine.spanYPx + GetEditor().GetDisplay().GetFontSize()) <= m_bufferOffsetYPx)
        {
            continue;
        }

        if ((windowLine.spanYPx - m_bufferOffsetYPx) >= m_textRegion->rect.Height())
        {
            break;
        }

        m_visibleLineRange.x = std::min(m_visibleLineRange.x, long(line));
        m_visibleLineRange.y = long(line);
    }
    m_visibleLineRange.y++;

    UpdateScrollers();
    /*
    LOG(DEBUG) << "Line Range: " << std::to_string(m_visibleLineRange.x) + ", " + std::to_string(m_visibleLineRange.y);
    LOG(DEBUG) << "YStart: " << m_windowLines[m_visibleLineRange.x].spanYPx << ", BufferOffset " << m_bufferOffsetYPx << ", font: " << GetEditor().GetDisplay().GetFontSize();
    */
}

const SpanInfo& ZepWindow::GetCursorLineInfo(long y)
{
    UpdateLayout();
    y = std::max(0l, y);
    y = std::min(y, long(m_windowLines.size() - 1));
    return m_windowLines[y];
}

// Convert a normalized y coordinate to the window region
float ZepWindow::ToWindowY(float pos) const
{
    return pos - m_bufferOffsetYPx + m_bufferRegion->rect.topLeftPx.y;
}

// TODO: This function draws one char at a time.  It could be more optimal at the expense of some
// complexity.  Basically, I don't like the current implementation, but it works for now.
// The text is displayed acorrding to the region bounds and the display lineData
// Additionally (and perhaps that should be a seperate function), this code draws line numbers
bool ZepWindow::DisplayLine(const SpanInfo& lineInfo, const NRectf& region, int displayPass)
{
    // A middle-dot whitespace character
    static const auto whiteSpace = makeStr(std::wstring(L"\x00b7"));
    static const auto blankSpace = ' ';

    auto cursorCL = BufferToDisplay();

    GetEditor().GetDisplay().SetClipRect(m_bufferRegion->rect);

    // Draw line numbers
    auto showLineNumber = [&]() {
        if (!IsInsideTextRegion(NVec2i(0, lineInfo.lineIndex)))
            return;
        auto cursorBufferLine = GetCursorLineInfo(cursorCL.y).bufferLineNumber;
        std::string strNum;
        if (m_displayMode == DisplayMode::Vim)
        {
            strNum = std::to_string(abs(lineInfo.bufferLineNumber - cursorBufferLine));
        }
        else
        {
            strNum = std::to_string(lineInfo.bufferLineNumber);
        }
        auto textSize = GetEditor().GetDisplay().GetTextSize((const utf8*)strNum.c_str(), (const utf8*)(strNum.c_str() + strNum.size()));

        auto digitCol = m_pBuffer->GetTheme().GetColor(ThemeColor::LineNumber);
        if (lineInfo.BufferCursorInside(m_bufferCursor))
        {
            digitCol = m_pBuffer->GetTheme().GetColor(ThemeColor::CursorNormal);
        }

        // Numbers
        GetEditor().GetDisplay().DrawChars(NVec2f(m_numberRegion->rect.bottomRightPx.x - textSize.x - textBorder, ToWindowY(lineInfo.spanYPx)), digitCol, (const utf8*)strNum.c_str(), (const utf8*)(strNum.c_str() + strNum.size()));
    };

    if (displayPass == WindowPass::Background)
    {
        // Show any markers
        for (auto& marker : m_pBuffer->GetRangeMarkers())
        {
            auto sel = marker.range;
            if (lineInfo.columnOffsets.x <= sel.second && lineInfo.columnOffsets.y > sel.first)
            {
                GetEditor().GetDisplay().DrawRectFilled(NRectf(NVec2f(m_indicatorRegion->rect.topLeftPx.x, ToWindowY(lineInfo.spanYPx)), NVec2f(m_indicatorRegion->rect.Center().x, ToWindowY(lineInfo.spanYPx) + GetEditor().GetDisplay().GetFontSize())), m_pBuffer->GetTheme().GetColor(marker.color));
            }
        }
        showLineNumber();
    }

    bool foundCR = false;
    auto screenPosX = m_textRegion->rect.topLeftPx.x + textBorder;

    char invalidChar;

    // Walk from the start of the line to the end of the line (in buffer chars)
    for (auto ch = lineInfo.columnOffsets.x; ch < lineInfo.columnOffsets.y; ch++)
    {
        auto& info = lineInfo.charInfo[ch - lineInfo.columnOffsets.x];
        auto pSyntax = m_pBuffer->GetSyntax();
        auto col = pSyntax != nullptr ? pSyntax->GetSyntaxColorAt(info.bufferLocation) : m_pBuffer->GetTheme().GetColor(ThemeColor::Text);

        const utf8* pCh = &m_pBuffer->GetText()[info.bufferLocation];

        // Visible white space
        if (pSyntax && pSyntax->GetSyntaxAt(info.bufferLocation) == ThemeColor::Whitespace)
        {
            pCh = (const utf8*)whiteSpace.c_str();
        }

        // Shown only one char for end of line
        if (*pCh == '\n' || *pCh == 0)
        {
            invalidChar = '@' + *pCh;
            if (m_windowFlags & WindowFlags::ShowCR)
            {
                pCh = (const utf8*)&invalidChar;
            }
            else
            {
                pCh = (const utf8*)&blankSpace;
            }
            col = m_pBuffer->GetTheme().GetColor(ThemeColor::HiddenText);
        }
        const utf8* pEnd = pCh + UTF8_CHAR_LEN(*pCh);

        if (displayPass == 0)
        {
            if (IsActiveWindow())
            {
                if (GetEditor().GetCurrentMode()->GetEditorMode() == EditorMode::Visual)
                {
                    auto sel = m_pBuffer->GetSelection();
                    if (info.bufferLocation >= sel.first && info.bufferLocation < sel.second)
                    {
                        GetEditor().GetDisplay().DrawRectFilled(NRectf(NVec2f(screenPosX, ToWindowY(lineInfo.spanYPx)), NVec2f(screenPosX + info.textSize.x, ToWindowY(lineInfo.spanYPx) + info.textSize.y)), m_pBuffer->GetTheme().GetColor(ThemeColor::VisualSelectBackground));
                    }
                }
            }

            // Show any markers
            for (auto& marker : m_pBuffer->GetRangeMarkers())
            {
                auto sel = marker.range;
                if (info.bufferLocation >= sel.first && info.bufferLocation < sel.second)
                {
                    GetEditor().GetDisplay().DrawRectFilled(NRectf(NVec2f(screenPosX, ToWindowY(lineInfo.spanYPx) + info.textSize.y - 1), NVec2f(screenPosX + info.textSize.x, ToWindowY(lineInfo.spanYPx) + info.textSize.y)), m_pBuffer->GetTheme().GetColor(marker.color));
                }
            }
        }
        else
        {
            if (pSyntax && pSyntax->GetSyntaxAt(info.bufferLocation) == ThemeColor::Whitespace)
            {
                auto centerChar = NVec2f(screenPosX + info.textSize.x / 2, ToWindowY(lineInfo.spanYPx) + info.textSize.y / 2);
                GetEditor().GetDisplay().DrawRectFilled(NRectf(centerChar - NVec2f(1.0f, 1.0f), centerChar + NVec2f(1.0f, 1.0f)), m_pBuffer->GetTheme().GetColor(ThemeColor::Whitespace));
            }
            else
            {
                GetEditor().GetDisplay().DrawChars(NVec2f(screenPosX, ToWindowY(lineInfo.spanYPx)), col, pCh, pEnd);
            }
        }

        screenPosX += info.textSize.x;
    }

    DisplayCursor();

    GetEditor().GetDisplay().SetClipRect(NRectf{});

    return true;
} // namespace Zep

bool ZepWindow::IsInsideTextRegion(NVec2i pos) const
{
    if (pos.y < m_visibleLineRange.x || pos.y >= m_visibleLineRange.y)
    {
        return false;
    }
    return true;
}

void ZepWindow::DisplayCursor()
{
    if (!IsActiveWindow())
        return;

    auto cursorCL = BufferToDisplay();

    // Draw the cursor
    auto cursorBufferLine = GetCursorLineInfo(cursorCL.y);

    if (!IsInsideTextRegion(cursorCL))
    {
        return;
    }

    CharInfo* pCharInfo = nullptr;
    CharInfo lastPos;
    if (cursorCL.x < cursorBufferLine.charInfo.size())
    {
        pCharInfo = &cursorBufferLine.charInfo[cursorCL.x];
    }
    else
    {
        if (!cursorBufferLine.charInfo.empty())
        {
            lastPos = cursorBufferLine.charInfo[cursorBufferLine.charInfo.size() - 1];
            lastPos.screenPosXPx += lastPos.textSize.x;
        }
        else
        {
            lastPos.screenPosXPx = 0;
        }
        lastPos.textSize = GetEditor().GetDisplay().GetTextSize((Zep::utf8*)"A");
        pCharInfo = &lastPos;
    }

    // Draw the Cursor symbol
    auto cursorPosPx = NVec2f(pCharInfo->screenPosXPx, cursorBufferLine.spanYPx - m_bufferOffsetYPx + m_textRegion->rect.topLeftPx.y);
    auto cursorBlink = GetEditor().GetCursorBlinkState();
    if (!cursorBlink)
    {
        switch (m_cursorType)
        {
            default:
            case CursorType::Hidden:
                break;

            case CursorType::Insert:
            {
                GetEditor().GetDisplay().DrawRectFilled(NRectf(NVec2f(cursorPosPx.x - 1, cursorPosPx.y), NVec2f(cursorPosPx.x, cursorPosPx.y + pCharInfo->textSize.y)), m_pBuffer->GetTheme().GetColor(ThemeColor::CursorInsert));
            }
            break;

            case CursorType::Normal:
            case CursorType::Visual:
            {
                GetEditor().GetDisplay().DrawRectFilled(NRectf(cursorPosPx, NVec2f(cursorPosPx.x + pCharInfo->textSize.x, cursorPosPx.y + pCharInfo->textSize.y)), m_pBuffer->GetTheme().GetColor(ThemeColor::CursorNormal));
            }
            break;
        }
    }
}

ZepTabWindow& ZepWindow::GetTabWindow() const
{
    return m_tabWindow;
}

void ZepWindow::SetWindowFlags(uint32_t windowFlags)
{
    m_windowFlags = windowFlags;
}

uint32_t ZepWindow::GetWindowFlags() const
{
    return m_windowFlags;
}

void ZepWindow::ToggleFlag(uint32_t flag)
{
    if (m_windowFlags & flag)
    {
        m_windowFlags &= ~flag;
    }
    else
    {
        m_windowFlags |= flag;
    }
}

long ZepWindow::GetMaxDisplayLines()
{
    UpdateLayout();
    return m_maxDisplayLines;
}

long ZepWindow::GetNumDisplayedLines()
{
    UpdateLayout();
    return std::min((long)m_windowLines.size(), GetMaxDisplayLines());
}

void ZepWindow::SetBufferCursor(BufferLocation location)
{
    m_bufferCursor = m_pBuffer->Clamp(location);
    m_lastCursorColumn = BufferToDisplay(m_bufferCursor).x;
}

void ZepWindow::SetBuffer(ZepBuffer* pBuffer)
{
    assert(pBuffer);
    m_pBuffer = pBuffer;
    m_layoutDirty = true;
}

BufferLocation ZepWindow::GetBufferCursor()
{
    return m_bufferCursor;
}

ZepBuffer& ZepWindow::GetBuffer() const
{
    return *m_pBuffer;
}

bool ZepWindow::IsActiveWindow() const
{
    return m_tabWindow.GetActiveWindow() == this;
}

NVec4f ZepWindow::FilterActiveColor(const NVec4f& col)
{
    if (!IsActiveWindow())
    {
        return NVec4f(Luminosity(col));
    }
    return col;
}

void ZepWindow::DisplayScrollers()
{ 
    if (m_vScrollRegion->rect.Empty())
        return;

    Scroller_Display(*m_vScroller, GetEditor(), m_pBuffer->GetTheme());

    GetEditor().GetDisplay().SetClipRect(m_bufferRegion->rect);
}

void ZepWindow::UpdateLayout()
{
    if (m_layoutDirty)
    {
        LayoutRegion(*m_bufferRegion);

        UpdateLineSpans();

        m_layoutDirty = false;
    }
}

void ZepWindow::Display()
{
    // Ensure line spans are valid; updated if the text is changed or the window dimensions change
    UpdateLayout();
    ScrollToCursor();
    UpdateScrollers();

    // Second pass if the scroller visibility changed, since this can change the whole layout!
    if (m_scrollVisibilityChanged)
    {
        m_layoutDirty = true;
        UpdateLayout();
        ScrollToCursor();
        UpdateScrollers();
        m_scrollVisibilityChanged = false;
    }

    auto cursorCL = BufferToDisplay(m_bufferCursor);

    // Always update
    UpdateAirline();

    UpdateLayout();

    GetEditor().GetDisplay().DrawRectFilled(m_textRegion->rect, m_pBuffer->GetTheme().GetColor(ThemeColor::Background));
    GetEditor().GetDisplay().DrawRectFilled(m_numberRegion->rect, m_pBuffer->GetTheme().GetColor(ThemeColor::LineNumberBackground));
    GetEditor().GetDisplay().DrawRectFilled(m_indicatorRegion->rect, m_pBuffer->GetTheme().GetColor(ThemeColor::LineNumberBackground));

    DisplayScrollers();

    if (m_numberRegion->rect.topLeftPx.x > m_numberRegion->rect.Width())
    {
        GetEditor().GetDisplay().DrawRectFilled(
            NRectf(NVec2f(m_numberRegion->rect.topLeftPx.x, m_numberRegion->rect.topLeftPx.y), NVec2f(m_numberRegion->rect.topLeftPx.x + 1, m_numberRegion->rect.bottomRightPx.y)), m_pBuffer->GetTheme().GetColor(ThemeColor::TabInactive));
    }

    if (IsActiveWindow() && cursorCL.x != -1)
    {
        if (GetEditor().GetCurrentMode()->GetEditorMode() != EditorMode::Visual)
        {
            auto& cursorLine = GetCursorLineInfo(cursorCL.y);

            if (IsInsideTextRegion(cursorCL))
            {
                // Cursor line
                GetEditor().GetDisplay().DrawRectFilled(NRectf(NVec2f(m_textRegion->rect.topLeftPx.x, cursorLine.spanYPx - m_bufferOffsetYPx + m_textRegion->rect.topLeftPx.y), NVec2f(m_textRegion->rect.bottomRightPx.x, cursorLine.spanYPx - m_bufferOffsetYPx + m_textRegion->rect.topLeftPx.y + GetEditor().GetDisplay().GetFontSize())), m_pBuffer->GetTheme().GetColor(ThemeColor::CursorLineBackground));
            }
        }
    }

    for (int displayPass = 0; displayPass < WindowPass::Max; displayPass++)
    {
        for (long windowLine = m_visibleLineRange.x; windowLine < m_visibleLineRange.y; windowLine++)
        {
            auto& lineInfo = m_windowLines[windowLine];
            if (!DisplayLine(lineInfo, m_textRegion->rect, displayPass))
            {
                break;
            }
        }
    }

    // Airline and underline
    GetEditor().GetDisplay().DrawRectFilled(m_airlineRegion->rect, m_pBuffer->GetTheme().GetColor(ThemeColor::AirlineBackground));

    auto airHeight = GetEditor().GetDisplay().GetFontSize();
    auto border = 12.0f;

    NVec2f screenPosYPx = m_airlineRegion->rect.topLeftPx + NVec2f(0.0f, textBorder);
    for (int i = 0; i < m_airline.leftBoxes.size(); i++)
    {
        auto textSize = GetEditor().GetDisplay().GetTextSize((const utf8*)m_airline.leftBoxes[i].text.c_str());
        textSize.x += border * 2;

        auto col = FilterActiveColor(m_airline.leftBoxes[i].background);
        GetEditor().GetDisplay().DrawRectFilled(NRectf(screenPosYPx, NVec2f(textSize.x + screenPosYPx.x, screenPosYPx.y + airHeight)), col);

        NVec4f textCol = m_pBuffer->GetTheme().GetComplement(m_airline.leftBoxes[i].background);
        GetEditor().GetDisplay().DrawChars(screenPosYPx + NVec2f(border, 0), textCol, (const utf8*)(m_airline.leftBoxes[i].text.c_str()));
        screenPosYPx.x += textSize.x;
    }
}

// *** Motions ***
void ZepWindow::MoveCursorY(int yDistance, LineLocation clampLocation)
{
    UpdateLayout();

    // Get the cursor
    auto cursorCL = BufferToDisplay();
    if (cursorCL.x == -1)
        return;

    // Find the screen line relative target
    auto target = cursorCL + NVec2i(0, yDistance);
    target.y = std::max(0l, target.y);
    target.y = std::min(target.y, long(m_windowLines.size() - 1));

    auto& line = m_windowLines[target.y];

    // Snap to the new vertical column if necessary (see comment below)
    if (target.x < m_lastCursorColumn)
        target.x = m_lastCursorColumn;

    // Update the master buffer cursor
    m_bufferCursor = line.columnOffsets.x + target.x;

    // Ensure the current x offset didn't walk us off the line
    // We are clamping to visible line here
    m_bufferCursor = std::max(m_bufferCursor, line.columnOffsets.x);
    m_bufferCursor = std::min(m_bufferCursor, line.columnOffsets.y - 1);

    // We can't call the buffer's LineLocation code, because when moving in span lines,
    // we are technically not moving in buffer lines; we are stepping in wrapped buffer lines.
    // But this function still requires a line end clamp! So we calculate it here based on the line info
    // In practice, we only care about LastNonCR and CRBegin, which are the Y Motions that Standard and Vim
    // Require (because standard has the cursor _on_ the CR sometimes, but Vim modes do not.
    // Because the LineLastNonCR might find a 'split' line which has a line end in the middle of the line,
    // it has to be more tricky to find the last 'Non CR' character, as here
    BufferLocation clampOffset = line.columnOffsets.y;
    switch (clampLocation)
    {
        case LineLocation::BeyondLineEnd:
            clampOffset = line.columnOffsets.y;
            break;
        case LineLocation::LineLastNonCR:
            if ((line.columnOffsets.y > 0) && (m_pBuffer->GetText()[line.columnOffsets.y - 1] == '\n' || m_pBuffer->GetText()[line.columnOffsets.y - 1] == 0))
            {
                clampOffset = std::max(line.columnOffsets.x, line.columnOffsets.y - 2);
            }
            else
            {
                clampOffset = std::max(line.columnOffsets.x, line.columnOffsets.y - 1);
            }
            break;
        case LineLocation::LineCRBegin:
            clampOffset = line.columnOffsets.y - 1;
            break;
        default:
            assert(!"Not supported Y motion line clamp!");
            break;
    }
    m_bufferCursor = std::min(m_bufferCursor, clampOffset);

    GetEditor().ResetCursorTimer();
}

NVec2i ZepWindow::BufferToDisplay()
{
    return BufferToDisplay(m_bufferCursor);
}

NVec2i ZepWindow::BufferToDisplay(const BufferLocation& loc)
{
    UpdateLayout();

    NVec2i ret(0, 0);
    int line_number = 0;

    // TODO:  A map
    for (auto& line : m_windowLines)
    {
        if (line.columnOffsets.x <= loc && line.columnOffsets.y > loc)
        {
            ret.y = line_number;
            ret.x = loc - line.columnOffsets.x;
            return ret;
        }
        line_number++;
    }

    // Max
    ret.y = int(m_windowLines.size() - 1);
    ret.x = m_windowLines[m_windowLines.size() - 1].columnOffsets.y - 1;
    return ret;
}

} // namespace Zep

/*
    // Ensure we can see the cursor
    /*
    NVec2i cursor(0, 0);
    cursor.x = m_pBuffer->GetBufferColumn(m_bufferCursor);
    cursor.y = m_pBuffer->GetBufferLine(m_bufferCursor) - m_pBuffer->GetBufferLine(m_dvisibleLineRange.x);

    // Handle the case where there is no need to scroll, since the visible lines are inside
    // The current screen rectangle.
    if (cursor.y >= m_nvisibleLineRange.y && m_linesFillScreen)
    {
        m_visibleLineRange.x = cursor.y - (m_nvisibleLineRange.y - m_vnisibleLineRange.x) + 1;
        m_linesChanged = true;
    }
    else if (cursor.y < m_nvisibleLineRange.x)
    {
        m_nvisibleLineRange.x = cursor.y;
        m_linesChanged = true;
    }

    // Clamp
    m_nvisibleLineRange.x = std::max(0l, (long)m_nvisibleLineRange.x);
*/
