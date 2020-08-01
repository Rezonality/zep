#pragma once

#include <mutex>

#ifdef _WIN32
#pragma warning(disable : 4127)
#endif
#include <concurrentqueue/blockingconcurrentqueue.h>
#include <concurrentqueue/concurrentqueue.h>
#ifdef _WIN32
#pragma warning(default : 4127)
#endif

#include <mutils/time/time_provider.h>
#include <mutils/time/timeline.h>

#ifdef _WIN32
#pragma warning(disable : 4505)
#endif
extern "C" {
#include <orca-c/field.h>
#include <orca-c/gbuffer.h>
#include <orca-c/sim.h>
}
#ifdef _WIN32
#pragma warning(default : 4505)
#endif

#include <zep/syntax.h>

namespace Zep
{

const int GridModulo = 8;

namespace OrcaFlags
{
enum
{
    None = 0,
    Reserved_Mark0 = (1 << 0),
    Reserved_Mark1 = (1 << 1),
    Reserved_Mark2 = (1 << 2),
    Reserved_Mark3 = (1 << 3),
    Reserved_Mark4 = (1 << 4),
    Reserved_Mark5 = (1 << 5),
    Reserved_Mark6 = (1 << 6),
    Reserved_Mark7 = (1 << 7),
    Changed = (1 << 8),
};
};

typedef enum
{
    Glyph_class_unknown,
    Glyph_class_grid,
    Glyph_class_grid_marker,
    Glyph_class_comment,
    Glyph_class_uppercase,
    Glyph_class_lowercase,
    Glyph_class_movement,
    Glyph_class_numeric,
    Glyph_class_bang,
    Glyph_class_message,
} Glyph_class;

inline Glyph_class glyph_class_of(Glyph glyph)
{
    if (glyph == '.')
        return Glyph_class_grid;
    if (glyph >= '0' && glyph <= '9')
        return Glyph_class_numeric;
    switch (glyph)
    {
    case '+':
        return Glyph_class_grid_marker;
    case 'N':
    case 'n':
    case 'E':
    case 'e':
    case 'S':
    case 's':
    case 'W':
    case 'w':
    case 'Z':
    case 'z':
        return Glyph_class_movement;
    case '!':
    case ':':
    case ';':
    case '=':
    case '%':
    case '?':
        return Glyph_class_message;
    case '*':
        return Glyph_class_bang;
    case '#':
        return Glyph_class_comment;
    }
    if (glyph >= 'A' && glyph <= 'Z')
        return Glyph_class_uppercase;
    if (glyph >= 'a' && glyph <= 'z')
        return Glyph_class_lowercase;
    return Glyph_class_unknown;
}

class Orca : public MUtils::ITimeConsumer
{
public:
    Orca();
    virtual ~Orca();

    // ITimeConsumer
    virtual void Tick() override;

    void Init(ZepEditor& editor);
    void WriteToBuffer(ZepBuffer* pBuffer, ZepWindow& window);
    void ReadFromBuffer(ZepBuffer* pBuffer);
    void SetTestField(const NVec2i& fieldSize);
    void Enable(bool enable);
    void Step();
    void Quit();

    NVec2i GetSize() const { return m_size; }
    uint32_t GetFrame() const { return m_frame.load(); }
    void SetFrame(uint32_t frame) { return m_frame.store(frame); }
    bool IsZeroQuantum() const { return m_zeroQuantum; }

    bool IsEnabled() const { return m_enable.load(); }

    void RunThread(ZepEditor& editor);

private:
    long FieldIndex(long x, long y);
    long StateIndex(long x, long y, long xOff, long yOff);
    long StateIndex(long x, long y);

    void WriteState(long x, long y, long xOffset, long yOffset, uint32_t state);
    void WriteState(long x, long y,uint32_t state);
    uint32_t ReadState(long x, long y, long xOffset = 0, long yOffset = 0);

    Glyph ReadField(long x, long y, Glyph failVal = 0);
    void WriteField(long x, long y, Glyph val);

    void BuildSyntax(int x, int y, uint32_t state, Glyph glyph);
    void BuildSyntax();

private:
    ZepEditor* m_pEditor = nullptr;
    NVec2i m_size;
    
    std::mutex m_mutex;
    std::atomic_bool m_enable = false;
    std::atomic_bool m_updated = true;
    std::atomic_bool m_step = false;
    
    Field m_field;
    std::vector<uint8_t> m_lastField;
    std::vector<SyntaxResult> m_syntax;
    NVec2i m_lastCursorPos = NVec2i{ 0, 0 };

    Mbuf_reusable m_mbuf_r;
    Oevent_list m_oevent_list;
    std::atomic<uint32_t> m_frame = 0;

    double m_lastBeat = 0.0;
    bool m_zeroQuantum = true;
    MUtils::Timeline<MUtils::NoteEvent> m_timeline;
};

} // namespace Zep
