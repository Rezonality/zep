#include "m3rdparty.h"
#include "config_app.h"
#include "mcommon/logger.h"
#include "src/buffer.h"
#include "src/display.h"
#include "src/editor.h"
#include "src/mode_standard.h"
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
        spEditor = std::make_shared<ZepEditor>(new ZepDisplayNull(), ZEP_ROOT, ZepEditorFlags::DisableThreads | ZepEditorFlags::DisableFileWatch);
        spMode = std::make_shared<ZepMode_Standard>(*spEditor);
        pBuffer = spEditor->GetEmptyBuffer("Test Buffer");

        pTabWindow = spEditor->GetActiveTabWindow();
        pWindow = spEditor->GetActiveTabWindow()->GetActiveWindow();

        // Setup editor with a default fixed_size so that text doesn't wrap and confuse the tests!
        spEditor->SetDisplayRegion(NVec2f(0.0f, 0.0f), NVec2f(1024.0f, 1024.0f));

        pWindow->SetBufferCursor(0);
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
        bool mod_marker = false;                                   \
        int mod = 0;                                               \
        for (auto& ch : command)                                   \
        {                                                          \
            if (ch == 0)                                           \
                continue;                                          \
            if (ch == '%')                                         \
            {                                                      \
                mod_marker = true;                                 \
                continue;                                          \
            }                                                      \
            if (mod_marker)                                        \
            {                                                      \
                mod_marker = false;                                \
                if (ch == 's')                                     \
                {                                                  \
                    mod |= ModifierKey::Shift;                     \
                    continue;                                      \
                }                                                  \
                else if (ch == 'c')                                \
                {                                                  \
                    mod |= ModifierKey::Ctrl;                      \
                    continue;                                      \
                }                                                  \
                else if (ch == 'r')                                \
                {                                                  \
                    spMode->AddKeyPress(ExtKeys::RIGHT, mod);      \
                    mod = 0;                                       \
                }                                                  \
                else if (ch == 'l')                                \
                {                                                  \
                    spMode->AddKeyPress(ExtKeys::LEFT, mod);       \
                    mod = 0;                                       \
                }                                                  \
            }                                                      \
            else if (ch == '\n')                                   \
            {                                                      \
                spMode->AddKeyPress(ExtKeys::RETURN, mod);         \
                mod = 0;                                           \
            }                                                      \
            else                                                   \
            {                                                      \
                spMode->AddKeyPress(ch, mod);                      \
                mod = 0;                                           \
            }                                                      \
        }                                                          \
        ASSERT_STREQ(pBuffer->GetText().string().c_str(), target); \
    };

TEST_F(StandardTest, UndoRedo)
{
    // The issue here is that setting the text _should_ update the buffer!
    pBuffer->SetText("Hello");
    spMode->AddCommandText(" ");
    spMode->Undo();
    spMode->Redo();
    spMode->Undo();
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "Hello");

    spMode->AddCommandText("iYo, ");
    spMode->Undo();
    spMode->Redo();
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "iYo, Hello");
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
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "HelloHello Goodbye");

    ASSERT_EQ(pWindow->GetBufferCursor(), 10);
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
    pBuffer->SetText("Hello");
    spMode->AddKeyPress(ExtKeys::DEL);
    spMode->AddKeyPress(ExtKeys::DEL);
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "llo");

    spMode->AddCommandText("vll");
    spMode->AddKeyPress(ExtKeys::DEL);
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "vlllo");

    pBuffer->SetText("H");
    spMode->AddKeyPress(ExtKeys::DEL);
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "H");
    
    spMode->AddKeyPress(ExtKeys::BACKSPACE);
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "");
}

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
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "llHello");
}

