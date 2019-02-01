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
#define UTF8_CHAR_LEN(byte) ((0xE5000000 >> ((byte >> 3) & 0x1e)) & 3) + 1

const float ScrollBarSize = 17.0f;
NVec2f DefaultCharSize;

ZepWindow::ZepWindow(ZepTabWindow& window, ZepBuffer* buffer)
    : ZepComponent(window.GetEditor())
    , m_tabWindow(window)
    , m_pBuffer(buffer)
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

    m_vScroller = std::make_shared<Scroller>(GetEditor(), *m_vScrollRegion);
    m_vScroller->vertical = false;

    timer_start(m_toolTipTimer);
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
    m_vScroller->vScrollLinePercent = 1.0f / m_windowLines.size();
    m_vScroller->vScrollPagePercent = m_vScroller->vScrollVisiblePercent;

    if (GetEditor().GetShowScrollBar() == 0)
    {
        m_vScrollRegion->fixed_size = NVec2f(0.0f);
    }
    else
    {
        if (m_vScroller->vScrollVisiblePercent >= 1.0f && GetEditor().GetShowScrollBar() != 2)
        {
            m_vScrollRegion->fixed_size = NVec2f(0.0f, 0.0f);
        }
        else
        {
            m_vScrollRegion->fixed_size = NVec2f(ScrollBarSize * GetEditor().GetPixelScale(), 0.0f);
        }
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
    if (payload->messageId == Msg::Buffer)
    {
        auto pMsg = std::static_pointer_cast<BufferMessage>(payload);

        if (pMsg->pBuffer != m_pBuffer)
        {
            return;
        }

        m_layoutDirty = true;

        if (pMsg->type != BufferMessageType::PreBufferChange)
        {
            // Put the cursor where the replaced text was added
            GetEditor().ResetCursorTimer();
        }
        DisableToolTipTillMove();
    }
    else if (payload->messageId == Msg::ComponentChanged)
    {
        if (payload->pComponent == m_vScroller.get())
        {
            auto pScroller = dynamic_cast<Scroller*>(payload->pComponent);
            m_bufferOffsetYPx = pScroller->vScrollPosition * (m_windowLines.size() * GetEditor().GetDisplay().GetFontSize());
            UpdateVisibleLineRange();
            EnsureCursorVisible();
            DisableToolTipTillMove();
        }
    }
    else if (payload->messageId == Msg::MouseMove)
    {
        if (!m_toolTips.empty())
        {
            if (ManhattanDistance(m_tipStartPos, payload->pos) > 4.0f)
            {
                timer_restart(m_toolTipTimer);
                m_toolTips.clear();
            }
        }
        else
        {
            timer_restart(m_toolTipTimer);
            m_tipStartPos = payload->pos;

            // Can now show tooltip again, due to mouse hover
            m_tipDisabledTillMove = false;
        }
    }
}

NVec2f ZepWindow::GetTextSize(const utf8* pCh, const utf8* pEnd)
{
    assert(pEnd != nullptr);
    if ((pEnd - pCh) == 1)
    {
        return m_charCache[*pCh];
    }
    return GetEditor().GetDisplay().GetTextSize(pCh, pEnd);
}

void ZepWindow::SetDisplayRegion(const NRectf& region)
{
    if (m_bufferRegion->rect == region)
    {
        return;
    }

    BuildCharCache();

    m_layoutDirty = true;
    m_bufferRegion->rect = region;

    m_airlineRegion->fixed_size = NVec2f(0.0f, GetEditor().GetDisplay().GetFontSize());

    // Border, and move the text across a bit
    m_numberRegion->fixed_size = NVec2f(float(leftBorderChars) * DefaultCharSize.x, 0);

    m_indicatorRegion->fixed_size = NVec2f(DefaultCharSize.x, 0.0f);

    m_defaultLineSize = GetEditor().GetDisplay().GetFontSize();
}

void ZepWindow::EnsureCursorVisible()
{
    UpdateLayout();
    BufferLocation loc = m_bufferCursor;
    for (auto& line : m_windowLines)
    {
        if (line->columnOffsets.x <= loc && line->columnOffsets.y > loc)
        {
            auto cursorLine = line->lineIndex;
            if (cursorLine < m_visibleLineRange.x)
            {
                MoveCursorY(std::abs(m_visibleLineRange.x - cursorLine));
            }
            else if (cursorLine >= m_visibleLineRange.y)
            {
                MoveCursorY((long(m_visibleLineRange.y) - cursorLine) - 1);
            }
            m_cursorMoved = false;
            return;
        }
    }
}

