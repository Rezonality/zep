#include "m3rdparty.h"
#include <gtest/gtest.h>
#include "src/editor.h"
#include "src/mode_vim.h"
#include "src/buffer.h"
#include "src/display.h"
#include "src/syntax_glsl.h"
#include "src/tab_window.h"
#include "src/window.h"

using namespace Zep;
class VimTest : public testing::Test
{
public:
    VimTest()
    {
        // Disable threads for consistent tests, at the expense of not catching thread errors!
        spEditor = std::make_shared<ZepEditor>(ZepEditorFlags::DisableThreads);
        spMode = std::make_shared<ZepMode_Vim>(*spEditor);
        pBuffer = spEditor->AddBuffer("Test Buffer");

        // Add a syntax highlighting checker, to increase test coverage
        // (seperate tests to come)
        auto spSyntax = std::make_shared<ZepSyntaxGlsl>(*pBuffer);
        pBuffer->SetSyntax(std::static_pointer_cast<ZepSyntax>(spSyntax));

        // Some vim commands depend on the shape of the view onto the buffer.  For example, 
        // moving 'down' might depend on word wrapping, etc. as to where in the buffer you actually land.
        // This is the reason for the difference between a 'Window' and a 'Buffer' (as well as that you can
        // have multiple windows onto a buffer)
        spDisplay = std::make_shared<ZepDisplayNull>(*spEditor);
        spDisplay->SetDisplaySize(NVec2f(0.0f, 0.0f), NVec2f(1024.0f, 1024.0f));

        // Since the tests don't display anything, ensure that the window state is up to date  before running them; 
        // this is usually done at display time, but some tests rely on window state/cursor limits being the same as if they 
        // were displaying the data
        spDisplay->Display();

        pTabWindow = spEditor->GetActiveTabWindow();
        pWindow = spEditor->GetActiveTabWindow()->GetActiveWindow();

        pWindow->MoveCursor(0);
    }

    ~VimTest()
    {
    }

public:
    std::shared_ptr<ZepEditor> spEditor;
    std::shared_ptr<ZepDisplayNull> spDisplay;
    ZepBuffer* pBuffer;
    ZepWindow* pWindow;
    ZepTabWindow* pTabWindow;
    std::shared_ptr<ZepMode_Vim> spMode;
};

TEST_F(VimTest, CheckDisplaySucceeds)
{
    pBuffer->SetText("Some text to display\nThis is a test.");
    spDisplay->SetDisplaySize(NVec2f(0.0f, 0.0f), NVec2f(50.0f, 1024.0f));
    ASSERT_NO_FATAL_FAILURE(spDisplay->Display());
    ASSERT_FALSE(pTabWindow->GetWindows().empty());
}

TEST_F(VimTest, CheckDisplayWrap)
{
    pBuffer->SetText("Some text to display\nThis is a test.");
    ASSERT_NO_FATAL_FAILURE(spDisplay->Display());
    ASSERT_FALSE(pTabWindow->GetWindows().empty());
}
// Given a sample text, a keystroke list and a target text, check the test returns the right thing
#define COMMAND_TEST(name, source, command, target) \
TEST_F(VimTest, name)                              \
{                                                   \
    pBuffer->SetText(source);                      \
    spMode->AddCommandText(command);                \
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), target);     \
};

#define COMMAND_TEST_RET(name, source, command, target) \
TEST_F(VimTest, name)                              \
{                                                   \
    pBuffer->SetText(source);                      \
    spMode->AddCommandText(command);                \
    spMode->AddKeyPress(ExtKeys::RETURN);        \
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), target);     \
};

#define SET_TEXT(txt) { pBuffer->SetText(txt); spDisplay->Display(); }

TEST_F(VimTest, UndoRedo)
{
    // The issue here is that setting the text _should_ update the buffer!
    pBuffer->SetText("Hello");
    spMode->AddCommandText("3x");
    spMode->Undo();
    spMode->Redo();
    spMode->Undo();
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "Hello");

    spMode->AddCommandText("iYo, jk");
    spMode->Undo();
    spMode->Redo();
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "Yo, Hello");
}

TEST_F(VimTest, DELETE)
{
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
}

TEST_F(VimTest, ESCAPE)
{
    pBuffer->SetText("Hello");
    spMode->AddCommandText("iHi, ");
    spMode->AddKeyPress(ExtKeys::ESCAPE);
    spMode->Undo();
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "Hello");
}