#define CURSOR_TEST(name, source, command, xcoord, ycoord)    \
    TEST_F(StandardTest, name)                                \
    {                                                         \
        int mod = 0;                                          \
        pBuffer->SetText(source);                             \
        bool mod_marker = false;                              \
        for (auto& ch : command)                              \
        {                                                     \
            if (ch == 0)                                      \
                continue;                                     \
            if (ch == '%')                                    \
            {                                                 \
                mod_marker = true;                            \
                continue;                                     \
            }                                                 \
            if (mod_marker)                                   \
            {                                                 \
                mod_marker = false;                           \
                if (ch == 's')                                \
                {                                             \
                    mod |= ModifierKey::Shift;                \
                    continue;                                 \
                }                                             \
                else if (ch == 'c')                           \
                {                                             \
                    mod |= ModifierKey::Ctrl;                 \
                    continue;                                 \
                }                                             \
                else if (ch == 'r')                           \
                {                                             \
                    spMode->AddKeyPress(ExtKeys::RIGHT, mod); \
                    mod = 0;                                  \
                }                                             \
                else if (ch == 'l')                           \
                {                                             \
                    spMode->AddKeyPress(ExtKeys::LEFT, mod);  \
                    mod = 0;                                  \
                }                                             \
            }                                                 \
            else if (ch == '\n')                              \
            {                                                 \
                spMode->AddKeyPress(ExtKeys::RETURN, mod);    \
                mod = 0;                                      \
            }                                                 \
            else                                              \
            {                                                 \
                spMode->AddKeyPress(ch, mod);                 \
                mod = 0;                                      \
            }                                                 \
        }                                                     \
        ASSERT_EQ(pWindow->BufferToDisplay().x, xcoord);      \
        ASSERT_EQ(pWindow->BufferToDisplay().y, ycoord);      \
    }

#define VISUAL_TEST(name, source, command, start, end)        \
    TEST_F(StandardTest, name)                                \
    {                                                         \
        int mod = 0;                                          \
        pBuffer->SetText(source);                             \
        bool mod_marker = false;                              \
        for (auto& ch : command)                              \
        {                                                     \
            if (ch == 0)                                      \
                continue;                                     \
            if (ch == '%')                                    \
            {                                                 \
                mod_marker = true;                            \
                continue;                                     \
            }                                                 \
            if (mod_marker)                                   \
            {                                                 \
                mod_marker = false;                           \
                if (ch == 's')                                \
                {                                             \
                    mod |= ModifierKey::Shift;                \
                    continue;                                 \
                }                                             \
                else if (ch == 'c')                           \
                {                                             \
                    mod |= ModifierKey::Ctrl;                 \
                    continue;                                 \
                }                                             \
                else if (ch == 'r')                           \
                {                                             \
                    spMode->AddKeyPress(ExtKeys::RIGHT, mod); \
                    mod = 0;                                  \
                }                                             \
                else if (ch == 'l')                           \
                {                                             \
                    spMode->AddKeyPress(ExtKeys::LEFT, mod);  \
                    mod = 0;                                  \
                }                                             \
            }                                                 \
            else if (ch == '\n')                              \
            {                                                 \
                spMode->AddKeyPress(ExtKeys::RETURN, mod);    \
                mod = 0;                                      \
            }                                                 \
            else                                              \
            {                                                 \
                spMode->AddKeyPress(ch, mod);                 \
                mod = 0;                                      \
            }                                                 \
        }                                                     \
        ASSERT_EQ(spMode->GetVisualRange().x, start);         \
        ASSERT_EQ(spMode->GetVisualRange().y, end);           \
    }

// NOTE: Cursor lands on the character after the shift select - i.e. the next 'Word'
CURSOR_TEST(motion_right, "one two", "%c%r", 4, 0);
CURSOR_TEST(motion_right_twice, "one two", "%c%r%c%r", 7, 0);
CURSOR_TEST(motion_right_twice_back, "one two", "%c%r%c%r%c%l", 4, 0);
CURSOR_TEST(motion_left, "one two", "%r%r%r%r%c%l", 0, 0);

// Visual Range selection
VISUAL_TEST(visual_shift_right, "one two", "%c%s%r", 0, 4);
VISUAL_TEST(visual_shift_right_right, "one two three", "%c%s%r%c%s%r", 0, 8);
VISUAL_TEST(visual_shift_right_right_back, "one two three", "%c%s%r%c%s%r%c%s%l", 0, 4);

COMMAND_TEST(paste_over, "one", "%s%r%s%r%s%r%cc%cv", "one");
COMMAND_TEST(paste_over_paste, "one", "%s%r%s%r%s%r%cc%cv%cv", "oneone");
COMMAND_TEST(paste_over_paste_paste_undo, "one", "%s%r%s%r%s%r%cc%cv%cv%cv%cz", "oneone");
