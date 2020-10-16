#include "config_app.h"
#include "zep/mcommon/logger.h"

#include "zep/buffer.h"
#include "zep/display.h"
#include "zep/editor.h"
#include <gtest/gtest.h>

using namespace Zep;
class BufferTest : public testing::Test
{
public:
    BufferTest()
    {
        // Disable threads for consistent tests, at the expense of not catching thread errors!
        // TODO : Fix/understand test failures with threading
        spEditor = std::make_shared<ZepEditor>(new ZepDisplayNull(NVec2f(1.0f, 1.0f)), ZEP_ROOT, ZepEditorFlags::DisableThreads);
        pBuffer = spEditor->InitWithText("", "");
    }

    ~BufferTest()
    {
    }

public:
    std::shared_ptr<ZepEditor> spEditor;
    ZepBuffer* pBuffer;
};

TEST_F(BufferTest, CreatedProperly)
{
    ASSERT_TRUE(pBuffer->GetWorkingBuffer().size() == 1);
}

TEST_F(BufferTest, DefaultConstructedWith0)
{
    auto pNew = std::make_shared<ZepBuffer>(*spEditor, std::string("empty"));
    ASSERT_TRUE(pNew->GetWorkingBuffer().size() == 1);
}

// TODO