TEST_F(VimTest, RETURN)
{
    /*spBuffer->SetText("one\ntwo");
    spMode->AddKeyPress(ExtKeys::RETURN);
    ASSERT_EQ(pWindow->GetCursor().y, 1);

    spMode->AddCommandText("li");
    spMode->AddKeyPress(ExtKeys::RETURN);
    ASSERT_STREQ(spBuffer->GetText().string().c_str(), "ne\ntwo");
    */
}

TEST_F(VimTest, TAB)
{
    pBuffer->SetText("Hello");
    spMode->AddCommandText("lllllllli");
    spMode->AddKeyPress(ExtKeys::TAB);
    ASSERT_STREQ(pBuffer->GetText().string().c_str(), "Hell    o");
}

TEST_F(VimTest, BACKSPACE)
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
// The various rules of vim keystrokes are hard to consolidate.
// diw deletes inner word for example, but also deletes just whitespace if you are on it.
// daw deletes the whole word, but also the first spaces up to it, or the last spaces after it....
// The best way to check these behaviours is of course with some unit that check each behaviour
// These are below.  I wil likely never write enough of these!

// Dot!
COMMAND_TEST(dot_ciw, "four two three", "ciwfourjkl.l.", "four three");
COMMAND_TEST(dot_ciw_step, "four two three", "ciwfourjklll.lll.", "four four four");
COMMAND_TEST(dot_3x, "four two three", "l3xll.", "f tthree");
COMMAND_TEST(dot_d2w_count3, "one two three four five", "d2w3.", "");

// daw
COMMAND_TEST(delete_daw_single_char, "(one) two three", "daw", "one) two three");
COMMAND_TEST(delete_daw_start_space, "(one) two three", "llldaw", "() two three");
COMMAND_TEST(delete_daw_inside_string, "(one) two three", "llllldaw", "(one) three");
COMMAND_TEST(delete_daw_2, "(one) two three", "2daw", ") two three");
COMMAND_TEST(delete_2d2d, "one\ntwo\nthree\nfour\nfive\nsix", "2d2d", "five\nsix");

// daW
COMMAND_TEST(delete_daW_single_char, "(one) two three", "daW", "two three");
COMMAND_TEST(delete_daW_start_space, "(one) two three", "llldaW", "two three");
COMMAND_TEST(delete_daW_inside_string, "(one) two three", "llllldaW", "(one) three");
COMMAND_TEST(delete_daW_2, "one two three", "2daW", "three");

// diw
COMMAND_TEST(delete_diw_first, "one two three", "diw", " two three");
COMMAND_TEST(delete_diw_start_space, "one two three", "llldiw", "onetwo three");
COMMAND_TEST(delete_diw_inner_spaces, "one    two three", "llllldiw", "onetwo three");
COMMAND_TEST(delete_diw_inside_string, "one two three", "ldiw", " two three");
COMMAND_TEST(delete_diw_2, "one two three", "2diw", "two three");
COMMAND_TEST(delete_diw_space_start, " one two three", "diw", "one two three");

// diW
COMMAND_TEST(delete_diW_first, "one! two three", "diW", " two three");

// dw
COMMAND_TEST(delete_dw, "one two three", "dw", "two three");
COMMAND_TEST(delete_dw_inside, "one two three", "ldw", "otwo three");
COMMAND_TEST(delete_dw_inside_2, "one two three", "lllllllldw", "one two ");

// dW
COMMAND_TEST(delete_dW, "one two! three", "lllldW", "one three");

// caw
COMMAND_TEST(delete_caw_single_char, "(one) two three", "cawabc", "abcone) two three");
COMMAND_TEST(delete_caw_start_space, "(one) two three", "lllcawabc", "(abc) two three");
COMMAND_TEST(delete_caw_inside_string, "(one) two three", "lllllcawabc", "(one)abc three");
COMMAND_TEST(delete_caw_2, "(one) two three", "2cawabc", "abc) two three");

// caW
COMMAND_TEST(delete_caW_single_char, "(one) two three", "caWabc", "abctwo three");
COMMAND_TEST(delete_caW_start_space, "(one) two three", "lllcaWabc", "abctwo three");
COMMAND_TEST(delete_caW_inside_string, "(one) two three", "lllllcaWabc", "(one)abc three");
COMMAND_TEST(delete_caW_2, "one two three", "2caWabc", "abcthree");

