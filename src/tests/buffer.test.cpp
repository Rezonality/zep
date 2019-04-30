#include "m3rdparty.h"
#include <gtest/gtest.h>
#include "src/buffer.h"

using namespace Zep;

/*
TEST(BufferTest, GetOldBlock)
{
    auto spEditor = std::make_shared<ZepEditor>();
    auto pBuffer = spEditor->AddBuffer("MyBuffer");
    pBuffer->SetText("a line of text");

    auto block = pBuffer->GetOldBlock(SearchType::WORD | SearchType::Word, 1, SearchDirection::Forward);
    ASSERT_EQ(block.firstBlock, 2);
    ASSERT_EQ(block.firstNonBlock, 6);
    ASSERT_EQ(block.secondNonBlock, 9);
   
    pBuffer->SetText("one two three");
    block = pBuffer->GetOldBlock(SearchType::WORD | SearchType::Word, 1, SearchDirection::Forward);
    ASSERT_EQ(block.firstBlock, 0);
    ASSERT_EQ(block.firstNonBlock,  3);
    ASSERT_EQ(block.secondNonBlock, 7);

    pBuffer->SetText("(one) two three");
    block = pBuffer->GetOldBlock(SearchType::WORD | SearchType::Word, 1, SearchDirection::Forward);
    ASSERT_EQ(block.firstBlock, 1);
    ASSERT_EQ(block.firstNonBlock,  4);
    ASSERT_EQ(block.secondNonBlock, 5);
    
    pBuffer->SetText("one   two three");
    block = pBuffer->GetOldBlock(SearchType::WORD | SearchType::Word, 1, SearchDirection::Forward);
    ASSERT_EQ(block.firstBlock, 0);
    ASSERT_EQ(block.firstNonBlock,  3);
    ASSERT_EQ(block.secondNonBlock, 9);
};
*/

