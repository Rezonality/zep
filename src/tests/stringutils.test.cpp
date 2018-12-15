#include <gtest/gtest.h>
#include "src/utils/stringutils.h"

using namespace Zep;
TEST(StringUtils, CountUtf8)
{
    ASSERT_EQ(StringUtils::CountUtf8BytesFromChar('c'), 1);
    ASSERT_EQ(StringUtils::CountUtf8BytesFromChar(0xc2ae), 3);
}

TEST(StringUtils, Utf8Length)
{
    std::vector<uint8_t> chars;
    chars.push_back('c');
    chars.push_back(0xc2);
    chars.push_back(0xae);
    ASSERT_EQ(StringUtils::Utf8Length((const char*)&chars[0]), 1);
}

TEST(StringUtils, ReplaceString)
{
    std::string strTest("I will replace this word");
    ASSERT_STREQ(StringUtils::ReplaceString(strTest, "this", "a").c_str(), "I will replace a word");
}

TEST(StringUtils, ReplaceStringInPlace)
{
    std::string strTest("I will replace this word");
    StringUtils::ReplaceStringInPlace(strTest, "this", "a");
    ASSERT_STREQ(strTest.c_str(), "I will replace a word");
}

TEST(StringUtils, Split)
{
    std::string strTest("A word on one linexAnd another");
    auto ret = StringUtils::Split(strTest, "x");
    ASSERT_EQ(ret.size(), 2);
    ASSERT_STREQ(ret[0].c_str(), "A word on one line");
    ASSERT_STREQ(ret[1].c_str(), "And another");

}

TEST(StringUtils, SplitLines)
{
    std::string strTest("A word on one line\nAnd another");
    auto ret = StringUtils::SplitLines(strTest);
    ASSERT_EQ(ret.size(), 2);
    ASSERT_STREQ(ret[0].c_str(), "A word on one line");
    ASSERT_STREQ(ret[1].c_str(), "And another");

}

TEST(StringUtils, Trim)
{
    std::string strStart("  foo  ");
    ASSERT_STREQ(StringUtils::LTrim(strStart).c_str(), "foo  ");
    ASSERT_STREQ(StringUtils::RTrim(strStart).c_str(), "foo");

    strStart = "  foo   ";
    ASSERT_STREQ(StringUtils::Trim(strStart).c_str(), "foo");
}

TEST(StringUtils, ToString)
{
    ASSERT_STREQ(StringUtils::toString(5).c_str(), "5");
    ASSERT_STREQ(StringUtils::toString(5.342f, 2).c_str(), "5.3");
    ASSERT_EQ(34, StringUtils::fromString<int>("34"));
}

TEST(StringUtils, MakeWStr)
{
    auto wstr = StringUtils::makeWStr("foo");
    ASSERT_TRUE(wstr == L"foo");
}

TEST(StringUtils, MakeStr)
{
    ASSERT_STREQ(StringUtils::makeStr(L"foo").c_str(), "foo");
}

TEST(StringUtils, ToLower)
{
    ASSERT_STREQ(StringUtils::toLower("AaBbCc").c_str(), "aabbcc");
}

TEST(StringUtils, MurmurHash)
{
    auto hash = StringUtils::murmur_hash("hello", 5, 1);
    auto inverse = StringUtils::murmur_hash_inverse(hash, 1);

    auto hash64 = StringUtils::murmur_hash_64("0123456789101112348855", 5, 1);

    ASSERT_NE(hash64, hash);
}