void ZepWindow::ScrollToCursor()
{
    if (!m_cursorMoved)
    {
        return;
    }

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
    m_cursorMoved = false;
}

// This is the most expensive part of window update; applying line span generation for wrapped text.
// It can take about a millisecond to do during editing on release buildj; but this is fast enough for now.
// There are several ways in which this function can be optimized:
// - Only regenerate spans which have changed, since only the edited line range will change.
// - Generate blocks of text, based on syntax highlighting, instead of single characters.
// - Have a no-wrap text mode and save a lot of the wrapping work.
// - Do some threading
void ZepWindow::UpdateLineSpans()
{
    TIME_SCOPE(UpdateLineSpans);

    m_maxDisplayLines = (long)std::max(0.0f, std::floor(m_textRegion->rect.Height() / m_defaultLineSize));
    float screenPosX = m_textRegion->rect.topLeftPx.x;

    // For now, we are compromising on ASCII; so don't query font fixed_size each time
    const auto& textBuffer = m_pBuffer->GetText();

    long bufferLine = 0;
    long spanLine = 0;
    float bufferPosYPx = 0;

    // Nuke the existing spans
    // In future we can in-place modify for speed
    std::for_each(m_windowLines.begin(), m_windowLines.end(), [](SpanInfo* pInfo) { delete pInfo; });
    m_windowLines.clear();

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

        // Start a new line
        SpanInfo* lineInfo = new SpanInfo();
        lineInfo->bufferLineNumber = bufferLine;
        lineInfo->lineIndex = spanLine;
        lineInfo->columnOffsets.x = columnOffsets.x;
        lineInfo->columnOffsets.y = columnOffsets.x;
        lineInfo->spanYPx = bufferPosYPx;

        // These offsets are 0 -> n + 1, i.e. the last offset the buffer returns is 1 beyond the current
        for (auto ch = columnOffsets.x; ch < columnOffsets.y; ch++)
        {
            const utf8* pCh = &textBuffer[ch];
            const auto textSize = GetTextSize(pCh, pCh + 1);

            // Wrap if we have displayed at least one char, and we have to
            if (m_wrap && ch != columnOffsets.x)
            {
                // At least a single char has wrapped; close the old line, start a new one
                if (((screenPosX + textSize.x) + textSize.x) >= (m_textRegion->rect.bottomRightPx.x))
                {
                    // Remember the offset beyond the end of the line
                    lineInfo->columnOffsets.y = ch;
                    m_windowLines.push_back(lineInfo);

                    // Next line
                    lineInfo = new SpanInfo();
                    spanLine++;
                    bufferPosYPx += GetEditor().GetDisplay().GetFontSize();

                    // Now jump to the next 'screen line' for the rest of this 'buffer line'
                    lineInfo->columnOffsets = NVec2i(ch, ch + 1);
                    lineInfo->lastNonCROffset = 0;
                    lineInfo->lineIndex = spanLine;
                    lineInfo->bufferLineNumber = bufferLine;
                    lineInfo->spanYPx = bufferPosYPx;
                    screenPosX = m_textRegion->rect.topLeftPx.x;
                }
                else
                {
                    screenPosX += textSize.x;
                }
            }

            lineInfo->spanYPx = bufferPosYPx;
            lineInfo->columnOffsets.y = ch + 1;

            lineInfo->lastNonCROffset = std::max(ch, 0l);
        }

        // Complete the line
        m_windowLines.push_back(lineInfo);

        // Next time round - down a buffer line, down a span line
        bufferLine++;
        spanLine++;
        screenPosX = m_textRegion->rect.topLeftPx.x;
        bufferPosYPx += GetEditor().GetDisplay().GetFontSize();
    }

    // Sanity
    if (m_windowLines.empty())
    {
        SpanInfo* lineInfo = new SpanInfo();
        lineInfo->columnOffsets.x = 0;
        lineInfo->columnOffsets.y = 0;
        lineInfo->lastNonCROffset = 0;
        lineInfo->bufferLineNumber = 0;
        m_windowLines.push_back(lineInfo);
    }

    m_bufferSizeYPx = m_windowLines[m_windowLines.size() - 1]->spanYPx + GetEditor().GetDisplay().GetFontSize();

    UpdateVisibleLineRange();
    m_layoutDirty = true;
}

