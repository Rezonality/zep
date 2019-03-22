/*
 * https://stackoverflow.com/questions/5028302/small-logger-class
 * File:   Log.h
 * Author: Alberto Lepe <dev@alepe.com>
 *
 * Created on December 1, 2015, 6:00 PM
 * Modified by cmaughan
 */

#pragma once
#include "utils.h"
#include <iostream>
#include <sstream>

#ifdef WIN32
// A reference to the debug API on windows, to help the logger output in VC.  This is better
// than out to the console sometimes, and as long as you are building on Windows, you are referencing the necessary
// kernel32.dll....
extern "C" {
__declspec(dllimport) void __stdcall OutputDebugStringA(_In_opt_ const char* pszChar);
}
#endif

#undef ERROR

namespace Mgfx
{

enum LogType
{
    NONE,
    DEBUG,
    INFO,
    WARN,
    WARNING,
    ERROR
};

struct Logger
{
    bool headers = false;
    LogType level = WARN;
};

extern Logger logger;

class LOG
{
public:
    LOG()
    {
    }
    LOG(LogType type)
    {
        msglevel = type;
        if (logger.headers && msglevel >= logger.level)
        {
            operator<<("[" + getLabel(type) + "] ");
        }
    }
    ~LOG()
    {
        if (opened)
        {
            out << std::endl;
#ifdef WIN32
            OutputDebugStringA(out.str().c_str());
#else
            std::cout << out.str();
#endif
        }
        opened = false;
    }
    template <class T>
    LOG& operator<<(const T& msg)
    {
        if (disabled || msglevel < logger.level)
            return *this;
        out << msg;
        opened = true;
        return *this;
    }

    static bool disabled;
private:
    bool opened = false;
    LogType msglevel = DEBUG;
    inline std::string getLabel(LogType type)
    {
        std::string label;
        switch (type)
        {
            case DEBUG:
                label = "DEBUG";
                break;
            case INFO:
                label = "INFO ";
                break;
            case WARN:
            case WARNING:
                label = "WARN ";
                break;
            case ERROR:
                label = "ERROR";
                break;
        }
        return label;
    }
    std::ostringstream out;
};

} // namespace Mgfx
