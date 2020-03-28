#include "zep/mcommon/logger.h"

#include "zep/buffer.h"
#include "zep/window.h"

#include "orca.h"
#include "syntax_orca.h"

using namespace MUtils;

namespace Zep
{

Orca::Orca()
{
}

Orca::~Orca()
{
    Quit();

    mbuf_reusable_deinit(&m_mbuf_r);
    oevent_list_deinit(&m_oevent_list);
    field_deinit(&m_field);
}

void Orca::Init(ZepEditor& editor)
{
    field_init(&m_field);
    oevent_list_init(&m_oevent_list);
    mbuf_reusable_init(&m_mbuf_r);

    m_thread = std::thread([&]() {
        RunThread(editor);
    });
}

long Orca::FieldIndex(long x, long y)
{
    return long(y * long(m_field.width) + x);
}

long Orca::StateIndex(long x, long y, long xOff, long yOff)
{
    auto xNew = x + xOff;
    auto yNew = y + yOff;
    if (xNew < 0 || yNew < 0 || xNew >= m_field.width || yNew >= m_field.height)
    {
        return -1;
    }
    return (yNew * (m_field.width + 1)) + xNew;
};

long Orca::StateIndex(long x, long y)
{
    if (x < 0 || y < 0 || x >= m_field.width || y >= m_field.height)
    {
        return -1;
    }
    return (y * (m_field.width + 1)) + x;
};

Glyph Orca::ReadField(long x, long y, Glyph failVal)
{
    if (x < m_field.width && y < m_field.height && x >= 0 && y >= 0)
    {
        return m_field.buffer[FieldIndex(x, y)];
    }
    return failVal;
};

void Orca::WriteField(long x, long y, Glyph val)
{
    if (x < m_field.width && y < m_field.height && x >= 0 && y >= 0)
    {
        m_field.buffer[FieldIndex(x, y)] = val;
    }
};

void Orca::ReadFromBuffer(ZepBuffer* pBuffer)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    ByteIndex start, end;
    pBuffer->GetLineOffsets(0, start, end);

    // We don't have a pending buffer update to show if we just updated from the buffer
    m_updated.store(false);

    // Update field sizes
    field_resize_raw_if_necessary(&m_field, pBuffer->GetLineCount(), (end - start - 1));
    if (m_lastField.size() < (m_field.width * m_field.height))
    {
        m_lastField.assign(m_field.width * m_field.height, 0);
    }

    m_size = NVec2i(m_field.width, m_field.height);

    // Copy the buffer into the field
    auto& text = pBuffer->GetText();
    auto sz = text.size();
    for (int y = 0; y < m_field.height; y++)
    {
        for (int x = 0; x < m_field.width; x++)
        {
            auto sourceIndex = y * (m_field.width + 1) + x;
            if (sourceIndex < sz)
            {
                auto ch = text[sourceIndex];
                // Replace things we inserted that aren't valid Orca syntax
                if (ch == '+' || ch == ' ')
                {
                    ch = '.';
                }
                m_field.buffer[y * m_field.width + x] = ch;
            }
        }
    }

    mbuf_reusable_ensure_size(&m_mbuf_r, m_field.height, m_field.width);
}

void Orca::WriteToBuffer(ZepBuffer* pBuffer, ZepWindow& window)
{
    if (!m_updated.load())
    {
        return;
    }

    std::unique_lock<std::mutex> lock(m_mutex);

    auto& text = pBuffer->GetMutableText();
    auto size = text.size();
    for (int y = 0; y < m_field.height; y++)
    {
        for (int x = 0; x < m_field.width; x++)
        {
            auto ch = ReadField(x, y);

            // Calculate which dots to show
            auto cursor = window.BufferToDisplay();
            auto cursorGrid = cursor / NVec2i(GridModulo, GridModulo);
            auto valGrid = NVec2i(x, y) / NVec2i(GridModulo, GridModulo);

            bool near = false;
            if (x > (((cursor.x / GridModulo) * GridModulo) - 1) && x <= ((1 + (cursor.x / GridModulo)) * GridModulo) && y > (((cursor.y / GridModulo) * GridModulo) - 1) && y <= ((1 + (cursor.y / GridModulo)) * GridModulo))
            {
                near = true;
            }

            auto targetIndex = y * (m_field.width + 1) + x;
            if (targetIndex < size)
            {
                if (y % GridModulo == 0 && x % GridModulo == 0 && ch == '.')
                {
                    ch = '+';
                    text[targetIndex] = '+';
                }
                else if (near && ch == '.')
                {
                    text[targetIndex] = ' ';
                }
                else
                {
                    text[targetIndex] = ch;
                }

            }
        }
    }

    // Special syntax trigger, since we do things differently with orca
    auto pSyntax = dynamic_cast<ZepSyntax_Orca*>(pBuffer->GetSyntax());
    if (pSyntax)
    {
        pSyntax->UpdateSyntax(m_syntax);
    }

    m_updated.store(false);
}