// ciw
COMMAND_TEST(delete_ciw_first, "one two three", "ciwabc", "abc two three");
COMMAND_TEST(delete_ciw_start_space, "one two three", "lllciwabc", "oneabctwo three");
COMMAND_TEST(delete_ciw_inner_spaces, "one    two three", "lllllciwabc", "oneabctwo three");
COMMAND_TEST(delete_ciw_inside_string, "one two three", "lciwabc", "abc two three");
COMMAND_TEST(delete_ciw_2, "one two three", "2ciwabc", "abctwo three");
COMMAND_TEST(delete_ciw_space_start, " one two three", "ciwabc", "abcone two three");

// ciW
COMMAND_TEST(delete_ciW_first, "one! two three", "ciWabc", "abc two three");

// cw
COMMAND_TEST(delete_cw, "one two three", "cwabc", "abc two three");
COMMAND_TEST(delete_cw_inside, "one two three", "lcwabc", "oabc two three");
COMMAND_TEST(delete_cw_inside_2, "one two three", "llllllllcwabc", "one two abc");

// TODO:
//COMMAND_TEST(delete_c2w, "one two three", "c2w", " three");

// cW
COMMAND_TEST(delete_cW, "one two! three", "llllcWabc", "one abc three");

// paste
COMMAND_TEST(paste_p_at_end_cr, "(one) two three\r\n", "vllllxlllllllllllljp", " two three\n(one)"); // Will replace \r
COMMAND_TEST(paste_p_at_end, "(one) two three", "vllllxllllllllllllp", " two three(one)");
COMMAND_TEST(paste_P_at_end, "(one) two three", "vllllxllllllllllllP", " two thre(one)e");
COMMAND_TEST(paste_P_middle, "(one) two three", "llllllvlylp", "(one) twtwo three");
COMMAND_TEST(paste_p_linewise, "(one)\ntwo\n", "Vyjp", "(one)\ntwo\n(one)\n"); 
COMMAND_TEST(paste_P_linewise, "(one)\ntwo\n", "VyjP", "(one)\n(one)\ntwo\n"); 

COMMAND_TEST(copy_to_register_and_paste, "(one)", "vll\"ryllllll\"rp", "(one)(on");
COMMAND_TEST(copy_to_register_and_append_paste, "(one)", "vll\"rylllv\"Ry\"Rp", "(one(one)");

COMMAND_TEST(copy_to_null_register_and_paste, "(one)", "vll\"_yllllll\"rp", "(one)");

COMMAND_TEST(copy_yy, "(one)", "yyp", "(one)(one)");
COMMAND_TEST(copy_visual_y, "(one)", "vllyp", "((onone)");

// Todo; check syntax highlight result is actually correct during test!
COMMAND_TEST(syntax_highlight, "void main { float4 } /* multicomment */ 32.0 // comment", "xu", "void main { float4 } /* multicomment */ 32.0 // comment");

COMMAND_TEST(dot_command, "one two three four", "daw..", "four");

// Join
COMMAND_TEST(join_lines, "one\ntwo", "J", "onetwo");

// Insert
COMMAND_TEST(insert_a_text, "one three", "lllatwo ", "one two three")
COMMAND_TEST(insert_i_text, "one three", "lllitwo", "onetwo three")
COMMAND_TEST(insert_A_end, "one three", "lllA four", "one three four")
COMMAND_TEST(insert_I, "   one three", "llllllllIzero jk", "   zero one three")

COMMAND_TEST(insert_line_o, "one", "otwojk", "one\ntwo");
COMMAND_TEST(insert_line_O, "one", "Otwojk", "two\none");

COMMAND_TEST(delete_x, "one three", "xxxx", "three")
COMMAND_TEST(delete_dd, "one three", "dd", "")
COMMAND_TEST(delete_D, "one three", "lD", "o")
COMMAND_TEST(undo_redo, "one two three", "vllydur", "one two three")

COMMAND_TEST(delete_d_visual, "one three", "lvld", "o three")

COMMAND_TEST(change_cc, "one two", "cchellojk", "hello")
COMMAND_TEST(change_C, "one two", "llChellojk", "onhello")
COMMAND_TEST(change_d_dollar, "one two", "llc$hellojk", "onhello")

COMMAND_TEST(change_S, "one two", "Shellojk", "hello")
COMMAND_TEST(change_s, "one two", "lsnlyjk", "onlye two")
COMMAND_TEST(change_s_visual, "one two", "vllstwo", "two two")

COMMAND_TEST(unfinished_d, "one two", "d", "one two")
COMMAND_TEST(unfinished_y, "one two", "y", "one two")

