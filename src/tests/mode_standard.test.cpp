#include "config_app.h"
#include "zep/mcommon/logger.h"

#include "zep/buffer.h"
#include "zep/display.h"
#include "zep/editor.h"
#include "zep/mode_standard.h"
#include "zep/tab_window.h"
#include "zep/window.h"

#include <gtest/gtest.h>

using namespace Zep;
class StandardTest : public testing::Test
{
public:
    StandardTest()
    {
        // Disable threads for consistent tests, at the expense of not catching thread errors!
        // TODO : Fix/understand test failures with threading
        spEditor = std::make_shared<ZepEditor>(new ZepDisplayNull(NVec2f(1.0f, 1.0f)), ZEP_ROOT, ZepEditorFlags::DisableThreads);
        pBuffer = spEditor->InitWithText("Test Buffer", "");

        pTabWindow = spEditor->GetActiveTabWindow();
        pWindow = spEditor->GetActiveTabWindow()->GetActiveWindow();

        // Setup editor with a default fixed_size so that text doesn't wrap and confuse the tests!
        spEditor->SetDisplayRegion(NVec2f(0.0f, 0.0f), NVec2f(1024.0f, 1024.0f));

        pWindow->SetBufferCursor(pBuffer->Begin());

        spEditor->SetGlobalMode(Zep::ZepMode_Standard::StaticName());
        spMode = spEditor->GetGlobalMode();
    }

    ~StandardTest()
    {
    }

public:
    std::shared_ptr<ZepEditor> spEditor;
    ZepBuffer* pBuffer;
    ZepWindow* pWindow;
    ZepTabWindow* pTabWindow;
    ZepMode* spMode;
};

TEST_F(StandardTest, CheckDisplaySucceeds)
{
    pBuffer->SetText("Some text to display\nThis is a test.");
    spEditor->SetDisplayRegion(NVec2f(0.0f, 0.0f), NVec2f(1024.0f, 1024.0f));
    ASSERT_NO_FATAL_FAILURE(spEditor->Display());
    ASSERT_FALSE(pTabWindow->GetWindows().empty());
}

#define PARSE_COMMAND(command)                                \
    bool mod_marker = false;                                  \
    int mod = 0;                                              \
    for (auto& ch : command)                                  \
    {                                                         \
        if (ch == 0)                                          \
            continue;                                         \
        if (ch == '%')                                        \
        {                                                     \
            mod_marker = true;                                \
            continue;                                         \
        }                                                     \
        if (mod_marker)                                       \
        {                                                     \
            mod_marker = false;                               \
            if (ch == 's')                                    \
            {                                                 \
                mod |= ModifierKey::Shift;                    \
                continue;                                     \
            }                                                 \
            else if (ch == 'c')                               \
            {                                                 \
                mod |= ModifierKey::Ctrl;                     \
                continue;                                     \
            }                                                 \
            else if (ch == 'r')                               \
            {                                                 \
                spMode->AddKeyPress(ExtKeys::RIGHT, mod);     \
                mod = 0;                                      \
            }                                                 \
            else if (ch == 'l')                               \
            {                                                 \
                spMode->AddKeyPress(ExtKeys::LEFT, mod);      \
                mod = 0;                                      \
            }                                                 \
            else if (ch == 'u')                               \
            {                                                 \
                spMode->AddKeyPress(ExtKeys::UP, mod);        \
                mod = 0;                                      \
            }                                                 \
            else if (ch == 'd')                               \
            {                                                 \
                spMode->AddKeyPress(ExtKeys::DOWN, mod);      \
                mod = 0;                                      \
            }                                                 \
            else if (ch == 'x')                               \
            {                                                 \
                spMode->AddKeyPress(ExtKeys::BACKSPACE, mod); \
                mod = 0;                                      \
            }                                                 \
        }                                                     \
        else if (ch == '\n')                                  \
        {                                                     \
            spMode->AddKeyPress(ExtKeys::RETURN, mod);        \
            mod = 0;                                          \
        }                                                     \
        else                                                  \
        {                                                     \
            spMode->AddKeyPress(ch, mod);                     \
            mod = 0;                                          \
        }                                                     \
    }

// TODO
// These tests were written before i added conversion of modifiers from Ext to <C-f>, etc.
// So this odd syntax probably isn't necessary, and we could call the mapped function with the
// decoded keystrokes
// Given a sample text, a keystroke list and a target text, check the test returns the right thing
#define COMMAND_TEST(name, source, command, target)                     \
    TEST_F(StandardTest, name)                                          \
    {                                                                   \
        pBuffer->SetText(source);                                       \
        PARSE_COMMAND(command)                                          \
        ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), target); \
    };

#define CURSOR_TEST(name, source, command, xcoord, ycoord) \
    TEST_F(StandardTest, name)                             \
    {                                                      \
        pBuffer->SetText(source);                          \
        PARSE_COMMAND(command);                            \
        ASSERT_EQ(pWindow->BufferToDisplay().x, xcoord);   \
        ASSERT_EQ(pWindow->BufferToDisplay().y, ycoord);   \
    }

#define VISUAL_TEST(name, source, command, start, end)                      \
    TEST_F(StandardTest, name)                                              \
    {                                                                       \
        pBuffer->SetText(source);                                           \
        PARSE_COMMAND(command)                                              \
        ASSERT_EQ(spMode->GetInclusiveVisualRange().first.Index(), start); \
        ASSERT_EQ(spMode->GetInclusiveVisualRange().second.Index(), end);  \
    }

