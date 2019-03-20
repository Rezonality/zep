#pragma once

#include <map>
#include "file/file.h"

namespace MessageType
{

enum
{
    Info = (1 << 0),
    Warning = (1 << 1),
    Error = (1 << 2),
    Task = (1 << 3),
    System = (1 << 4)
};

} // MessageType

struct ColumnRange
{
    int32_t start;
    int32_t end;
    bool operator < (const ColumnRange& rhs) const
    {
        if (start == rhs.start)
        {
            return end < rhs.end;
        }
        return start < rhs.start;
    }
};

class UIMessage
{
public:
    UIMessage(uint32_t type, const std::string& message, const fs::path& file = fs::path(), int32_t line = -1, ColumnRange columnRange = { -1, -1 })
        : m_type(type),
        m_message(message),
        m_file(file),
        m_line(line),
        m_columnRange(columnRange),
        m_id(CurrentID++)
        {

        }

    void Log();

    bool operator < (const UIMessage& rhs) const
    {
        if (m_file == rhs.m_file)
        {
            if (m_line != -1 && rhs.m_line != -1)
            {
                if (m_line == rhs.m_line)
                {
                    // Same line, sort by column
                    return m_columnRange < rhs.m_columnRange;
                }
                else
                {
                    // Different lines, same file
                    return m_line < rhs.m_line;
                }
            }
        }
        // just sort by message order
        return m_id < rhs.m_id;
    }

    const uint32_t m_type;
    const std::string m_message;
    const fs::path m_file;
    const int32_t m_line;
    const ColumnRange m_columnRange;
    const uint64_t m_id;
private:
    static uint64_t CurrentID;
};

class UIManager
{
public:
    static UIManager& Instance();
    UIManager();

    UIManager(const UIManager& rhs) = delete;
    const UIManager& operator = (const UIManager& rhs) = delete;

    uint64_t AddMessage(uint32_t type, const std::string& message, const fs::path& file = fs::path(), int32_t line = -1, const ColumnRange& columnRange = ColumnRange{ -1, -1 });
    void RemoveMessage(uint64_t id);
    void ClearFileMessages(fs::path path);

private:
    std::map<uint64_t, std::shared_ptr<UIMessage>> m_taskMessages;
    std::map<fs::path, std::vector<uint64_t>> m_fileMessages;
};