COMMAND_TEST_RET(registers, "one\ntwo", "2Y:reg", "one\ntwo")
COMMAND_TEST_RET(bufferset, "one", ":buf 1", "one")
COMMAND_TEST_RET(buffers, "one", ":ls", "one")
COMMAND_TEST_RET(invalid_command, "one", ":invalid", "one")

// Visual
COMMAND_TEST(visual_switch_v, "one", "lvlv", "one");
COMMAND_TEST(visual_switch_V, "one", "lVlV", "one");

#define CURSOR_TEST(name, source, command, xcoord, ycoord) \
TEST_F(VimTest, name)                               \
{                                                   \
    pBuffer->SetText(source);                      \
    spMode->AddCommandText(command);                \
    ASSERT_EQ(pWindow->BufferToDisplay().x, xcoord);     \
    ASSERT_EQ(pWindow->BufferToDisplay().y, ycoord);     \
};

// Motions
CURSOR_TEST(motion_lright, "one", "ll", 2, 0);
CURSOR_TEST(motion_lright_2, "one", "2l", 2, 0);
CURSOR_TEST(motion_lright_limit, "one", "llllll", 2, 0);
CURSOR_TEST(motion_hleft_limit, "one", "h", 0, 0);
CURSOR_TEST(motion_hleft, "one", "llh", 1, 0);
CURSOR_TEST(motion_jdown, "one\ntwo", "j", 0, 1);
CURSOR_TEST(motion_jdown_lright, "one\ntwo", "jl", 1, 1);
CURSOR_TEST(motion_kup, "one\ntwo", "jk", 0, 0);
CURSOR_TEST(motion_kup_lright, "one\ntwo", "jkl", 1, 0);
CURSOR_TEST(motion_kup_limit, "one\ntwo", "kkkkkkkk", 0, 0);
CURSOR_TEST(motion_jdown_limit, "one\ntwo", "jjjjjjjjj", 0, 1);
CURSOR_TEST(motion_jklh_find_center, "one\ntwo\nthree", "jjlk", 1, 1);

CURSOR_TEST(motion_goto_endline, "one two", "$", 6, 0);
CURSOR_TEST(motion_G_goto_enddoc, "one\ntwo", "G", 0, 1);

CURSOR_TEST(motion_3G, "one\ntwo\nthree\nfour\n", "3G", 0, 3);

CURSOR_TEST(motion_goto_begindoc, "one\ntwo", "lljgg", 0, 0);
CURSOR_TEST(motion_goto_beginline, "one two", "lllll0", 0, 0);
CURSOR_TEST(motion_goto_firstlinechar, "   one two", "^", 3, 0);

CURSOR_TEST(motion_2w, "one two three", "2w", 8, 0);
CURSOR_TEST(motion_w, "one! two three", "w", 3, 0);
CURSOR_TEST(motion_w_space, "one two three", "lllw", 4, 0);
CURSOR_TEST(motion_W, "one! two three", "W", 5, 0);
CURSOR_TEST(motion_b, "one! two three", "wwb", 3, 0);
CURSOR_TEST(motion_b_endofword, "one! two three", "llb", 0, 0);
CURSOR_TEST(motion_b_space, "one! two three", "eeelb", 5, 0);
CURSOR_TEST(motion_B, "one! two three", "wwwB", 5, 0);
CURSOR_TEST(motion_B_frombegin, "one! two three", "wwB", 0, 0);
CURSOR_TEST(motion_B_fromspace, "one! two three", "wwjB", 0, 0);

CURSOR_TEST(motion_e, "one! two three", "eee", 7, 0);
CURSOR_TEST(motion_e_jump_space, "one! two three", "elle", 7, 0);
CURSOR_TEST(motion_e_space, "one two three", "llle", 6, 0);
CURSOR_TEST(motion_E, "one! two three", "eelE", 7, 0);
CURSOR_TEST(motion_ge, "one! two three", "wwwge", 7, 0);
CURSOR_TEST(motion_gE, "one two!!!two", "llllllllllllgE", 2, 0);
CURSOR_TEST(motion_ge_startspace, "one! two three", "wwjge", 3, 0);

CURSOR_TEST(motion_0, "one two", "llll0", 0, 0);
CURSOR_TEST(motion_gg, "one two", "llllgg", 0, 0);
CURSOR_TEST(motion_dollar, "one two", "ll$", 6, 0);
