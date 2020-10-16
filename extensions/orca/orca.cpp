#include "zep/mcommon/logger.h"

#include "zep/buffer.h"
#include "zep/window.h"

#include "orca.h"
#include "syntax_orca.h"

#include <mutils/profile/profile.h>

using namespace MUtils;
using namespace std::chrono;

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

    TimeProvider::Instance().UnRegisterConsumer(this);
}

void Orca::Init(ZepEditor& editor)
{
    m_pEditor = &editor;
    field_init(&m_field);
    oevent_list_init(&m_oevent_list);
    mbuf_reusable_init(&m_mbuf_r);

    TimeProvider::Instance().RegisterConsumer(this);
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

    ByteRange range;
    pBuffer->GetLineOffsets(0, range);

    // We don't have a pending buffer update to show if we just updated from the buffer
    m_updated.store(false);

    // Update field sizes
    field_resize_raw_if_necessary(&m_field, pBuffer->GetLineCount(), (range.second - range.first - 1));
    if (m_lastField.size() < (m_field.width * m_field.height))
    {
        m_lastField.assign(m_field.width * m_field.height, 0);
    }

    m_size = NVec2i(m_field.width, m_field.height);

    // Copy the buffer into the field
    auto& text = pBuffer->GetWorkingBuffer();
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
    if (!m_updated.load() && window.BufferToDisplay() == m_lastCursorPos)
    {
        return;
    }

    MUtilsZoneScoped;

    std::unique_lock<std::mutex> lock(m_mutex);

    m_lastCursorPos = window.BufferToDisplay();

    // Copy the calculated buffer data from the orca buffer
    auto& text = pBuffer->GetMutableWorkingBuffer();
    for (int y = 0; y < m_field.height; y++)
    {
        for (int x = 0; x < m_field.width; x++)
        {
            auto targetIndex = y * (m_field.width + 1) + x;
            auto ch = ReadField(x, y);
            // Special case + marker for grid bounds
            if (y % GridModulo == 0 && x % GridModulo == 0 && ch == '.')
            {
                ch = '+';
            }
            text[targetIndex] = ch;
        }
    }

    // Copy the precalculated syntax data to the buffer's syntax view
    auto pSyntax = dynamic_cast<ZepSyntax_Orca*>(pBuffer->GetSyntax());
    if (pSyntax)
    {
        BuildSyntax();
        pSyntax->UpdateSyntax(m_syntax);
    }

    m_updated.store(false);
}

// Generate the syntax information for the whole buffer
void Orca::BuildSyntax()
{
    MUtilsZoneScoped;

    m_syntax.resize((m_field.width + 1) * m_field.height);

    for (long y = 0; y < m_field.height; y++)
    {
        bool inComment = false;
        for (long x = 0; x < m_field.width; x++)
        {
            auto cursorGrid = m_lastCursorPos / NVec2i(GridModulo, GridModulo);
            auto valGrid = NVec2i(x, y) / NVec2i(GridModulo, GridModulo);

            // For highlighting the cursor area
            bool near = false;
            if (x > (((m_lastCursorPos.x / GridModulo) * GridModulo) - 1) && x <= ((1 + (m_lastCursorPos.x / GridModulo)) * GridModulo) && y > (((m_lastCursorPos.y / GridModulo) * GridModulo) - 1) && y <= ((1 + (m_lastCursorPos.y / GridModulo)) * GridModulo))
            {
                // This cell is near the cursor and needs the highlight
                if (x % 2 == 0 && y % 2 == 0)
                    near = true;
            }

            // Copy the mark into the bottom of the state
            auto index = FieldIndex(x, y);
            auto mark = (uint32_t)m_mbuf_r.buffer[index];

            auto glyph = m_field.buffer[index];
            auto glyphOld = m_lastField[index];

            // TODO: Do I still want/need this?
            if (glyph != glyphOld)
            {
                mark |= OrcaFlags::Changed;
            }

            if (y % GridModulo == 0 && x % GridModulo == 0 && glyph == '.')
            {
                glyph = Glyph_class_grid_marker;
            }
            else if (near && glyph == '.')
            {
                glyph = Glyph_class_grid_marker;
            }
            else if (glyph == '#')
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

    if (m & Mark_flag_lock)
    {
        return;
    }

    if (glyph == Glyph_class_grid || glyph == Glyph_class_grid_marker)
    {
        res.foreground = ThemeColor::Whitespace;
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
    TimeProvider::Instance().UnRegisterConsumer(this);
}

void Orca::Tick()
{
    if (!m_enable.load() && !m_step.load())
    {
        return;
    }

    MUtilsZoneScopedN("Orca Engine");

    auto& tp = TimeProvider::Instance();

    m_zeroQuantum = false;
    auto beat = tp.GetBeat();
    if ((beat - m_lastBeat) > tp.GetQuantum())
    {
        m_lastBeat = beat;
        m_zeroQuantum = true;
    }

    m_step.store(false);

    // Lock zone
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        TIME_SCOPE(OrcaUpdate)
        mbuffer_clear(m_mbuf_r.buffer, m_field.height, m_field.width);
        oevent_list_clear(&m_oevent_list);
        orca_run(m_field.buffer, m_mbuf_r.buffer, m_field.height, m_field.width, m_frame++, &m_oevent_list, 0);

        for (uint32_t i = 0; i < m_oevent_list.count; i++)
        {
            auto& ev = m_oevent_list.buffer[i];

            if (ev.any.oevent_type == Oevent_type_midi_note)
            {
                // Note information
                    auto note = ev.midi_note.note + ((ev.midi_note.octave + 1) * 12);
                auto velocity = (ev.midi_note.velocity > 0) ? ((float)ev.midi_note.velocity / 127.0f) : 1.0;
                auto duration = std::max(ev.midi_note.duration, (uint8_t)1) * tp.GetTimePerBeat();

                // Max MIDI note allowed
                note = std::min<int>(127, note);

                // TODO: For now, play at next beat
                // We know that m_time is 'on' the engine's beat
                // Note: need time to be unique??!
                auto time = TimeProvider::Instance().Now() + nanoseconds(i) + tp.GetTimePerBeat() * 4;

                // Submit event
                auto pEvent = m_timeline.GetEventPool().Alloc();
                pEvent->SetTime(time, duration_cast<milliseconds>(duration));
                pEvent->velocity = float(velocity);
                pEvent->midiNote = note;
                pEvent->pressed = true;
                pEvent->channelId = note;

                m_timeline.StoreTimeEvent(pEvent);
            }
        }

        m_lastField.assign(m_field.buffer, m_field.buffer + size_t(m_field.width * m_field.height));
        m_updated.store(true);

        // This call is thread safe
        m_pEditor->RequestRefresh();
    }
}

} // namespace Zep
