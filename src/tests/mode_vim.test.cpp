#include "config_app.h"

#include "zep/buffer.h"
#include "zep/display.h"
#include "zep/editor.h"
#include "zep/mcommon/logger.h"
#include "zep/mode_vim.h"
#include "zep/tab_window.h"
#include "zep/window.h"

#include <gtest/gtest.h>
#include <regex>

// TESTS
// TODO:
// - A check that navigation up/down on wrapped lines is correct.  Needs to setup a window with a long buffer
// line that wraps, and that navigation moves correctly

using namespace Zep;
class VimTest : public testing::Test
{
public:
    VimTest()
    {
        // Disable threads for consistent tests, at the expense of not catching thread errors!
        // TODO : Fix/understand test failures with threading
        spEditor = std::make_shared<ZepEditor>(new ZepDisplayNull(NVec2f(1.0f, 1.0f)), ZEP_ROOT, ZepEditorFlags::DisableThreads);
        spMode = std::make_shared<ZepMode_Vim>(*spEditor);
        spMode->Init();

        pBuffer = spEditor->InitWithText("Test Buffer", "");

        pTabWindow = spEditor->GetActiveTabWindow();
        pWindow = spEditor->GetActiveTabWindow()->GetActiveWindow();
        spMode->Begin(pWindow);

        // Setup editor with a default fixed_size so that text doesn't wrap and confuse the tests!
        spEditor->SetDisplayRegion(NVec2f(0.0f, 0.0f), NVec2f(1024.0f, 1024.0f));

        pWindow->SetBufferCursor(pBuffer->Begin());

        //Quicker testing
        //COMMAND_TEST(delete_cw, "one two three", "cwabc", "abc two three");
        //COMMAND_TEST(delete_ciw_on_newline, "one\n\nthree", "jciwtwo", "one\ntwo\nthree");
        /*pBuffer->SetText("one\n\nthree");
        spEditor->Display(*spDisplay);
        spMode->AddCommandText("jciwtwo");               
        assert(pBuffer->GetWorkingBuffer().string() == "abc two three");
        */
    }

    ~VimTest()
    {
    }

public:
    std::shared_ptr<ZepEditor> spEditor;
    ZepBuffer* pBuffer;
    ZepWindow* pWindow;
    ZepTabWindow* pTabWindow;
    std::shared_ptr<ZepMode_Vim> spMode;
};

#define HANDLE_VIM_COMMAND(command)               \
    for (auto& ch : command)                      \
    {                                             \
        if (ch == 0)                              \
            continue;                             \
        if (ch == '\n')                           \
        {                                         \
            spMode->AddKeyPress(ExtKeys::RETURN); \
        }                                         \
        else                                      \
        {                                         \
            spMode->AddKeyPress(ch);              \
        }                                         \
    }

#define CURSOR_TEST(name, source, command, xcoord, ycoord) \
    TEST_F(VimTest, name)                                  \
    {                                                      \
        pBuffer->SetText(source);                          \
        HANDLE_VIM_COMMAND(command);                       \
        ASSERT_EQ(pWindow->BufferToDisplay().x, xcoord);   \
        ASSERT_EQ(pWindow->BufferToDisplay().y, ycoord);   \
    };

#define VISUAL_TEST(name, source, command, start, end) \
    TEST_F(VimTest, name)                                  \
    {                                                      \
        pBuffer->SetText(source);                          \
        HANDLE_VIM_COMMAND(command);                       \
        ASSERT_EQ(spMode->GetInclusiveVisualRange().first.Index(), start); \
        ASSERT_EQ(spMode->GetInclusiveVisualRange().second.Index(), end);  \
    };

// Given a sample text, a keystroke list and a target text, check the test returns the right thing
#define COMMAND_TEST(name, source, command, target)                     \
    TEST_F(VimTest, name)                                               \
    {                                                                   \
        pBuffer->SetText(source);                                       \
        spMode->AddCommandText(command);                                \
        ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), target); \
    };

#define COMMAND_TEST_RET(name, source, command, target)                 \
    TEST_F(VimTest, name)                                               \
    {                                                                   \
        pBuffer->SetText(source);                                       \
        spMode->AddCommandText(command);                                \
        spMode->AddKeyPress(ExtKeys::RETURN);                           \
        ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), target); \
    };

TEST_F(VimTest, CheckDisplaySucceeds)
{
    pBuffer->SetText("Some text to display\nThis is a test.");
    spEditor->SetDisplayRegion(NVec2f(0.0f, 0.0f), NVec2f(1024.0f, 1024.0f));
    ASSERT_NO_FATAL_FAILURE(spEditor->Display());
    ASSERT_FALSE(pTabWindow->GetWindows().empty());
}

