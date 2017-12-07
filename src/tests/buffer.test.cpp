#include "m3rdparty.h"
#include <gtest/gtest.h>
#include "src/buffer.h"

using namespace Zep;

TEST(BufferTest, GetBlock)
{
    auto spEditor = std::make_shared<ZepEditor>();
    auto spBuffer = spEditor->AddBuffer("MyBuffer");
    spBuffer->SetText("a line of text");

    auto block = spBuffer->GetBlock(SearchType::Word | SearchType::AlphaNumeric, 1, SearchDirection::Forward);
    ASSERT_EQ(block.firstBlock, 2);
    ASSERT_EQ(block.firstNonBlock, 6);
    ASSERT_EQ(block.secondNonBlock, 9);
   
    spBuffer->SetText("one two three");
    block = spBuffer->GetBlock(SearchType::Word | SearchType::AlphaNumeric, 1, SearchDirection::Forward);
    ASSERT_EQ(block.firstBlock, 0);
    ASSERT_EQ(block.firstNonBlock,  3);
    ASSERT_EQ(block.secondNonBlock, 7);

    spBuffer->SetText("(one) two three");
    block = spBuffer->GetBlock(SearchType::Word | SearchType::AlphaNumeric, 1, SearchDirection::Forward);
    ASSERT_EQ(block.firstBlock, 1);
    ASSERT_EQ(block.firstNonBlock,  4);
    ASSERT_EQ(block.secondNonBlock, 5);
    
    spBuffer->SetText("one   two three");
    block = spBuffer->GetBlock(SearchType::Word | SearchType::AlphaNumeric, 1, SearchDirection::Forward);
    ASSERT_EQ(block.firstBlock, 0);
    ASSERT_EQ(block.firstNonBlock,  3);
    ASSERT_EQ(block.secondNonBlock, 9);
};

