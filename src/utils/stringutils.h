#pragma once

#include <iomanip>
#include <vector>
#include <sstream>

namespace StringUtils
{

inline size_t CountUtf8BytesFromChar(unsigned int c)
{
    if (c < 0x80) return 1;
    if (c < 0x800) return 2;
    if (c >= 0xdc00 && c < 0xe000) return 0;
    if (c >= 0xd800 && c < 0xdc00) return 4;
    return 3;
}

inline size_t Utf8Length(const char* s)
{
    size_t len = 1;
    while (len <= 4 && *s)
    {
        if ((*s++ & 0xc0) != 0x80)
            break;
        len++;
    }
    return len;
}

std::string ReplaceString(std::string subject, const std::string& search, const std::string& replace);
std::vector<std::string> Split(const std::string& text, const std::string& delims);
std::vector<std::string> SplitLines(const std::string& text);

// trim from beginning of string (left)
inline std::string& LTrim(std::string& s, const char* t = " \t\n\r\f\v")
{
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from end of string (right)
inline std::string& RTrim(std::string& s, const char* t = " \t\n\r\f\v")
{
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from both ends of string (left & right)
inline std::string& Trim(std::string& s, const char* t = " \t\n\r\f\v")
{
    return LTrim(RTrim(s, t), t);
}

template<typename T>
std::string toString(const T &t) {
    std::ostringstream oss;
    oss << t;
    return oss.str();
}

template<typename T>
std::string toString(const T &t, int precision) {
    std::ostringstream oss;
    oss << std::setprecision(precision) << t;
    return oss.str();
}
template<typename T>
T fromString(const std::string& s) {
    std::istringstream stream(s);
    T t;
    stream >> t;
    return t;
}

inline std::wstring makeWStr(const std::string& str)
{
    return std::wstring(str.begin(), str.end());
}

std::string makeStr(const std::wstring& str);

std::string toLower(const std::string& str);

unsigned int murmur_hash_inverse(unsigned int h, unsigned int seed);
uint64_t murmur_hash_64(const void * key, uint32_t len, uint64_t seed);
uint32_t murmur_hash(const void * key, int len, uint32_t seed);

} // StringUtils