TEST_F(VimTest, CheckDisplayWrap)
{
    pBuffer->SetText("Some text to display\nThis is a test.");
    ASSERT_NO_FATAL_FAILURE(spEditor->Display());
    ASSERT_FALSE(pTabWindow->GetWindows().empty());
}
TEST_F(VimTest, UndoRedo)
{
    // The issue here is that setting the text _should_ update the buffer!
    pBuffer->SetText("Hello");
    spMode->AddCommandText("3x");
    spMode->Undo();
    spMode->Redo();
    spMode->Undo();
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "Hello");

    spMode->AddCommandText("iYo, jk");
    spMode->Undo();
    spMode->Redo();
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "Yo, Hello");
}

TEST_F(VimTest, DELETE)
{
    pBuffer->SetText("Hello");
    spMode->AddKeyPress(ExtKeys::DEL);
    spMode->AddKeyPress(ExtKeys::DEL);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "llo");

    spMode->AddCommandText("vll");
    spMode->AddKeyPress(ExtKeys::DEL);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "");

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
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "Hello");
}

TEST_F(VimTest, RETURN)
{
    pBuffer->SetText("Õne\ntwo");
    spMode->AddKeyPress(ExtKeys::RETURN);
    ASSERT_EQ(pWindow->BufferToDisplay().y, 1);

    spMode->AddCommandText("li");
    spMode->AddKeyPress(ExtKeys::RETURN);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "Õne\nt\nwo");
}

TEST_F(VimTest, TAB)
{
    pBuffer->SetText("HellÕ");
    spMode->AddCommandText("llllllllli");
    spMode->AddKeyPress(ExtKeys::TAB);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "Hell    Õ");
}

TEST_F(VimTest, BACKSPACE)
{
    pBuffer->SetText("Hello");
    spMode->AddCommandText("ll");
    spMode->AddKeyPress(ExtKeys::BACKSPACE);
    spMode->AddKeyPress(ExtKeys::BACKSPACE);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "Hello");
    ASSERT_EQ(pWindow->GetBufferCursor().Index(), 0);

    spMode->AddCommandText("lli");
    spMode->AddKeyPress(ExtKeys::BACKSPACE);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "Hllo");

    // Check that appending on the line then hitting backspace removes the last char
    // A bug that showed up at some point
    pBuffer->SetText("AB");
    spMode->AddKeyPress(ExtKeys::ESCAPE);
    spMode->AddCommandText("AC");
    spMode->AddKeyPress(ExtKeys::BACKSPACE);
    ASSERT_STREQ(pBuffer->GetWorkingBuffer().string().c_str(), "AB");
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
// CM: This is the whole issue right here.  The display needs to update or the 'j' to move down a line is invalid
// And then the ciw will also change the display;... Shouldn't have to run a display pass to update the visible line buffer.
COMMAND_TEST(delete_ciw_on_newline, "one\n\nthree", "jciwtwo", "one\ntwo\nthree");
COMMAND_TEST(delete_ciw_first, "one two three", "ciwabc", "abc two three");
COMMAND_TEST(delete_ciw_start_space, "one two three", "lllciwabc", "oneabctwo three");
COMMAND_TEST(delete_ciw_inner_spaces, "one    two three", "lllllciwabc", "oneabctwo three");
COMMAND_TEST(delete_ciw_inside_string, "one two three", "lciwabc", "abc two three");

COMMAND_TEST(delete_ciw_single_non_word, "one = two", "llllciw2", "one 2 two");
COMMAND_TEST(delete_ciw_2, "one two three", "2ciwabc", "abctwo three");
COMMAND_TEST(delete_ciw_space_start, " one two three", "ciwabc", "abcone two three");

// ciW
COMMAND_TEST(delete_ciW_first, "one! two three", "ciWabc", "abc two three");
COMMAND_TEST(delete_ciw_nonascii, "one£ two three", "ciwabc", "abc£ two three");
COMMAND_TEST(delete_ciW_nonascii, "one£ two three", "ciWabc", "abc£ two three");

// cw
COMMAND_TEST(delete_cw, "one two three", "cwabc", "abc two three");
COMMAND_TEST(delete_cw_inside, "one two three", "lcwabc", "oabc two three");
COMMAND_TEST(delete_cw_inside_2, "one two three", "llllllllcwabc", "one two abc");