void ZepWindow::UpdateVisibleLineRange()
{
    TIME_SCOPE(UpdateVisibleLineRange);

    m_visibleLineRange.x = (long)m_windowLines.size();
    m_visibleLineRange.y = 0;
    for (long line = 0; line < long(m_windowLines.size()); line++)
    {
        auto& windowLine = *m_windowLines[line];
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
    return *m_windowLines[y];
}

// Convert a normalized y coordinate to the window region
float ZepWindow::ToWindowY(float pos) const
{
    return pos - m_bufferOffsetYPx + m_bufferRegion->rect.topLeftPx.y;
}

void ZepWindow::DisplayToolTip(const NVec2f& pos, const RangeMarker& marker) const
{
    auto textSize = GetEditor().GetDisplay().GetTextSize((const utf8*)marker.description.c_str(), (const utf8*)(marker.description.c_str() + marker.description.size()));

    float boxShadowWidth = 2.0f;
    // Draw a black area a little wider than the tip box.
    NRectf tipBox(pos.x + textBorder, pos.y + textBorder + m_defaultLineSize, textSize.x, textSize.y);
    tipBox.Adjust(-textBorder - boxShadowWidth, -textBorder - boxShadowWidth, textBorder + boxShadowWidth, textBorder + boxShadowWidth);

    // Try to make it fit.  Move up from the bottom, then back from the top, so that the starting rect is always clamped to the origin
    auto xDiff = tipBox.bottomRightPx.x - m_textRegion->rect.bottomRightPx.x;
    if (xDiff > 0)
    {
        tipBox.Adjust(-xDiff, 0.0f);
    }
    auto yDiff = tipBox.bottomRightPx.y - m_textRegion->rect.bottomRightPx.y;
    if (yDiff > 0)
    {
        tipBox.Adjust(0.0f, -yDiff);
    }
    xDiff = tipBox.topLeftPx.x - m_textRegion->rect.topLeftPx.x;
    if (xDiff < 0)
    {
        tipBox.Adjust(-xDiff, 0.0f);
    }
    yDiff = tipBox.topLeftPx.y - m_textRegion->rect.topLeftPx.y;
    if (yDiff < 0)
    {
        tipBox.Adjust(0.0f, -yDiff);
    }

    GetEditor().GetDisplay().DrawRectFilled(tipBox, m_pBuffer->GetTheme().GetColor(ThemeColor::Background));

    // Draw a lighter inner and a border the same color as the marker theme
    tipBox.Adjust(boxShadowWidth, boxShadowWidth, -boxShadowWidth, -boxShadowWidth);
    GetEditor().GetDisplay().DrawRectFilled(tipBox, m_pBuffer->GetTheme().GetColor(ThemeColor::AirlineBackground));
    GetEditor().GetDisplay().DrawLine(tipBox.topLeftPx, tipBox.TopRight(), m_pBuffer->GetTheme().GetColor(marker.highlightColor));
    GetEditor().GetDisplay().DrawLine(tipBox.BottomLeft(), tipBox.bottomRightPx, m_pBuffer->GetTheme().GetColor(marker.highlightColor));
    GetEditor().GetDisplay().DrawLine(tipBox.topLeftPx, tipBox.BottomLeft(), m_pBuffer->GetTheme().GetColor(marker.highlightColor));
    GetEditor().GetDisplay().DrawLine(tipBox.TopRight(), tipBox.bottomRightPx, m_pBuffer->GetTheme().GetColor(marker.highlightColor));

    // Draw the text in the box
    GetEditor().GetDisplay().DrawChars(tipBox.topLeftPx + NVec2f(textBorder, textBorder), m_pBuffer->GetTheme().GetColor(marker.textColor), (const utf8*)marker.description.c_str());
}

// TODO: This function draws one char at a time.  It could be more optimal at the expense of some
// complexity.  Basically, I don't like the current implementation, but it works for now.
// The text is displayed acorrding to the region bounds and the display lineData
// Additionally (and perhaps that should be a seperate function), this code draws line numbers
bool ZepWindow::DisplayLine(const SpanInfo& lineInfo, int displayPass)
{
    // A middle-dot whitespace character
    static const auto whiteSpace = string_from_wstring(std::wstring(L"\x00b7"));
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
        GetEditor().GetDisplay().DrawChars(NVec2f(m_numberRegion->rect.bottomRightPx.x - textSize.x, ToWindowY(lineInfo.spanYPx)), digitCol, (const utf8*)strNum.c_str(), (const utf8*)(strNum.c_str() + strNum.size()));
    };

    if (displayPass == WindowPass::Background)
    {
        // Show any markers in the left indicator region
        for (auto& marker : m_pBuffer->GetRangeMarkers())
        {
            if (marker->displayType & RangeMarkerDisplayType::Indicator)
            {
                auto sel = marker->range;
                if (lineInfo.columnOffsets.x <= sel.second && lineInfo.columnOffsets.y > sel.first)
                {
                    GetEditor().GetDisplay().DrawRectFilled(NRectf(NVec2f(m_indicatorRegion->rect.topLeftPx.x, ToWindowY(lineInfo.spanYPx)), NVec2f(m_indicatorRegion->rect.Center().x, ToWindowY(lineInfo.spanYPx) + GetEditor().GetDisplay().GetFontSize())), m_pBuffer->GetTheme().GetColor(marker->highlightColor));
                }
            }
        }
        showLineNumber();
    }

    auto screenPosX = m_textRegion->rect.topLeftPx.x;

    char invalidChar;

    auto pSyntax = m_pBuffer->GetSyntax();

    auto tipTimeSeconds = timer_get_elapsed_seconds(m_toolTipTimer);

    // Walk from the start of the line to the end of the line (in buffer chars)
    for (auto ch = lineInfo.columnOffsets.x; ch < lineInfo.columnOffsets.y; ch++)
    {
        const utf8* pCh = &m_pBuffer->GetText()[ch];

        // Visible white space
        if (pSyntax && pSyntax->GetSyntaxAt(ch) == ThemeColor::Whitespace)
        {
            pCh = (const utf8*)whiteSpace.c_str();
        }

        // Shown only one char for end of line
        bool hiddenChar = false;
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
            hiddenChar = true;
        }

        // Note: We don't really support UTF8, but our whitespace symbol is UTF8!
        const utf8* pEnd = pCh + UTF8_CHAR_LEN(*pCh);

        auto textSize = GetTextSize(pCh, pEnd);
        if (displayPass == 0)
        {
            if (IsActiveWindow())
            {
                if (GetEditor().GetCurrentMode()->GetEditorMode() == EditorMode::Visual)
                {
                    auto sel = m_pBuffer->GetSelection();
                    if (ch >= sel.first && ch < sel.second && !hiddenChar)
                    {
                        GetEditor().GetDisplay().DrawRectFilled(NRectf(NVec2f(screenPosX, ToWindowY(lineInfo.spanYPx)), NVec2f(screenPosX + textSize.x, ToWindowY(lineInfo.spanYPx) + textSize.y)), m_pBuffer->GetTheme().GetColor(ThemeColor::VisualSelectBackground));
                    }
                }
            }

            NRectf charRect(NVec2f(screenPosX, ToWindowY(lineInfo.spanYPx)), NVec2f(screenPosX + textSize.x, ToWindowY(lineInfo.spanYPx) + textSize.y));
            if (charRect.Contains(m_tipStartPos))
            {
                // Record the mouse-over buffer location
                m_tipBufferLocation = ch;
            }

            // Show any markers 
            for (auto& marker : m_pBuffer->GetRangeMarkers())
            {
                auto sel = marker->range;
                if (ch >= sel.first && ch < sel.second)
                {
                    if (marker->displayType & RangeMarkerDisplayType::Underline)
                    {
                        GetEditor().GetDisplay().DrawRectFilled(NRectf(NVec2f(screenPosX, ToWindowY(lineInfo.spanYPx) + textSize.y - 1), NVec2f(screenPosX + textSize.x, ToWindowY(lineInfo.spanYPx) + textSize.y)), m_pBuffer->GetTheme().GetColor(marker->highlightColor));
                    }
                    else if (marker->displayType & RangeMarkerDisplayType::Background)
                    {
                        GetEditor().GetDisplay().DrawRectFilled(charRect, m_pBuffer->GetTheme().GetColor(marker->highlightColor));
                    }

                    // If this marker has an associated tooltip, pop it up after a time delay
                    if (marker->displayType & RangeMarkerDisplayType::Tooltip)
                    {
                        if (m_toolTips.empty() &&
                            !m_tipDisabledTillMove &&
                            m_tipBufferLocation == ch &&
                            (tipTimeSeconds > 0.5f))
                        {
                            // Register this tooltip
                            m_toolTips[NVec2f(m_tipStartPos.x, m_tipStartPos.y + textBorder)] = marker;
                        }
                    }
                }
            }
        }
        else
        {
            if (pSyntax && pSyntax->GetSyntaxAt(ch) == ThemeColor::Whitespace)
            {
                auto centerChar = NVec2f(screenPosX + textSize.x / 2, ToWindowY(lineInfo.spanYPx) + textSize.y / 2);
                GetEditor().GetDisplay().DrawRectFilled(NRectf(centerChar - NVec2f(1.0f, 1.0f), centerChar + NVec2f(1.0f, 1.0f)), m_pBuffer->GetTheme().GetColor(ThemeColor::Whitespace));
            }
            else
            {
                NVec4f col;
                if (*pCh == '\n' || *pCh == 0)
                {
                    col = m_pBuffer->GetTheme().GetColor(ThemeColor::HiddenText);
                }
                else
                {
                    col = pSyntax != nullptr ? pSyntax->GetSyntaxColorAt(ch) : m_pBuffer->GetTheme().GetColor(ThemeColor::Text);
                }
                GetEditor().GetDisplay().DrawChars(NVec2f(screenPosX, ToWindowY(lineInfo.spanYPx)), col, pCh, pEnd);
            }
        }

        screenPosX += textSize.x;
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

    auto& tex = m_pBuffer->GetText();
    auto xPos = m_textRegion->rect.topLeftPx.x;
    NVec2f cursorSize;
    bool found = false;
    for (auto ch = cursorBufferLine.columnOffsets.x; ch < cursorBufferLine.columnOffsets.y; ch++)
    {
        cursorSize = GetTextSize(&tex[ch], &tex[ch] + 1);
        if ((ch - cursorBufferLine.columnOffsets.x) == cursorCL.x)
        {
            found = true;
            break;
        }
        xPos += cursorSize.x;
    }
    if (!found)
    {
        cursorSize = DefaultCharSize;
        xPos += cursorSize.x;
    }

    // Draw the Cursor symbol
    auto cursorPosPx = NVec2f(xPos, cursorBufferLine.spanYPx - m_bufferOffsetYPx + m_textRegion->rect.topLeftPx.y);
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
                GetEditor().GetDisplay().DrawRectFilled(NRectf(NVec2f(cursorPosPx.x - 1, cursorPosPx.y), NVec2f(cursorPosPx.x, cursorPosPx.y + cursorSize.y)), m_pBuffer->GetTheme().GetColor(ThemeColor::CursorInsert));
            }
            break;

            case CursorType::Normal:
            case CursorType::Visual:
            {
                GetEditor().GetDisplay().DrawRectFilled(NRectf(cursorPosPx, NVec2f(cursorPosPx.x + cursorSize.x, cursorPosPx.y + cursorSize.y)), m_pBuffer->GetTheme().GetColor(ThemeColor::CursorNormal));
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
    m_cursorMoved = true;
    DisableToolTipTillMove();
}

