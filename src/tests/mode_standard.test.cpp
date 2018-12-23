#include "m3rdparty.h"
#include "src/buffer.h"
#include "src/display.h"
#include "src/editor.h"
#include "src/mode_standard.h"
#include "src/syntax_glsl.h"
#include "src/tab_window.h"
#include "src/window.h"
#include <gtest/gtest.h>

using namespace Zep;
class StandardTest : public testing::Test
{
public:
    StandardTest()
    {
        // Disable threads for consistent tests, at the expense of not catching thread errors!
        // TODO : Fix/understand test failures with threading
        spEditor = std::make_shared<ZepEditor>(new ZepDisplayNull(), ZepEditorFlags::DisableThreads);
        spMode = std::make_shared<ZepMode_Standard>(*spEditor);
        pBuffer = spEditor->AddBuffer("Test Buffer");

        // Add a syntax highlighting checker, to increase test coverage
        // (seperate tests to come)
        auto spSyntax = std::make_shared<ZepSyntaxGlsl>(*pBuffer);
        pBuffer->SetSyntax(std::static_pointer_cast<ZepSyntax>(spSyntax));

        pTabWindow = spEditor->GetActiveTabWindow();
        pWindow = spEditor->GetActiveTabWindow()->GetActiveWindow();

        // Setup editor with a default size so that text doesn't wrap and confuse the tests!
        spEditor->SetDisplayRegion(NVec2f(0.0f, 0.0f), NVec2f(1024.0f, 1024.0f));

        pWindow->SetBufferCursor(0);

        //Quicker testing
        //COMMAND_TEST(delete_cw, "one two three", "cwabc", "abc two three");
        //COMMAND_TEST(delete_ciw_on_newline, "one\n\nthree", "jciwtwo", "one\ntwo\nthree");
        /*pBuffer->SetText("one\n\nthree");
        spEditor->Display(*spDisplay);
        spMode->AddCommandText("jciwtwo");               
        assert(pBuffer->GetText().string() == "abc two three");
        */
    }

    ~StandardTest()
    {
    }

public:
    std::shared_ptr<ZepEditor> spEditor;
    ZepBuffer* pBuffer;
    ZepWindow* pWindow;
    ZepTabWindow* pTabWindow;
    std::shared_ptr<ZepMode_Standard> spMode;
};

TEST_F(StandardTest, CheckDisplaySucceeds)
{
    pBuffer->SetText("Some text to display\nThis is a test.");
    spEditor->SetDisplayRegion(NVec2f(0.0f, 0.0f), NVec2f(1024.0f, 1024.0f));
    ASSERT_NO_FATAL_FAILURE(spEditor->Display());
    ASSERT_FALSE(pTabWindow->GetWindows().empty());
}

// Given a sample text, a keystroke list and a target text, check the test returns the right thing
#define COMMAND_TEST(name, source, command, target)                \
    TEST_F(StandardTest, name)                                     \
    {                                                              \
        pBuffer->SetText(source);                                  \
        spMode->AddCommandText(command);                           \
        ASSERT_STREQ(pBuffer->GetText().string().c_str(), target); \
    };

#define COMMAND_TEST_RET(name, source, command, target)            \
    TEST_F(StandardTest, name)                                     \
    {                                                              \
        pBuffer->SetText(source);                                  \
        spMode->AddCommandText(command);                           \
        spMode->AddKeyPress(ExtKeys::RETURN);                      \
        ASSERT_STREQ(pBuffer->GetText().string().c_str(), target); \
    };

TEST_F(StandardTest, UndoRedo)
{
    // The issue here is that setting the text _should_ update the buffer!
    /*
    pBuffer->SetText("Hello");
    spMode->AddCommandText(" ");
    spMode->Undo();
    spMode->Redo();
    spMode->Undo();
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "Hello");

    spMode->AddCommandText("iYo, ");
    spMode->Undo();
    spMode->Redo();
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "Yo, Hello");
    */
}