// TODO: Inner count doesn't work, counts on changing words doesn't work.
// I think this is because the first command runs and the second one tries to modify the following space instead of the next word.
// Need to debug..
//COMMAND_TEST(delete_c2w, "one two three", "c2w", " three");

// cW
COMMAND_TEST(delete_cW, "one two! three", "llllcWabc", "one abc three");

COMMAND_TEST(replace_char, "one", "lrx", "oxe");

COMMAND_TEST(replace_char_digit, "one", "lr2", "o2e");

COMMAND_TEST(replace_char_undo, "one", "lrxu", "one");

COMMAND_TEST(replace_range_with_char, "one", "lvlrx", "oxx");

COMMAND_TEST(replace_range_with_char_undo, "one", "lvlrxu", "one");

COMMAND_TEST(replace_with_char_and_count, "one", "l2rx", "oxx");

COMMAND_TEST(replace_char_end, "one", "ll3rx", "one"); // Should ignore due to lack of chars in buffer

// paste
COMMAND_TEST(paste_p_at_end_cr, "(one) two three\r\n", "vllllxlllllllllllljp", " two three\n(one)"); // Will replace \r
COMMAND_TEST(paste_p_at_end, "(one) two three", "vllllxllllllllllllp", " two three(one)");
COMMAND_TEST(paste_p_middle, "(one) two three", "llllllvlylp", "(one) twtwo three");
COMMAND_TEST(paste_P_at_end, "(one) two three", "vllllxllllllllllllP", " two thre(one)e");
COMMAND_TEST(paste_p_linewise, "(one)\ntwo\n", "Vyjp", "(one)\ntwo\n(one)\n");
COMMAND_TEST(paste_P_linewise, "(one)\ntwo\n", "VyjP", "(one)\n(one)\ntwo\n");

COMMAND_TEST(copy_to_register_and_paste, "(one)", "vll\"ryllllll\"rp", "(one)(on");
COMMAND_TEST(copy_to_register_and_append_paste, "(one)", "vll\"rylllv\"Ry\"Rp", "(one(one)");

COMMAND_TEST(copy_to_null_register_and_paste, "(one)", "vll\"_yllllll\"rp", "(one)");

COMMAND_TEST(copy_yy, "(one)", "yyp", "(one)(one)");
COMMAND_TEST(copy_yy_paste_line, "one\ntwo", "yyp", "one\none\ntwo");
COMMAND_TEST(copy_visual_y, "(one)", "vllyp", "((onone)");

// Todo; check syntax highlight result is actually correct during test!
COMMAND_TEST(syntax_highlight, "void main { float4 } /* multicomment */ 32.0 // comment", "xu", "void main { float4 } /* multicomment */ 32.0 // comment");

COMMAND_TEST(dot_command, "one two three four", "daw..", "four");

// Join - leaves a space, skips white space
COMMAND_TEST(join_lines, "one\ntwo", "J", "one two");
COMMAND_TEST(join_lines_on_empty, "one\n\ntwo", "jJ", "one\ntwo");

COMMAND_TEST(join_lines_skip_ws, "one\n   two", "J", "one two");

COMMAND_TEST(join_visual, "one\ntwo", "vlJ", "one two");

COMMAND_TEST(join_to_end, "one", "J", "one");

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

COMMAND_TEST(delete_to_eol, "hello\nworld", "lll10x", "hel\nworld");
COMMAND_TEST(delete_x_paste, "hello", "lxp", "hlelo");
COMMAND_TEST(delete_3x, "hello", "3x", "lo");
COMMAND_TEST(delete_3x_paste, "hello", "l3xp", "hoell");

COMMAND_TEST(delete_d_visual, "one three", "lvld", "o three")

COMMAND_TEST(change_cc, "one two", "cchellojk", "hello")
COMMAND_TEST(change_C, "one two", "llChellojk", "onhello")
COMMAND_TEST(change_d_dollar, "one two", "llc$hellojk", "onhello")

COMMAND_TEST(Substitute_S, "one two", "Shellojk", "hello")
COMMAND_TEST(Substitute_s, "one two", "lsnlyjk", "onlye two")
COMMAND_TEST(Substitute_s_visual, "one two", "vllstwo", "two two")

COMMAND_TEST(unfinished_d, "one two", "d", "one two")
COMMAND_TEST(unfinished_y, "one two", "y", "one two")

COMMAND_TEST_RET(registers, "one\ntwo", "2Y:reg", "one\ntwo")
COMMAND_TEST_RET(bufferset, "one", ":buf 1", "one")
COMMAND_TEST_RET(buffers, "one", ":ls", "one")
COMMAND_TEST_RET(invalid_command, "one", ":invalid", "one")