void ZepWindow::DisableToolTipTillMove()
{
    m_tipDisabledTillMove = true;
    m_toolTips.clear();
}

void ZepWindow::SetBuffer(ZepBuffer* pBuffer)
{
    assert(pBuffer);
    m_pBuffer = pBuffer;
    m_layoutDirty = true;
    m_bufferOffsetYPx = 0;
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

    m_vScroller->Display(m_pBuffer->GetTheme());

    GetEditor().GetDisplay().SetClipRect(m_bufferRegion->rect);
}

void ZepWindow::UpdateLayout()
{
    if (m_layoutDirty)
    {
        BuildCharCache();

        LayoutRegion(*m_bufferRegion);

        UpdateLineSpans();

        m_layoutDirty = false;
    }
}

void ZepWindow::BuildCharCache()
{
    if (m_charCacheDirty == false)
    {
        return;
    }
    m_charCacheDirty = false;
   
    const char chA = 'A';
    DefaultCharSize = GetEditor().GetDisplay().GetTextSize((const utf8*)&chA, (const utf8*)&chA + 1);
    for (int i = 0; i < 256; i++)
    {
        utf8 ch = (utf8)i;
        m_charCache[i] = GetEditor().GetDisplay().GetTextSize(&ch, &ch + 1);
    }
}