TEST_F(StandardTest, copy_pasteover_paste)
{
    // The issue here is that setting the text _should_ update the buffer!
    pBuffer->SetText("Hello Goodbye");
    spMode->AddKeyPress(ExtKeys::RIGHT, ModifierKey::Shift);
    spMode->AddKeyPress(ExtKeys::RIGHT, ModifierKey::Shift);
    spMode->AddKeyPress(ExtKeys::RIGHT, ModifierKey::Shift);
    spMode->AddKeyPress(ExtKeys::RIGHT, ModifierKey::Shift);
    spMode->AddKeyPress(ExtKeys::RIGHT, ModifierKey::Shift);
    spMode->AddKeyPress('c', ModifierKey::Ctrl);

    spMode->AddKeyPress('v', ModifierKey::Ctrl);
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "Hello Goodbye");
    
    spMode->AddKeyPress('v', ModifierKey::Ctrl);
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "Hello Hello Goodbye");
}

TEST_F(StandardTest, down_a_shorter_line)
{
    // The issue here is that setting the text _should_ update the buffer!
    pBuffer->SetText("Hello Goodbye\nF");
    spMode->AddKeyPress(ExtKeys::RIGHT);
    spMode->AddKeyPress(ExtKeys::RIGHT);
    spMode->AddKeyPress(ExtKeys::RIGHT);
    spMode->AddKeyPress(ExtKeys::RIGHT);
    spMode->AddKeyPress(ExtKeys::DOWN);
    spMode->AddKeyPress('o');
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "Hello Goodbye\nFo");
}

TEST_F(StandardTest, DELETE)
{
    /*
    pBuffer->SetText("Hello");
    spMode->AddKeyPress(ExtKeys::DEL);
    spMode->AddKeyPress(ExtKeys::DEL);
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "llo");

    spMode->AddCommandText("vll");
    spMode->AddKeyPress(ExtKeys::DEL);
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "");

    pBuffer->SetText("H");
    spMode->AddKeyPress(ExtKeys::DEL);
    spMode->AddKeyPress(ExtKeys::DEL);
    */
}

TEST_F(StandardTest, RETURN)
{
    /*spBuffer->SetText("one\ntwo");
    spMode->AddKeyPress(ExtKeys::RETURN);
    ASSERT_EQ(pWindow->GetCursor().y, 1);

    spMode->AddCommandText("li");
    spMode->AddKeyPress(ExtKeys::RETURN);
    ASSERT_STREQ(spBuffer->GetText().string().c_str(), "ne\ntwo");
    */
}

/*
TEST_F(StandardTest, BACKSPACE)
{
    pBuffer->SetText("Hello");
    spMode->AddCommandText("ll");
    spMode->AddKeyPress(ExtKeys::BACKSPACE);
    spMode->AddKeyPress(ExtKeys::BACKSPACE);
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "Hello");
    ASSERT_EQ(pWindow->GetBufferCursor(), 0);

    spMode->AddCommandText("lli");
    spMode->AddKeyPress(ExtKeys::BACKSPACE);
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "Hllo");
}
#define CURSOR_TEST(name, source, command, xcoord, ycoord) \
    TEST_F(VimTest, name)                                  \
    {                                                      \
        pBuffer->SetText(source);                          \
        for (auto& ch : command)                           \
        {                                                  \
            if (ch == '\n')                                \
            {                                              \
                spMode->AddKeyPress(ExtKeys::RETURN);      \
            }                                              \
            else                                            \
            {                                               \
                spMode->AddKeyPress(ch);                     \
            }                                                 \
        }                                                      \
        ASSERT_EQ(pWindow->BufferToDisplay().x, xcoord);       \
        ASSERT_EQ(pWindow->BufferToDisplay().y, ycoord);        \
    } ;

// Motions
CURSOR_TEST(motion_lright, "one", "ll", 2, 0);
CURSOR_TEST(motion_cr_then_escape, "one", "$a\njk", 0, 1);
*/
