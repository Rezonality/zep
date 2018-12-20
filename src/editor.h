#pragma once

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <threadpool/ThreadPool.hpp>
#include <vector>

// Basic Architecture

// Editor
//      Buffers
//      Modes -> (Active BufferRegion)
// Display
//      BufferRegions (->Buffers)
//
// A buffer is just an array of chars in a gap buffer, with simple operations to insert, delete and search
// A display is something that can display a collection of regions and the editor controls in a window
// A buffer region is a single view onto a buffer inside the main display
//
// The editor has a list of ZepBuffers.
// The editor has different editor modes (vim/standard)
// ZepDisplay can render the editor (with imgui or something else)
// The display has multiple BufferRegions, each a window onto a buffer.
// Multiple regions can refer to the same buffer (N Regions : N Buffers)
// The Modes receive key presses and act on a buffer region
namespace Zep
{

class ZepBuffer;
class ZepMode;
class ZepMode_Vim;
class ZepMode_Standard;
class ZepEditor;
class ZepSyntax;
class ZepTabWindow;
class ZepWindow;

struct IZepDisplay;

class Timer;

// Helper for 2D operations
template <class T>
struct NVec2
{
    NVec2(T xVal, T yVal)
        : x(xVal)
        , y(yVal)
    {
    }

    NVec2()
        : x(0)
        , y(0)
    {
    }

    T x;
    T y;

    bool operator==(const NVec2<T>& rhs) const
    {
        if (x == rhs.x && y == rhs.y)
            return true;
        return false;
    }

    bool operator!=(const NVec2<T>& rhs) const
    {
        return !(*this = rhs);
    }
};
template <class T>
inline NVec2<T> operator+(const NVec2<T>& lhs, const NVec2<T>& rhs)
{
    return NVec2<T>(lhs.x + rhs.x, lhs.y + rhs.y);
}
template <class T>
inline NVec2<T> operator-(const NVec2<T>& lhs, const NVec2<T>& rhs)
{
    return NVec2<T>(lhs.x - rhs.x, lhs.y - rhs.y);
}
template <class T>
inline NVec2<T>& operator+=(NVec2<T>& lhs, const NVec2<T>& rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    return lhs;
}
template <class T>
inline NVec2<T>& operator-=(NVec2<T>& lhs, const NVec2<T>& rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    return lhs;
}
template <class T>
inline NVec2<T> operator*(const NVec2<T>& lhs, float val)
{
    return NVec2<T>(lhs.x * val, lhs.y * val);
}
template <class T>
inline NVec2<T>& operator*=(NVec2<T>& lhs, float val)
{
    lhs.x *= val;
    lhs.y *= val;
    return lhs;
}
template <class T>
inline NVec2<T> Clamp(const NVec2<T>& val, const NVec2<T>& min, const NVec2<T>& max)
{
    return NVec2<T>(std::min(max.x, std::max(min.x, val.x)), std::min(max.y, std::max(min.y, val.y)));
}

using NVec2f = NVec2<float>;
using NVec2i = NVec2<long>;

template <class T>
struct NVec4
{
    NVec4(T xVal, T yVal, T zVal, T wVal)
        : x(xVal)
        , y(yVal)
        , z(zVal)
        , w(wVal)
    {
    }

    NVec4(T val)
        : NVec4(val, val, val, val)
    {
    }

    NVec4()
        : x(0)
        , y(0)
        , z(0)
        , w(1)
    {
    }

    T x;
    T y;
    T z;
    T w;

    bool operator==(const NVec4<T>& rhs) const
    {
        if (x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w)
            return true;
        return false;
    }