void ZepWindow::Display()
{
    TIME_SCOPE(Display);
    // Ensure line spans are valid; updated if the text is changed or the window dimensions change
    UpdateLayout();
    ScrollToCursor();
    UpdateScrollers();

    // Second pass if the scroller visibility changed, since this can change the whole layout!
    if (m_scrollVisibilityChanged)
    {
        m_layoutDirty = true;
        m_cursorMoved = true;
        UpdateLayout();
        ScrollToCursor();
        UpdateScrollers();

        m_scrollVisibilityChanged = false;
    }

    auto cursorCL = BufferToDisplay(m_bufferCursor);
    m_tipBufferLocation = BufferLocation{-1};

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

    {
        TIME_SCOPE(DrawLine);
        for (int displayPass = 0; displayPass < WindowPass::Max; displayPass++)
        {
            for (long windowLine = m_visibleLineRange.x; windowLine < m_visibleLineRange.y; windowLine++)
            {
                auto& lineInfo = *m_windowLines[windowLine];
                if (!DisplayLine(lineInfo, displayPass))
                {
                    break;
                }
            }
        }
    }

    // No tooltip, and we can show one, then ask for tooltips
    if (!m_tipDisabledTillMove &&
        (timer_get_elapsed_seconds(m_toolTipTimer) > 0.5f) &&
        m_toolTips.empty() &&
        m_lastTipQueryPos != m_tipStartPos)
    {
        auto spMsg = std::make_shared<ToolTipMessage>(m_pBuffer, m_tipStartPos, m_tipBufferLocation);
        GetEditor().Broadcast(spMsg);
        if (spMsg->handled &&
            spMsg->spMarker != nullptr)
        {
            m_toolTips[NVec2f(m_tipStartPos.x, m_tipStartPos.y)] = spMsg->spMarker;
        }
        m_lastTipQueryPos = m_tipStartPos;
    }

    for (auto& toolTip : m_toolTips)
    {
        DisplayToolTip(toolTip.first, *toolTip.second);
    }

    // Airline and underline
    GetEditor().GetDisplay().DrawRectFilled(m_airlineRegion->rect, m_pBuffer->GetTheme().GetColor(ThemeColor::AirlineBackground));

    auto airHeight = GetEditor().GetDisplay().GetFontSize();
    auto border = 12.0f;

    NVec2f screenPosYPx = m_airlineRegion->rect.topLeftPx;
    for (int i = 0; i < (int)m_airline.leftBoxes.size(); i++)
    {
        auto pText = (const utf8*)m_airline.leftBoxes[i].text.c_str();
        auto textSize = GetTextSize(pText, pText + m_airline.leftBoxes[i].text.size());
        textSize.x += border * 2;

        auto col = FilterActiveColor(m_airline.leftBoxes[i].background);
        GetEditor().GetDisplay().DrawRectFilled(NRectf(screenPosYPx, NVec2f(textSize.x + screenPosYPx.x, screenPosYPx.y + airHeight)), col);

        NVec4f textCol = m_pBuffer->GetTheme().GetComplement(m_airline.leftBoxes[i].background);
        GetEditor().GetDisplay().DrawChars(screenPosYPx + NVec2f(border, 0.0f), textCol, (const utf8*)(m_airline.leftBoxes[i].text.c_str()));
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

    auto& line = *m_windowLines[target.y];

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

    m_cursorMoved = true;
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
        if (line->columnOffsets.x <= loc && line->columnOffsets.y > loc)
        {
            ret.y = line_number;
            ret.x = loc - line->columnOffsets.x;
            return ret;
        }
        line_number++;
    }

    // Max
    ret.y = int(m_windowLines.size() - 1);
    ret.x = m_windowLines[m_windowLines.size() - 1]->columnOffsets.y - 1;
    return ret;
}

} // namespace Zep

#if 0
    // Ensure we can see the cursor
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
#endif