// Visual
COMMAND_TEST(visual_switch_v, "one", "lvlv", "one");
COMMAND_TEST(visual_switch_V, "one", "lVlV", "one");

COMMAND_TEST(chage_to, "one two", "ctthey", "heytwo");

COMMAND_TEST(change_to_digit, "one 1wo", "ct1hey", "hey1wo");

COMMAND_TEST(delete_to_char, "one 1wo", "dt1", "1wo");
COMMAND_TEST(delete_to_digit, "one two", "dtt", "two");
COMMAND_TEST(delete_final_line, "one\n", "jdd", "one");

COMMAND_TEST(visual_inner_word, "one-three", "lviwd", "-three");
COMMAND_TEST(visual_inner_word_undo, "one-three", "lviwdu", "one-three");
COMMAND_TEST(visual_inner_WORD, "one-three ", "lviWd", " ");
COMMAND_TEST(visual_inner_WORD_undo, "one-three ", "lviWdu", "one-three ");
COMMAND_TEST(visual_a_word, "one three", "vawd", "three");
COMMAND_TEST(visual_a_word_undo, "one three", "vawdu", "one three");
COMMAND_TEST(visual_a_WORD, "one-three four", "vaWd", "four");
COMMAND_TEST(visual_a_WORD_undo, "one-three four", "vaWdu", "one-three four");
COMMAND_TEST(visual_copy_paste_over, "hello goodbye", "vllllyllllllvlllllllp", "hello hello");

VISUAL_TEST(visual_select_from_end, "one\n", "jvk", 0, 4);

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
CURSOR_TEST(motion_find_jumpto, "one two", "/two\n", 4, 0);
CURSOR_TEST(motion_G_goto_enddoc, "one\ntwo", "G", 0, 1);
CURSOR_TEST(motion_3G, "one\ntwo\nthree\nfour\n", "3G", 0, 2); // Note: Goto line3, offset 2!
CURSOR_TEST(motion_0G, "one\ntwo\nthree\nfour\n", "0G", 0, 4); // Note: 0 means go to last line
CURSOR_TEST(motion_goto_begindoc, "one\ntwo", "lljgg", 0, 0);
CURSOR_TEST(motion_goto_firstlinechar, "   one two", "^", 3, 0);
CURSOR_TEST(motion_2w, "one two three", "2w", 8, 0);
CURSOR_TEST(motion_w, "one! two three", "w", 3, 0);
CURSOR_TEST(motion_w_space, "one two three", "lllw", 4, 0);
CURSOR_TEST(motion_W, "one! two three", "W", 5, 0);
CURSOR_TEST(motion_W_over_line, "one;\ntwo", "W", 0, 1);

CURSOR_TEST(motion_w_over_nonascii, "abc£ def", "w", 3, 0);
CURSOR_TEST(motion_W_over_nonascii, "abc£ def", "W", 3, 0);

CURSOR_TEST(motion_b, "one! two three", "wwb", 3, 0);
CURSOR_TEST(motion_b_from_non_word, "one! two three", "wwbb", 0, 0);
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
CURSOR_TEST(motion_cr_then_escape, "one", "$a\njk", 0, 1);
CURSOR_TEST(cursor_copy_yy_paste_line, "one\ntwo", "yyp", 0, 1);

CURSOR_TEST(find_a_char, "one two", "ft", 4, 0);
CURSOR_TEST(find_a_char_fail, "one two", "fz", 0, 0);
CURSOR_TEST(find_a_char_stay_on_line, "one two\nthree", "fefe", 2, 0);
CURSOR_TEST(find_a_char_repeat, "one one one", "fo;", 8, 0);
CURSOR_TEST(find_a_char_num, "one2 one2", "2f2", 8, 0);
CURSOR_TEST(find_a_char_beside, "ooo", "fo;", 2, 0);
CURSOR_TEST(find_backwards, "foo", "lllllFf", 0, 0);

inline std::string MakeCommandRegex(const std::string& command)
{
    return std::string(R"((?:(\d)|(<\S>*)|("\w?)*)()") + command + ")";
}

TEST(Regex, VimRegex)
{
    auto rx = MakeCommandRegex("y");
    std::regex re(rx);

    std::smatch match;
    std::string testString("3y");
    if (std::regex_match(testString, match, re))
    {
        for (auto& m : match)
        {
            ZEP_UNUSED(m);
            ZLOG(DBG, "Match: " << m.str());
        }
    }
}