    bool operator!=(const NVec4<T>& rhs) const
    {
        return !(*this = rhs);
    }
};
template <class T>
inline NVec4<T> operator+(const NVec4<T>& lhs, const NVec4<T>& rhs)
{
    return NVec4<T>(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
}
template <class T>
inline NVec4<T> operator-(const NVec4<T>& lhs, const NVec4<T>& rhs)
{
    return NVec4<T>(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
}
template <class T>
inline NVec4<T>& operator+=(NVec4<T>& lhs, const NVec4<T>& rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    lhs.z += rhs.z;
    lhs.w += rhs.w;
    return lhs;
}
template <class T>
inline NVec4<T>& operator-=(NVec4<T>& lhs, const NVec4<T>& rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    lhs.z -= rhs.z;
    lhs.w -= rhs.w;
    return lhs;
}
template <class T>
inline NVec4<T> operator*(const NVec4<T>& lhs, float val)
{
    return NVec4<T>(lhs.x * val, lhs.y * val, lhs.z * val, lhs.w * val);
}
template <class T>
inline NVec4<T>& operator*=(NVec4<T>& lhs, float val)
{
    lhs.x *= val;
    lhs.y *= val;
    lhs.z *= val;
    lhs.w *= val;
    return lhs;
}
template <class T>
inline NVec4<T> Clamp(const NVec4<T>& val, const NVec4<T>& min, const NVec4<T>& max)
{
    return NVec4<T>(std::min(max.x, std::max(min.x, val.x)),
        std::min(max.y, std::max(min.y, val.y)),
        std::min(max.z, std::max(min.z, val.z)),
        std::min(max.w, std::max(min.w, val.w)));
}

inline uint32_t ToPacked(const NVec4<float>& val)
{
    uint32_t col = 0;
    col |= uint32_t(val.x * 255.0f) << 24;
    col |= uint32_t(val.y * 255.0f) << 16;
    col |= uint32_t(val.z * 255.0f) << 8;
    col |= uint32_t(val.w * 255.0f);
    return col;
}

inline float Luminosity(const NVec4<float>& intensity)
{
    return (0.2126f * intensity.x + 0.7152f * intensity.y + 0.0722f * intensity.z);
}

inline NVec4<float> HSVToRGB(float h, float s, float v) 
{
	auto r = 0.0f, g = 0.0f, b = 0.0f;

	if (s == 0)
	{
		r = v;
		g = v;
		b = v;
	}
	else
	{
		int i;
		float f, p, q, t;

		if (h == 360)
			h = 0;
		else
			h = h / 60.0f;

		i = (int)trunc(h);
		f = h - i;

		p = v * (1.0f - s);
		q = v * (1.0f - (s * f));
		t = v * (1.0f - (s * (1.0f - f)));

		switch (i)
		{
		case 0:
			r = v;
			g = t;
			b = p;
			break;

		case 1:
			r = q;
			g = v;
			b = p;
			break;

		case 2:
			r = p;
			g = v;
			b = t;
			break;

		case 3:
			r = p;
			g = q;
			b = v;
			break;

		case 4:
			r = t;
			g = p;
			b = v;
			break;

		default:
			r = v;
			g = p;
			b = q;
			break;
		}

	}

    return NVec4<float>(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}

using NVec4f = NVec4<float>;
using NVec4i = NVec4<long>;

using utf8 = uint8_t;

extern const char* Msg_HandleCommand;
extern const char* Msg_Quit;
extern const char* Msg_GetClipBoard;
extern const char* Msg_SetClipBoard;

class ZepMessage
{
public:
    ZepMessage(const char* id, const std::string& strIn = std::string())
        : messageId(id)
        , str(strIn)
    {
    }

    const char* messageId; // Message ID
    std::string str; // Generic string for simple messages
    bool handled = false; // If the message was handled
};

struct IZepComponent
{
    virtual void Notify(std::shared_ptr<ZepMessage> message) = 0;
    virtual ZepEditor& GetEditor() const = 0;
};

class ZepComponent : public IZepComponent
{
public:
    ZepComponent(ZepEditor& editor);
    virtual ~ZepComponent();
    ZepEditor& GetEditor() const override
    {
        return m_editor;
    }

private:
    ZepEditor& m_editor;
};

// Registers are used by the editor to store/retrieve text fragments
struct Register
{
    Register()
        : text("")
        , lineWise(false)
    {
    }
    Register(const char* ch, bool lw = false)
        : text(ch)
        , lineWise(lw)
    {
    }
    Register(utf8* ch, bool lw = false)
        : text((const char*)ch)
        , lineWise(lw)
    {
    }
    Register(const std::string& str, bool lw = false)
        : text(str)
        , lineWise(lw)
    {
    }

    std::string text;
    bool lineWise = false;
};

using tRegisters = std::map<std::string, Register>;
using tBuffers = std::deque<std::shared_ptr<ZepBuffer>>;
using tSyntaxFactory = std::function<std::shared_ptr<ZepSyntax>(ZepBuffer*)>;

namespace ZepEditorFlags
{
enum
{
    None = (0),
    DisableThreads = (1 << 0)
};
};

const float bottomBorder = 4.0f;
const float textBorder = 4.0f;
const float leftBorder = 30.0f;

struct DisplayRegion
{
    NVec2f topLeftPx;
    NVec2f bottomRightPx;
    NVec2f BottomLeft() const
    {
        return NVec2f(topLeftPx.x, bottomRightPx.y);
    }
    NVec2f TopRight() const
    {
        return NVec2f(bottomRightPx.x, topLeftPx.y);
    }
    float Height() const
    {
        return bottomRightPx.y - topLeftPx.y;
    }
    float Width() const
    {
        return bottomRightPx.x - topLeftPx.x;
    }
    bool operator==(const DisplayRegion& region) const
    {
        return (topLeftPx == region.topLeftPx) && (bottomRightPx == region.bottomRightPx);
    }
    bool operator!=(const DisplayRegion& region) const
    {
        return !(*this == region);
    }
};

class ZepEditor
{
public:
    ZepEditor(IZepDisplay* pDisplay, uint32_t flags = 0);
    ~ZepEditor();

    void Quit();

    void InitWithFileOrDir(const std::string& str);

    ZepMode* GetCurrentMode() const;
    void Display();

    void RegisterMode(std::shared_ptr<ZepMode> spMode);
    void SetMode(const std::string& mode);
    ZepMode* GetCurrentMode();

    void RegisterSyntaxFactory(const std::string& extension, tSyntaxFactory factory);
    bool Broadcast(std::shared_ptr<ZepMessage> payload);
    void RegisterCallback(IZepComponent* pClient)
    {
        m_notifyClients.insert(pClient);
    }
    void UnRegisterCallback(IZepComponent* pClient)
    {
        m_notifyClients.erase(pClient);
    }

    const tBuffers& GetBuffers() const;
    ZepBuffer* AddBuffer(const std::string& str);
    ZepBuffer* GetMRUBuffer() const;
    void SaveBuffer(ZepBuffer& buffer);

    void SetRegister(const std::string& reg, const Register& val);
    void SetRegister(const char reg, const Register& val);
    void SetRegister(const std::string& reg, const char* pszText);
    void SetRegister(const char reg, const char* pszText);
    Register& GetRegister(const std::string& reg);
    Register& GetRegister(const char reg);
    const tRegisters& GetRegisters() const;

    void Notify(std::shared_ptr<ZepMessage> message);
    uint32_t GetFlags() const
    {
        return m_flags;
    }

    // Tab windows
    using tTabWindows = std::vector<ZepTabWindow*>;
    void SetCurrentTabWindow(ZepTabWindow* pTabWindow);
    ZepTabWindow* GetActiveTabWindow() const;
    ZepTabWindow* AddTabWindow();
    void RemoveTabWindow(ZepTabWindow* pTabWindow);
    const tTabWindows& GetTabWindows() const;

    void ResetCursorTimer();
    bool GetCursorBlinkState() const;

    void RequestRefresh();
    bool RefreshRequired() const;

    void SetCommandText(const std::string& strCommand);

    const std::vector<std::string>& GetCommandLines()
    {
        return m_commandLines;
    }

    void UpdateWindowState();

    // Setup the display size for the editor
    void SetDisplayRegion(const NVec2f& topLeft, const NVec2f& bottomRight);
    void UpdateSize();

    IZepDisplay& GetDisplay() const
    {
        return *m_pDisplay;
    }

private:
    IZepDisplay* m_pDisplay;
    std::set<IZepComponent*> m_notifyClients;
    mutable tRegisters m_registers;

    std::shared_ptr<ZepMode_Vim> m_spVimMode;
    std::shared_ptr<ZepMode_Standard> m_spStandardMode;
    std::map<std::string, tSyntaxFactory> m_mapSyntax;
    std::map<std::string, std::shared_ptr<ZepMode>> m_mapModes;

    // Blinking cursor
    std::shared_ptr<Timer> m_spCursorTimer;

    // Active mode
    ZepMode* m_pCurrentMode = nullptr;

    // Tab windows
    tTabWindows m_tabWindows;
    ZepTabWindow* m_pActiveTabWindow = nullptr;

    // List of buffers that the editor is managing
    // May or may not be visible
    tBuffers m_buffers;
    uint32_t m_flags = 0;

    mutable bool m_bPendingRefresh = true;
    mutable bool m_lastCursorBlink = false;

    std::vector<std::string> m_commandLines; // Command information, shown under the buffer

    DisplayRegion m_tabContentRegion;
    DisplayRegion m_commandRegion;
    DisplayRegion m_tabRegion;
    NVec2f m_topLeftPx;
    NVec2f m_bottomRightPx;
    bool m_bRegionsChanged = false;
};

} // namespace Zep