TEST_F(StandardTest, UndoRedo)
{
    // The issue here is that setting the text _should_ update the buffer!
    pBuffer->SetText("Hello");
    spMode->AddCommandText(" ");
    spMode->Undo();
    spMode->Redo();
    spMode->Undo();
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "Hello");

    spMode->AddCommandText("iYo, ");
    spMode->Undo();
    spMode->Redo();
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "iYo, Hello");
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
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "Hello Goodbye");

    // Note this is incorrect for what we expect, but a side effect of the test: Fix it.
    // The actual behavior in the editor is correct!
    spMode->AddKeyPress('v', ModifierKey::Ctrl);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "HelloHello Goodbye");

    ASSERT_EQ(pWindow->GetBufferCursor().Index(), 10);
}

TEST_F(StandardTest, BackToInsertIfShiftReleased)
{
    // The issue here is that setting the text _should_ update the buffer!
    pBuffer->SetText("abc");
    spMode->AddKeyPress(ExtKeys::RIGHT, ModifierKey::Shift);
    ASSERT_EQ(spMode->GetEditorMode(), EditorMode::Visual);
    spMode->AddKeyPress(ExtKeys::RIGHT);
    ASSERT_EQ(spMode->GetEditorMode(), EditorMode::Insert);
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
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "Hello Goodbye\nFo");
}

TEST_F(StandardTest, DELETE)
{
    pBuffer->SetText("Hello");
    spMode->AddKeyPress(ExtKeys::DEL);
    spMode->AddKeyPress(ExtKeys::DEL);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "llo");

    spMode->AddCommandText("vll");
    spMode->AddKeyPress(ExtKeys::DEL);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "vlllo");

    // Doesn't delete H because the cursor was previously at the end?
    // Is this a behavior expectation or a bug?  Should the cursor clamp to the previously
    // set text end, or reset to 0??
    pBuffer->SetText("H");
    spMode->AddKeyPress(ExtKeys::DEL);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "H");

    spMode->AddKeyPress(ExtKeys::BACKSPACE);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "");
}

TEST_F(StandardTest, BACKSPACE)
{
    pBuffer->SetText("Hello");
    spMode->AddCommandText("ll");
    spMode->AddKeyPress(ExtKeys::BACKSPACE);
    spMode->AddKeyPress(ExtKeys::BACKSPACE);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "Hello");
    ASSERT_EQ(pWindow->GetBufferCursor().Index(), 0);

    spMode->AddCommandText("lli");
    spMode->AddKeyPress(ExtKeys::BACKSPACE);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "llHello");
}

CURSOR_TEST(motion_right, "one two", "%r", 1, 0);
CURSOR_TEST(motion_left, "one two", "%r%r%l", 1, 0);
CURSOR_TEST(motion_left_over_newline, "one\ntwo", "%d%r%r%l%l%l", 3, 0);
CURSOR_TEST(motion_right_over_newline, "one\ntwo", "%r%r%r%r%r", 1, 1);
CURSOR_TEST(motion_down, "one\ntwo", "%d", 0, 1);
CURSOR_TEST(motion_up, "one\ntwo", "%d%u", 0, 0);

// NOTE: Cursor lands on the character after the shift select - i.e. the next 'Word'
// These are CTRL+-> -< movements, tested for comparison with notepad behavior.
CURSOR_TEST(motion_right_word, "one two", "%c%r", 4, 0);
CURSOR_TEST(motion_right_twice_word, "one two", "%c%r%c%r", 7, 0);
CURSOR_TEST(motion_right_twice_back_word, "one two", "%c%r%c%r%c%l", 4, 0);
CURSOR_TEST(motion_left_word, "one two", "%r%r%r%r%c%l", 0, 0);
CURSOR_TEST(motion_right_newline, "one\ntwo", "%c%r", 3, 0);
CURSOR_TEST(motion_right_newline_twice, "one\ntwo", "%c%r%c%r", 0, 1);
CURSOR_TEST(motion_right_newline_twice_back, "one\ntwo", "%c%r%c%r%c%l", 3, 0);
CURSOR_TEST(motion_right_newline_twice_back_back, "one\ntwo", "%c%r%c%r%c%l%c%l", 0, 0);

CURSOR_TEST(paste_over_cursor_after, "one", "%c%s%r%cc%cv", 3, 0);

// Visual Range selection
VISUAL_TEST(visual_shift_right, "one two", "%c%s%r", 0, 3);
VISUAL_TEST(visual_shift_right_right, "one two three", "%c%s%r%c%s%r", 0, 7);
VISUAL_TEST(visual_shift_right_right_back, "one two three", "%c%s%r%c%s%r%c%s%l", 0, 3);

COMMAND_TEST(paste_over, "one", "%s%r%s%r%s%r%cc%cv", "one");
COMMAND_TEST(paste_over_paste, "one", "%s%r%s%r%s%r%cc%cv%cv", "oneone");
COMMAND_TEST(paste_over_paste_paste_undo, "one", "%s%r%s%r%s%r%cc%cv%cv%cv%cz", "oneone");

COMMAND_TEST(delete_back_to_previous_line, "one\n\ntwo", "%d%d%x", "one\ntwo");
