#include <gtest/gtest.h>

#include "zep/gap_buffer.h"
 
TEST(GapBuffer, PushPop)
{
    GapBuffer<char> buffer(0, 4);
    buffer.push_back('a');
    buffer.push_back('b');
    buffer.push_back('c');
    buffer.push_back('d');
    buffer.push_back('e');
    auto out = buffer.string(true);
    ASSERT_EQ(out, "abcde|4|");

    buffer.pop_back();
    buffer.pop_back();
    buffer.pop_back();
    out = buffer.string(true);
    ASSERT_EQ(out, "ab|7|");

    buffer.pop_back();
    buffer.pop_back();
    out = buffer.string(true);
    ASSERT_EQ(out, "|9|");

    // Causes annoying break in debugger when trapping asserts
    //EXPECT_DEBUG_DEATH(buffer.pop_back(), ".*!empty.*");
}

TEST(GapBuffer, FrontBack)
{
    GapBuffer<char> buffer;

    std::string foo("Hello");
    buffer.insert(buffer.begin(), foo.begin(), foo.end());

    ASSERT_EQ(buffer[0], 'H');
    ASSERT_EQ(buffer[4], 'o');

    buffer.push_back('a');
    ASSERT_EQ(buffer[5], 'a');
}

TEST(GapBuffer, Assign)
{
    GapBuffer<char> buffer(0, 4);

    std::string foo("Hello");
    buffer.assign(foo.begin(), foo.end());

    std::string out = buffer.string(true);
    ASSERT_TRUE(out == "Hello|4|");
  
    foo = "He";
    buffer.assign(foo.begin(), foo.end());
    out = buffer.string(true);
    ASSERT_TRUE(out == "He|7|");
    
    foo = "";
    buffer.assign(foo.begin(), foo.end());
    out = buffer.string(true);
    ASSERT_TRUE(out == "|9|");

    foo = "abcdefghijklmnopqrstuvwxyz";
    buffer.assign(foo.begin(), foo.end());
    out = buffer.string(true);
    ASSERT_TRUE(out == "abcdefghijklmnopqrstuvwxyz|9|");

    buffer.assign({ 'r', 'e', 'd' });
    out = buffer.string(false);
    ASSERT_TRUE(out == "red");

    buffer.assign(10, 'x');
    out = buffer.string(false);
    ASSERT_TRUE(out == "xxxxxxxxxx");
}

TEST(GapBuffer, Resize)
{
    GapBuffer<char> buffer(0, 4);

    std::string str("A sentence containing text.");
    buffer.assign(str.begin(), str.end());

    ASSERT_EQ(str.size(), buffer.size());

    buffer.resize(3);
    ASSERT_EQ(3, buffer.size());
    
    auto out = buffer.string();
    ASSERT_TRUE(out == "A s");
}

TEST(GapBuffer, Manipulations)
{
    GapBuffer<char> buffer(0, 4);

    // Empty buffer
    ASSERT_TRUE(buffer.size() == 0);

    buffer.resize(2);
    ASSERT_TRUE(buffer.size() == 2);

    auto itr = buffer.begin();
    *itr++ = '0';
    *itr++ = '1';

    std::string foo("Hello");
    buffer.insert(buffer.begin(), foo.begin(), foo.end());

    std::string out = buffer.string(true);
    ASSERT_TRUE(out == "Hello|4|01");

    auto itrErasePoint = buffer.erase(buffer.begin(), buffer.begin() + 3);
    out = buffer.string(true);
    ASSERT_TRUE(out == "|7|lo01");

    *itrErasePoint = 'c';
    out = buffer.string(true);
    ASSERT_TRUE(out == "|7|co01");

    buffer.insert(buffer.begin() + 2, foo.begin(), foo.end());
    out = buffer.string(true);
    ASSERT_TRUE(out == "coHello|2|01");

    // This insertion will make more gap space, and pad that with default gap fixed_size (4)
    foo = "A really long string";
    buffer.insert(buffer.begin() + 7, foo.begin(), foo.end());
    out = buffer.string(true);
    ASSERT_TRUE(out == "coHelloA really long string|4|01");
}