void Orca::RunThread(ZepEditor& editor)
{
    TimeProvider::Instance().RegisterConsumer(this);

    for (;;)
    {
        // Wait for a tick or timeout and check quit
        TimeEvent tick;
        if (!m_tickQueue.wait_dequeue_timed(tick, std::chrono::milliseconds(250)))
        {
            if (m_quit.load())
            {
                break;
            }

            continue;
        }

        if (m_quit.load())
        {
            break;
        }

        if (!m_enable.load() && !m_step.load())
        {
            continue;
        }
        m_step.store(false);

        // Lock zone
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            TIME_SCOPE(OrcaUpdate)
                mbuffer_clear(m_mbuf_r.buffer, m_field.height, m_field.width);
            oevent_list_clear(&m_oevent_list);
            orca_run(m_field.buffer, m_mbuf_r.buffer, m_field.height, m_field.width, m_tickCount++, &m_oevent_list, 0);

            UpdateStateFlags();

            m_lastField.assign(m_field.buffer, m_field.buffer + size_t(m_field.width * m_field.height));
            m_updated.store(true);

            // This call is thread safe
            editor.RequestRefresh();
        }
    }

    TimeProvider::Instance().UnRegisterConsumer(this);
}

// Store the state flags for the syntax update later.
void Orca::UpdateStateFlags()
{
    m_syntax.resize((m_field.width + 1) * m_field.height);

    for (long y = 0; y < m_field.height; y++)
    {
        bool inComment = false;
        for (long x = 0; x < m_field.width; x++)
        {
            // Copy the mark into the bottom of the state
            auto index = FieldIndex(x, y);
            auto mark = (uint32_t)m_mbuf_r.buffer[index];

            auto glyph = m_field.buffer[index];
            auto glyphOld = m_lastField[index];
            if (glyph != glyphOld)
            {
                mark |= OrcaFlags::Changed;
            }

            if (glyph == '#')
            {
                glyph = Glyph_class_comment;
                inComment = !inComment;
            }

            if (inComment)
            {
                glyph = Glyph_class_comment;
            }

            BuildSyntax(x, y, mark, glyph);
        }
    }
}

void Orca::BuildSyntax(int x, int y, uint32_t m, Glyph glyph)
{
    auto& res = m_syntax[StateIndex(x, y)];
    res.background = ThemeColor::Background;
    res.foreground = ThemeColor::None;

    const NVec4f cyan = NVec4f(0.4f, 0.85f, 0.86f, 1.0f);

    if (glyph == Glyph_class_comment)
    {
        res.foreground = ThemeColor::Comment;
        return;
    }

    // Show the grid + marker every GridModulo across, by displaying the whitespace
    if (x % GridModulo == 0 && y % GridModulo == 0 && glyph == '.')
    {
        res.foreground = ThemeColor::Whitespace;
        return;
    }

    // Locked are initially Dim
    if (m & Mark_flag_lock)
    {
        res.foreground = ThemeColor::TextDim;
    }

    // Haste inputs always cyan
    if (m & Mark_flag_haste_input)
    {
        res.foreground = ThemeColor::Custom;
        res.customForegroundColor = cyan;
        return;
    }
    else if (m & Mark_flag_input)
    {
        res.foreground = ThemeColor::Text;
        return;
    }

    // Outputs always black on white
    if (m & Mark_flag_output)
    {
        res.foreground = ThemeColor::Background;
        res.background = ThemeColor::Text;
        return;
    }


    switch (glyph_class_of(glyph))
    {
    case Glyph_class_unknown:
        res.foreground = ThemeColor::Error;
        return;
        break;
    case Glyph_class_grid:
        res.foreground = ThemeColor::Background;
        return;
        break;
    case Glyph_class_grid_marker:
        res.foreground = ThemeColor::Whitespace;
        return;
        break;
    case Glyph_class_lowercase:
        res.foreground = ThemeColor::TextDim;
        return;
    case Glyph_class_uppercase:
    case Glyph_class_message:
        res.background = ThemeColor::Custom;
        res.customBackgroundColor = cyan;
        res.foreground = ThemeColor::Background;
        return;
    case Glyph_class_movement:
        res.foreground = ThemeColor::TextDim;
        return;
        break;
    case Glyph_class_bang:
        res.background = ThemeColor::FlashColor;
        res.foreground = ThemeColor::Text;
        return;
        break;
    default:
        res.foreground = ThemeColor::TextDim;
        res.background = ThemeColor::Background;
        break;
    }
}

void Orca::SetTickCount(int count)
{
    m_tickCount.store(count);
}

void Orca::SetTestField(const NVec2i& fieldSize)
{
    ZEP_UNUSED(fieldSize);
}

void Orca::Enable(bool enable)
{
    m_enable.store(enable);
}

void Orca::Step()
{
    // Single step
    m_step.store(true);
}

void Orca::Quit()
{
    // Check if already quit
    if (m_quit.load())
    {
        return;
    }

    // Quit and wait
    m_quit.store(true);
    m_thread.join();
}

void Orca::AddTimeEvent(const MUtils::TimeEvent& ev)
{
    m_tickQueue.enqueue(ev);
}

} // Zep
