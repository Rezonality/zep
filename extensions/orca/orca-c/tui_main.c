#include "base.h"
#include "field.h"
#include "gbuffer.h"
#include "osc_out.h"
#include "oso.h"
#include "sim.h"
#include "sysmisc.h"
#include "term_util.h"
#include "vmio.h"
#include <getopt.h>
#include <locale.h>

#define SOKOL_IMPL
#include "sokol_time.h"
#undef SOKOL_IMPL

#ifdef FEAT_PORTMIDI
#include <portmidi.h>
#endif

#if NCURSES_VERSION_PATCH < 20081122
int _nc_has_mouse(void);
#define has_mouse _nc_has_mouse
#endif

#define TIME_DEBUG 0
#if TIME_DEBUG
static int spin_track_timeout = 0;
#endif

#define staticni ORCA_NOINLINE static

staticni void usage(void) { // clang-format off
fprintf(stderr,
"Usage: orca [options] [file]\n\n"
"General options:\n"
"    --undo-limit <number>  Set the maximum number of undo steps.\n"
"                           If you plan to work with large files,\n"
"                           set this to a low number.\n"
"                           Default: 100\n"
"    --initial-size <nxn>   When creating a new grid file, use these\n"
"                           starting dimensions.\n"
"    --bpm <number>         Set the tempo (beats per minute).\n"
"                           Default: 120\n"
"    --seed <number>        Set the seed for the random function.\n"
"                           Default: 1\n"
"    -h or --help           Print this message and exit.\n"
"\n"
"OSC/MIDI options:\n"
"    --strict-timing\n"
"        Attempt to reduce timing jitter of outgoing MIDI and OSC\n"
"        messages. Uses more CPU time. May have no effect.\n"
"\n"
"    --osc-midi-bidule <path>\n"
"        Set MIDI to be sent via OSC formatted for Plogue Bidule.\n"
"        The path argument is the path of the Plogue OSC MIDI device.\n"
"        Example: /OSC_MIDI_0/MIDI\n"
);} // clang-format on

typedef enum {
  Glyph_class_unknown,
  Glyph_class_grid,
  Glyph_class_comment,
  Glyph_class_uppercase,
  Glyph_class_lowercase,
  Glyph_class_movement,
  Glyph_class_numeric,
  Glyph_class_bang,
} Glyph_class;

static Glyph_class glyph_class_of(Glyph glyph) {
  if (glyph == '.')
    return Glyph_class_grid;
  if (glyph >= '0' && glyph <= '9')
    return Glyph_class_numeric;
  switch (glyph) {
  case 'N':
  case 'n':
  case 'E':
  case 'e':
  case 'S':
  case 's':
  case 'W':
  case 'w':
  case 'Z':
  case 'z':
    return Glyph_class_movement;
  case '!':
  case ':':
  case ';':
  case '=':
  case '%':
  case '?':
    return Glyph_class_lowercase;
  case '*':
    return Glyph_class_bang;
  case '#':
    return Glyph_class_comment;
  }
  if (glyph >= 'A' && glyph <= 'Z')
    return Glyph_class_uppercase;
  if (glyph >= 'a' && glyph <= 'z')
    return Glyph_class_lowercase;
  return Glyph_class_unknown;
}

static attr_t term_attrs_of_cell(Glyph g, Mark m) {
  Glyph_class gclass = glyph_class_of(g);
  attr_t attr = A_normal;
  switch (gclass) {
  case Glyph_class_unknown:
    attr = A_bold | fg_bg(C_red, C_natural);
    break;
  case Glyph_class_grid:
    attr = A_bold | fg_bg(C_black, C_natural);
    break;
  case Glyph_class_comment:
    attr = A_dim | Cdef_normal;
    break;
  case Glyph_class_uppercase:
    attr = A_normal | fg_bg(C_black, C_cyan);
    break;
  case Glyph_class_lowercase:
  case Glyph_class_movement:
  case Glyph_class_numeric:
    attr = A_bold | Cdef_normal;
    break;
  case Glyph_class_bang:
    attr = A_bold | Cdef_normal;
    break;
  }
  if (gclass != Glyph_class_comment) {
    if ((m & (Mark_flag_lock | Mark_flag_input)) ==
        (Mark_flag_lock | Mark_flag_input)) {
      // Standard locking input
      attr = A_normal | Cdef_normal;
    } else if ((m & Mark_flag_input) == Mark_flag_input) {
      // Non-locking input
      attr = A_normal | Cdef_normal;
    } else if (m & Mark_flag_lock) {
      // Locked only
      attr = A_dim | Cdef_normal;
    }
  }
  if (m & Mark_flag_output) {
    attr = A_reverse;
  }
  if (m & Mark_flag_haste_input) {
    attr = A_bold | fg_bg(C_cyan, C_natural);
  }
  return attr;
}

typedef enum {
  Ged_input_mode_normal = 0,
  Ged_input_mode_append,
  Ged_input_mode_selresize,
  Ged_input_mode_slide,
} Ged_input_mode;

typedef struct {
  Usz y, x, h, w;
} Ged_cursor;

void ged_cursor_init(Ged_cursor *tc) {
  tc->x = tc->y = 0;
  tc->w = tc->h = 1;
}

static void ged_cursor_move_relative(Ged_cursor *tc, Usz field_h, Usz field_w,
                                     Isz delta_y, Isz delta_x) {
  Isz y0 = (Isz)tc->y + delta_y;
  Isz x0 = (Isz)tc->x + delta_x;
  if (y0 >= (Isz)field_h)
    y0 = (Isz)field_h - 1;
  if (y0 < 0)
    y0 = 0;
  if (x0 >= (Isz)field_w)
    x0 = (Isz)field_w - 1;
  if (x0 < 0)
    x0 = 0;
  tc->y = (Usz)y0;
  tc->x = (Usz)x0;
}

staticni void draw_grid_cursor(WINDOW *win, int draw_y, int draw_x, int draw_h,
                               int draw_w, Glyph const *gbuffer, Usz field_h,
                               Usz field_w, int scroll_y, int scroll_x,
                               Usz cursor_y, Usz cursor_x, Usz cursor_h,
                               Usz cursor_w, Ged_input_mode input_mode,
                               bool is_playing) {
  (void)input_mode;
  if (cursor_y >= field_h || cursor_x >= field_w)
    return;
  if (scroll_y < 0) {
    draw_y += -scroll_y;
    scroll_y = 0;
  }
  if (scroll_x < 0) {
    draw_x += -scroll_x;
    scroll_x = 0;
  }
  Usz offset_y = (Usz)scroll_y;
  Usz offset_x = (Usz)scroll_x;
  if (offset_y >= field_h || offset_x >= field_w)
    return;
  if (draw_y >= draw_h || draw_x >= draw_w)
    return;
  attr_t const curs_attr = A_reverse | A_bold | fg_bg(C_yellow, C_natural);
  if (offset_y <= cursor_y && offset_x <= cursor_x) {
    Usz cdraw_y = cursor_y - offset_y + (Usz)draw_y;
    Usz cdraw_x = cursor_x - offset_x + (Usz)draw_x;
    if (cdraw_y < (Usz)draw_h && cdraw_x < (Usz)draw_w) {
      Glyph beneath = gbuffer[cursor_y * field_w + cursor_x];
      char displayed;
      if (beneath == '.') {
        displayed = is_playing ? '@' : '~';
      } else {
        displayed = beneath;
      }
      chtype ch = (chtype)displayed | curs_attr;
      wmove(win, (int)cdraw_y, (int)cdraw_x);
      waddchnstr(win, &ch, 1);
    }
  }

  // Early out for selection area that won't have any visual effect
  if (cursor_h <= 1 && cursor_w <= 1)
    return;

  // Now mutate visually selected area under grid to have the selection color
  // attributes. (This will rewrite the attributes on the cursor character we
  // wrote above, but if it was the only character that would have been
  // changed, we already early-outed.)
  //
  // We'll do this by reading back the characters on the grid from the curses
  // window buffer, changing the attributes, then writing it back. This is
  // easier than pulling the glyphs from the gbuffer, since we already did the
  // ruler calculations to turn . into +, and we don't need special behavior
  // for any other attributes (e.g. we don't show a special state for selected
  // uppercase characters.)
  //
  // First, confine cursor selection to the grid field/gbuffer that actually
  // exists, in case the cursor selection exceeds the area of the field.
  Usz sel_rows = field_h - cursor_y;
  if (cursor_h < sel_rows)
    sel_rows = cursor_h;
  Usz sel_cols = field_w - cursor_x;
  if (cursor_w < sel_cols)
    sel_cols = cursor_w;
  // Now, confine the selection area to what's visible on screen. Kind of
  // tricky since we have to handle it being partially visible from any edge on
  // any axis, and we have to be mindful overflow.
  Usz vis_sel_y;
  Usz vis_sel_x;
  if (offset_y > cursor_y) {
    vis_sel_y = 0;
    Usz sub_y = offset_y - cursor_y;
    if (sub_y > sel_rows)
      sel_rows = 0;
    else
      sel_rows -= sub_y;
  } else {
    vis_sel_y = cursor_y - offset_y;
  }
  if (offset_x > cursor_x) {
    vis_sel_x = 0;
    Usz sub_x = offset_x - cursor_x;
    if (sub_x > sel_cols)
      sel_cols = 0;
    else
      sel_cols -= sub_x;
  } else {
    vis_sel_x = cursor_x - offset_x;
  }
  vis_sel_y += (Usz)draw_y;
  vis_sel_x += (Usz)draw_x;
  if (vis_sel_y >= (Usz)draw_h || vis_sel_x >= (Usz)draw_w)
    return;
  Usz vis_sel_h = (Usz)draw_h - vis_sel_y;
  Usz vis_sel_w = (Usz)draw_w - vis_sel_x;
  if (sel_rows < vis_sel_h)
    vis_sel_h = sel_rows;
  if (sel_cols < vis_sel_w)
    vis_sel_w = sel_cols;
  if (vis_sel_w == 0 || vis_sel_h == 0)
    return;
  enum { Bufcount = 4096 };
  chtype chbuffer[Bufcount];
  if (Bufcount < vis_sel_w)
    vis_sel_w = Bufcount;
  for (Usz iy = 0; iy < vis_sel_h; ++iy) {
    int at_y = (int)(vis_sel_y + iy);
    int num = mvwinchnstr(win, at_y, (int)vis_sel_x, chbuffer, (int)vis_sel_w);
    for (int ix = 0; ix < num; ++ix) {
      chbuffer[ix] = (chtype)((chbuffer[ix] & (A_CHARTEXT | A_ALTCHARSET)) |
                              (chtype)curs_attr);
    }
    waddchnstr(win, chbuffer, (int)num);
  }
}

typedef struct Undo_node {
  Field field;
  Usz tick_num;
  struct Undo_node *prev, *next;
} Undo_node;

typedef struct {
  Undo_node *first, *last;
  Usz count, limit;
} Undo_history;

static void undo_history_init(Undo_history *hist, Usz limit) {
  *hist = (Undo_history){0};
  hist->limit = limit;
}
static void undo_history_deinit(Undo_history *hist) {
  Undo_node *a = hist->first;
  while (a) {
    Undo_node *b = a->next;
    field_deinit(&a->field);
    free(a);
    a = b;
  }
}

staticni bool undo_history_push(Undo_history *hist, Field *field,
                                Usz tick_num) {
  if (hist->limit == 0)
    return false;
  Undo_node *new_node;
  if (hist->count == hist->limit) {
    new_node = hist->first;
    if (new_node == hist->last) {
      hist->first = NULL;
      hist->last = NULL;
    } else {
      hist->first = new_node->next;
      hist->first->prev = NULL;
    }
  } else {
    new_node = malloc(sizeof(Undo_node));
    if (!new_node)
      return false;
    ++hist->count;
    field_init(&new_node->field);
  }
  field_copy(field, &new_node->field);
  new_node->tick_num = tick_num;
  if (hist->last) {
    hist->last->next = new_node;
    new_node->prev = hist->last;
  } else {
    hist->first = new_node;
    hist->last = new_node;
    new_node->prev = NULL;
  }
  new_node->next = NULL;
  hist->last = new_node;
  return true;
}

staticni void undo_history_pop(Undo_history *hist, Field *out_field,
                               Usz *out_tick_num) {
  Undo_node *last = hist->last;
  if (!last)
    return;
  field_copy(&last->field, out_field);
  *out_tick_num = last->tick_num;
  if (hist->first == last) {
    hist->first = NULL;
    hist->last = NULL;
  } else {
    Undo_node *new_last = last->prev;
    new_last->next = NULL;
    hist->last = new_last;
  }
  field_deinit(&last->field);
  free(last);
  --hist->count;
}

staticni void undo_history_apply(Undo_history *hist, Field *out_field,
                                 Usz *out_tick_num) {
  Undo_node *last = hist->last;
  if (!last)
    return;
  field_copy(&last->field, out_field);
  *out_tick_num = last->tick_num;
}

static Usz undo_history_count(Undo_history *hist) { return hist->count; }

staticni void print_activity_indicator(WINDOW *win, Usz activity_counter) {
  // 7 segments that can each light up as Colors different colors.
  // This gives us Colors^Segments total configurations.
  enum { Segments = 7, Colors = 4 };
  Usz states = 1; // calculate Colors^Segments
  for (Usz i = 0; i < Segments; ++i)
    states *= Colors;
  // Wrap the counter to the range of displayable configurations.
  Usz val = activity_counter % states;
  chtype lamps[Colors];
#if 1 // Appearance where segments are always lit
  lamps[0] = ACS_HLINE | fg_bg(C_black, C_natural) | A_bold;
  lamps[1] = ACS_HLINE | fg_bg(C_white, C_natural) | A_normal;
  lamps[2] = ACS_HLINE | A_bold;
  lamps[3] = lamps[1];
#elif 0 // Brighter appearance where segments are always lit
  lamps[0] = ACS_HLINE | fg_bg(C_black, C_natural) | A_bold;
  lamps[1] = ACS_HLINE | A_normal;
  lamps[2] = ACS_HLINE | A_bold;
  lamps[3] = lamps[1];
#else   // Appearance where segments can turn off completely
  lamps[0] = ' ';
  lamps[1] = ACS_HLINE | fg_bg(C_black, C_natural) | A_bold;
  lamps[2] = ACS_HLINE | A_normal;
  lamps[3] = lamps[1];
#endif
  chtype buffer[Segments];
  for (Usz i = 0; i < Segments; ++i) {
    // Instead of a left-to-right, straightforward ascending least-to-most
    // significant digits display, we'll display it as a spiral.
    Usz j = i % 2 ? (6 - i / 2) : (i / 2);
    buffer[j] = lamps[val % Colors];
    val = val / Colors;
  }
  waddchnstr(win, buffer, Segments);
  // If you want to see what various combinations of colors and attributes look
  // like in different terminals.
#if 0
  waddch(win, 'a' | fg_bg(C_black, C_natural) | A_dim);
  waddch(win, 'b' | fg_bg(C_black, C_natural) | A_normal);
  waddch(win, 'c' | fg_bg(C_black, C_natural) | A_bold);
  waddch(win, 'd' | A_dim);
  waddch(win, 'e' | A_normal);
  waddch(win, 'f' | A_bold);
  waddch(win, 'g' | fg_bg(C_white, C_natural) | A_dim);
  waddch(win, 'h' | fg_bg(C_white, C_natural) | A_normal);
  waddch(win, 'i' | fg_bg(C_white, C_natural) | A_bold);
#endif
}

staticni void advance_faketab(WINDOW *win, int offset_x, int tabstop) {
  if (tabstop < 1)
    return;
  int y, x, h, w;
  getyx(win, y, x);
  getmaxyx(win, h, w);
  (void)h;
  x = ((x + tabstop - 1) / tabstop) * tabstop + offset_x % tabstop;
  if (w < 1)
    w = 1;
  if (x >= w)
    x = w - 1;
  wmove(win, y, x);
}

staticni void draw_hud(WINDOW *win, int win_y, int win_x, int height, int width,
                       char const *filename, Usz field_h, Usz field_w,
                       Usz ruler_spacing_y, Usz ruler_spacing_x, Usz tick_num,
                       Usz bpm, Ged_cursor const *ged_cursor,
                       Ged_input_mode input_mode, Usz activity_counter) {
  (void)height;
  (void)width;
  enum { Tabstop = 8 };
  wmove(win, win_y, win_x);
  wprintw(win, "%zux%zu", field_w, field_h);
  advance_faketab(win, win_x, Tabstop);
  wprintw(win, "%zu/%zu", ruler_spacing_x, ruler_spacing_y);
  advance_faketab(win, win_x, Tabstop);
  wprintw(win, "%zuf", tick_num);
  advance_faketab(win, win_x, Tabstop);
  wprintw(win, "%zu", bpm);
  advance_faketab(win, win_x, Tabstop);
  print_activity_indicator(win, activity_counter);
  wmove(win, win_y + 1, win_x);
  wprintw(win, "%zu,%zu", ged_cursor->x, ged_cursor->y);
  advance_faketab(win, win_x, Tabstop);
  wprintw(win, "%zu:%zu", ged_cursor->w, ged_cursor->h);
  advance_faketab(win, win_x, Tabstop);
  switch (input_mode) {
  case Ged_input_mode_normal:
    wattrset(win, A_normal);
    waddstr(win, "insert");
    break;
  case Ged_input_mode_append:
    wattrset(win, A_bold);
    waddstr(win, "append");
    break;
  case Ged_input_mode_selresize:
    wattrset(win, A_bold);
    waddstr(win, "select");
    break;
  case Ged_input_mode_slide:
    wattrset(win, A_reverse);
    waddstr(win, "slide");
    break;
  }
  advance_faketab(win, win_x, Tabstop);
  wattrset(win, A_normal);
  waddstr(win, filename);
}

staticni void draw_glyphs_grid(WINDOW *win, int draw_y, int draw_x, int draw_h,
                               int draw_w, Glyph const *restrict gbuffer,
                               Mark const *restrict mbuffer, Usz field_h,
                               Usz field_w, Usz offset_y, Usz offset_x,
                               Usz ruler_spacing_y, Usz ruler_spacing_x,
                               bool use_fancy_dots, bool use_fancy_rulers) {
  assert(draw_y >= 0 && draw_x >= 0);
  assert(draw_h >= 0 && draw_w >= 0);
  enum { Bufcount = 4096 };
  chtype chbuffer[Bufcount];
  // todo buffer limit
  if (offset_y >= field_h || offset_x >= field_w)
    return;
  if (draw_y >= draw_h || draw_x >= draw_w)
    return;
  Usz rows = (Usz)(draw_h - draw_y);
  if (field_h - offset_y < rows)
    rows = field_h - offset_y;
  Usz cols = (Usz)(draw_w - draw_x);
  if (field_w - offset_x < cols)
    cols = field_w - offset_x;
  if (Bufcount < cols)
    cols = Bufcount;
  if (rows == 0 || cols == 0)
    return;
  bool use_rulers = ruler_spacing_y != 0 && ruler_spacing_x != 0;
  chtype bullet = use_fancy_dots ? ACS_BULLET : '.';
  enum { T = 1 << 0, B = 1 << 1, L = 1 << 2, R = 1 << 3 };
  chtype rs[(T | B | L | R) + 1];
  if (use_rulers) {
    for (Usz i = 0; i < sizeof rs / sizeof(chtype); ++i)
      rs[i] = '+';
    if (use_fancy_rulers) {
      rs[T | L] = ACS_ULCORNER;
      rs[T | R] = ACS_URCORNER;
      rs[B | L] = ACS_LLCORNER;
      rs[B | R] = ACS_LRCORNER;
      rs[T] = ACS_TTEE;
      rs[B] = ACS_BTEE;
      rs[L] = ACS_LTEE;
      rs[R] = ACS_RTEE;
    }
  }
  for (Usz iy = 0; iy < rows; ++iy) {
    Usz line_offset = (offset_y + iy) * field_w + offset_x;
    Glyph const *g_row = gbuffer + line_offset;
    Mark const *m_row = mbuffer + line_offset;
    bool use_y_ruler = use_rulers && (iy + offset_y) % ruler_spacing_y == 0;
    for (Usz ix = 0; ix < cols; ++ix) {
      Glyph g = g_row[ix];
      Mark m = m_row[ix];
      chtype ch;
      if (g == '.') {
        if (use_y_ruler && (ix + offset_x) % ruler_spacing_x == 0) {
          int p = 0; // clang-format off
          if (iy + offset_y     == 0      ) p |= T;
          if (iy + offset_y + 1 == field_h) p |= B;
          if (ix + offset_x     == 0      ) p |= L;
          if (ix + offset_x + 1 == field_w) p |= R;
          ch = rs[p]; // clang-format on
        } else {
          ch = bullet;
        }
      } else {
        ch = (chtype)g;
      }
      attr_t attrs = term_attrs_of_cell(g, m);
      chbuffer[ix] = ch | attrs;
    }
    wmove(win, draw_y + (int)iy, draw_x);
    waddchnstr(win, chbuffer, (int)cols);
  }
}

staticni void draw_glyphs_grid_scrolled(
    WINDOW *win, int draw_y, int draw_x, int draw_h, int draw_w,
    Glyph const *restrict gbuffer, Mark const *restrict mbuffer, Usz field_h,
    Usz field_w, int scroll_y, int scroll_x, Usz ruler_spacing_y,
    Usz ruler_spacing_x, bool use_fancy_dots, bool use_fancy_rulers) {
  if (scroll_y < 0) {
    draw_y += -scroll_y;
    scroll_y = 0;
  }
  if (scroll_x < 0) {
    draw_x += -scroll_x;
    scroll_x = 0;
  }
  draw_glyphs_grid(win, draw_y, draw_x, draw_h, draw_w, gbuffer, mbuffer,
                   field_h, field_w, (Usz)scroll_y, (Usz)scroll_x,
                   ruler_spacing_y, ruler_spacing_x, use_fancy_dots,
                   use_fancy_rulers);
}

static void ged_cursor_confine(Ged_cursor *tc, Usz height, Usz width) {
  if (height == 0 || width == 0)
    return;
  if (tc->y >= height)
    tc->y = height - 1;
  if (tc->x >= width)
    tc->x = width - 1;
}

staticni void draw_oevent_list(WINDOW *win, Oevent_list const *oevent_list) {
  wmove(win, 0, 0);
  int win_h = getmaxy(win);
  wprintw(win, "Count: %d", (int)oevent_list->count);
  for (Usz i = 0, num_events = oevent_list->count; i < num_events; ++i) {
    int cury = getcury(win);
    if (cury + 1 >= win_h)
      return;
    wmove(win, cury + 1, 0);
    Oevent const *ev = oevent_list->buffer + i;
    Oevent_types evt = ev->any.oevent_type;
    switch (evt) {
    case Oevent_type_midi_note: {
      Oevent_midi_note const *em = &ev->midi_note;
      wprintw(
          win,
          "MIDI Note\tchannel %d\toctave %d\tnote %d\tvelocity %d\tlength %d",
          (int)em->channel, (int)em->octave, (int)em->note, (int)em->velocity,
          (int)em->duration);
      break;
    }
    case Oevent_type_midi_cc: {
      Oevent_midi_cc const *ec = &ev->midi_cc;
      wprintw(win, "MIDI CC\tchannel %d\tcontrol %d\tvalue %d",
              (int)ec->channel, (int)ec->control, (int)ec->value);
      break;
    }
    case Oevent_type_midi_pb: {
      Oevent_midi_pb const *ep = &ev->midi_pb;
      wprintw(win, "MIDI PB\tchannel %d\tmsb %d\tlsb %d", (int)ep->channel,
              (int)ep->msb, (int)ep->lsb);
      break;
    }
    case Oevent_type_osc_ints: {
      Oevent_osc_ints const *eo = &ev->osc_ints;
      wprintw(win, "OSC\t%c\tcount: %d ", eo->glyph, eo->count, eo->count);
      waddch(win, ACS_VLINE);
      for (Usz j = 0; j < eo->count; ++j) {
        wprintw(win, " %d", eo->numbers[j]);
      }
      break;
    }
    case Oevent_type_udp_string: {
      Oevent_udp_string const *eo = &ev->udp_string;
      wprintw(win, "UDP\tcount %d\t", (int)eo->count);
      for (Usz j = 0; j < (Usz)eo->count; ++j) {
        waddch(win, (chtype)eo->chars[j]);
      }
      break;
    }
    }
  }
}

staticni void ged_resize_grid(Field *field, Mbuf_reusable *mbr, Usz new_height,
                              Usz new_width, Usz tick_num, Field *scratch_field,
                              Undo_history *undo_hist, Ged_cursor *ged_cursor) {
  assert(new_height > 0 && new_width > 0);
  undo_history_push(undo_hist, field, tick_num);
  field_copy(field, scratch_field);
  field_resize_raw(field, new_height, new_width);
  // junky copies until i write a smarter thing
  memset(field->buffer, '.', new_height * new_width * sizeof(Glyph));
  gbuffer_copy_subrect(scratch_field->buffer, field->buffer,
                       scratch_field->height, scratch_field->width,
                       field->height, field->width, 0, 0, 0, 0,
                       scratch_field->height, scratch_field->width);
  ged_cursor_confine(ged_cursor, new_height, new_width);
  mbuf_reusable_ensure_size(mbr, new_height, new_width);
}

staticni Usz adjust_rulers_humanized(Usz ruler, Usz in, Isz delta_rulers) {
  // slightly more confusing because desired grid sizes are +1 (e.g. ruler of
  // length 8 wants to snap to 25 and 33, not 24 and 32). also this math is
  // sloppy.
  assert(ruler > 0);
  if (in == 0)
    return delta_rulers > 0 ? ruler * (Usz)delta_rulers : 1;
  // could overflow if inputs are big
  if (delta_rulers < 0)
    in += ruler - 1;
  Isz n = ((Isz)in - 1) / (Isz)ruler + delta_rulers;
  if (n < 0)
    n = 0;
  return ruler * (Usz)n + 1;
}

// Resizes by number of ruler divisions, and snaps size to closest division in
// a way a human would expect. Adds +1 to the output, so grid resulting size is
// 1 unit longer than the actual ruler length.
staticni bool ged_resize_grid_snap_ruler(Field *field, Mbuf_reusable *mbr,
                                         Usz ruler_y, Usz ruler_x, Isz delta_h,
                                         Isz delta_w, Usz tick_num,
                                         Field *scratch_field,
                                         Undo_history *undo_hist,
                                         Ged_cursor *ged_cursor) {
  assert(ruler_y > 0);
  assert(ruler_x > 0);
  Usz field_h = field->height;
  Usz field_w = field->width;
  assert(field_h > 0);
  assert(field_w > 0);
  if (ruler_y == 0 || ruler_x == 0 || field_h == 0 || field_w == 0)
    return false;
  Usz new_field_h = field_h;
  Usz new_field_w = field_w;
  if (delta_h != 0)
    new_field_h = adjust_rulers_humanized(ruler_y, field_h, delta_h);
  if (delta_w != 0)
    new_field_w = adjust_rulers_humanized(ruler_x, field_w, delta_w);
  if (new_field_h > ORCA_Y_MAX)
    new_field_h = ORCA_Y_MAX;
  if (new_field_w > ORCA_X_MAX)
    new_field_w = ORCA_X_MAX;
  if (new_field_h == field_h && new_field_w == field_w)
    return false;
  ged_resize_grid(field, mbr, new_field_h, new_field_w, tick_num, scratch_field,
                  undo_hist, ged_cursor);
  return true;
}

typedef enum {
  Midi_mode_type_null,
  Midi_mode_type_osc_bidule,
#ifdef FEAT_PORTMIDI
  Midi_mode_type_portmidi,
#endif
} Midi_mode_type;

typedef struct {
  Midi_mode_type type;
} Midi_mode_any;

typedef struct {
  Midi_mode_type type;
  char const *path;
} Midi_mode_osc_bidule;

#ifdef FEAT_PORTMIDI
typedef struct {
  Midi_mode_type type;
  PmDeviceID device_id;
  PortMidiStream *stream;
} Midi_mode_portmidi;
// Not sure whether it's OK to call Pm_Terminate() without having a successful
// call to Pm_Initialize() -- let's just treat it with tweezers.
static bool portmidi_is_initialized = false;
#endif

typedef union {
  Midi_mode_any any;
  Midi_mode_osc_bidule osc_bidule;
#ifdef FEAT_PORTMIDI
  Midi_mode_portmidi portmidi;
#endif
} Midi_mode;

void midi_mode_init_null(Midi_mode *mm) { mm->any.type = Midi_mode_type_null; }
void midi_mode_init_osc_bidule(Midi_mode *mm, char const *path) {
  mm->osc_bidule.type = Midi_mode_type_osc_bidule;
  mm->osc_bidule.path = path;
}
#ifdef FEAT_PORTMIDI
enum {
  Portmidi_artificial_latency = 1,
};
struct {
  U64 clock_base;
  bool did_init;
} portmidi_global_data;
static PmTimestamp portmidi_timestamp_now(void) {
  if (!portmidi_global_data.did_init) {
    portmidi_global_data.did_init = true;
    portmidi_global_data.clock_base = stm_now();
  }
  return (PmTimestamp)(stm_ms(stm_since(portmidi_global_data.clock_base)));
}
static PmTimestamp portmidi_timeproc(void *time_info) {
  (void)time_info;
  return portmidi_timestamp_now();
}
static PmError portmidi_init_if_necessary(void) {
  if (portmidi_is_initialized)
    return 0;
  PmError e = Pm_Initialize();
  if (e)
    return e;
  portmidi_is_initialized = true;
  return 0;
}
staticni PmError midi_mode_init_portmidi(Midi_mode *mm, PmDeviceID dev_id) {
  PmError e = portmidi_init_if_necessary();
  if (e)
    goto fail;
  e = Pm_OpenOutput(&mm->portmidi.stream, dev_id, NULL, 128, portmidi_timeproc,
                    NULL, Portmidi_artificial_latency);
  if (e)
    goto fail;
  mm->portmidi.type = Midi_mode_type_portmidi;
  mm->portmidi.device_id = dev_id;
  return pmNoError;
fail:
  midi_mode_init_null(mm);
  return e;
}
// Returns true on success. todo currently output only
staticni bool portmidi_find_device_id_by_name(char const *name, Usz namelen,
                                              PmError *out_pmerror,
                                              PmDeviceID *out_id) {
  *out_pmerror = portmidi_init_if_necessary();
  if (*out_pmerror)
    return false;
  int num = Pm_CountDevices();
  for (int i = 0; i < num; ++i) {
    PmDeviceInfo const *info = Pm_GetDeviceInfo(i);
    if (!info || !info->output)
      continue;
    Usz len = strlen(info->name);
    if (len != namelen)
      continue;
    if (strncmp(name, info->name, namelen) == 0) {
      *out_id = i;
      return true;
    }
  }
  return false;
}
static bool portmidi_find_name_of_device_id(PmDeviceID id, PmError *out_pmerror,
                                            oso **out_name) {
  *out_pmerror = portmidi_init_if_necessary();
  if (*out_pmerror)
    return false;
  int num = Pm_CountDevices();
  if (id < 0 || id >= num)
    return false;
  PmDeviceInfo const *info = Pm_GetDeviceInfo(id);
  if (!info || !info->output)
    return false;
  osoput(out_name, info->name);
  return true;
}
#endif
staticni void midi_mode_deinit(Midi_mode *mm) {
  switch (mm->any.type) {
  case Midi_mode_type_null:
  case Midi_mode_type_osc_bidule:
    break;
#ifdef FEAT_PORTMIDI
  case Midi_mode_type_portmidi:
    // Because PortMidi seems to work correctly ony more platforms when using
    // its timing stuff, we are using it. And because we are using it, and
    // because it may be buffering events for sending 'later', we might have
    // pending outgoing MIDI events. We'll need to wait until they finish being
    // before calling Pm_Close, otherwise users could have problems like MIDI
    // notes being stuck on. This is slow and blocking, but not much we can do
    // about it right now.
    //
    // TODO use nansleep on platforms that support it.
    for (U64 start = stm_now();
         stm_ms(stm_since(start)) <= (double)Portmidi_artificial_latency;)
      sleep(0);
    Pm_Close(mm->portmidi.stream);
    break;
#endif
  }
}

typedef struct {
  Field field;
  Field scratch_field;
  Field clipboard_field;
  Mbuf_reusable mbuf_r;
  Undo_history undo_hist;
  Oevent_list oevent_list;
  Oevent_list scratch_oevent_list;
  Susnote_list susnote_list;
  Ged_cursor ged_cursor;
  Usz tick_num;
  Usz ruler_spacing_y, ruler_spacing_x;
  Ged_input_mode input_mode;
  Usz bpm;
  U64 clock;
  double accum_secs;
  double time_to_next_note_off;
  Oosc_dev *oosc_dev;
  Midi_mode midi_mode;
  Usz activity_counter;
  Usz random_seed;
  Usz drag_start_y, drag_start_x;
  int win_h, win_w;
  int softmargin_y, softmargin_x;
  int grid_h;
  int grid_scroll_y, grid_scroll_x; // not sure if i like this being int
  U8 midi_bclock_sixths;            // 0..5, holds 6th of the quarter note step
  bool needs_remarking : 1;
  bool is_draw_dirty : 1;
  bool is_playing : 1;
  bool midi_bclock : 1;
  bool draw_event_list : 1;
  bool is_mouse_down : 1;
  bool is_mouse_dragging : 1;
  bool is_hud_visible : 1;
} Ged;

static void ged_init(Ged *a, Usz undo_limit, Usz init_bpm, Usz init_seed) {
  field_init(&a->field);
  field_init(&a->scratch_field);
  field_init(&a->clipboard_field);
  mbuf_reusable_init(&a->mbuf_r);
  undo_history_init(&a->undo_hist, undo_limit);
  oevent_list_init(&a->oevent_list);
  oevent_list_init(&a->scratch_oevent_list);
  susnote_list_init(&a->susnote_list);
  ged_cursor_init(&a->ged_cursor);
  a->tick_num = 0;
  a->ruler_spacing_y = a->ruler_spacing_x = 8;
  a->input_mode = Ged_input_mode_normal;
  a->bpm = init_bpm;
  a->clock = 0;
  a->accum_secs = 0.0;
  a->time_to_next_note_off = 1.0;
  a->oosc_dev = NULL;
  midi_mode_init_null(&a->midi_mode);
  a->activity_counter = 0;
  a->random_seed = init_seed;
  a->drag_start_y = a->drag_start_x = 0;
  a->win_h = a->win_w = 0;
  a->softmargin_y = a->softmargin_x = 0;
  a->grid_h = 0;
  a->grid_scroll_y = a->grid_scroll_x = 0;
  a->midi_bclock_sixths = 0;
  a->needs_remarking = true;
  a->is_draw_dirty = false;
  a->is_playing = false;
  a->midi_bclock = false;
  a->draw_event_list = false;
  a->is_mouse_down = false;
  a->is_mouse_dragging = false;
  a->is_hud_visible = false;
}

static void ged_deinit(Ged *a) {
  field_deinit(&a->field);
  field_deinit(&a->scratch_field);
  field_deinit(&a->clipboard_field);
  mbuf_reusable_deinit(&a->mbuf_r);
  undo_history_deinit(&a->undo_hist);
  oevent_list_deinit(&a->oevent_list);
  oevent_list_deinit(&a->scratch_oevent_list);
  susnote_list_deinit(&a->susnote_list);
  if (a->oosc_dev)
    oosc_dev_destroy(a->oosc_dev);
  midi_mode_deinit(&a->midi_mode);
}

static bool ged_is_draw_dirty(Ged *a) {
  return a->is_draw_dirty || a->needs_remarking;
}

staticni void send_midi_3bytes(Oosc_dev *oosc_dev, Midi_mode const *midi_mode,
                               int status, int byte1, int byte2) {
  switch (midi_mode->any.type) {
  case Midi_mode_type_null:
    break;
  case Midi_mode_type_osc_bidule: {
    if (!oosc_dev)
      break;
    oosc_send_int32s(oosc_dev, midi_mode->osc_bidule.path,
                     (int[]){status, byte1, byte2}, 3);
    break;
  }
#ifdef FEAT_PORTMIDI
  case Midi_mode_type_portmidi: {
    // timestamp is totally fake, to prevent problems with some MIDI systems
    // getting angry if there's no timestamping info.
    //
    // Eventually, we will want to create real timestamps based on a real orca
    // clock, instead of ad-hoc at the last moment like this. When we do that,
    // we'll need to thread the timestamping/timing info through the function
    // calls, instead of creating it at the last moment here. (This timestamp
    // is actually 'useless', because it doesn't convey any additional
    // information. But if we don't provide it, at least to PortMidi, some
    // people's MIDI setups may malfunction and have terrible timing problems.)
    PmTimestamp pm_timestamp = portmidi_timestamp_now();
    PmError pme = Pm_WriteShort(midi_mode->portmidi.stream, pm_timestamp,
                                Pm_Message(status, byte1, byte2));
    (void)pme;
    break;
  }
#endif
  }
}

static void send_midi_chan_msg(Oosc_dev *oosc_dev, Midi_mode const *midi_mode,
                               int type /*0..15*/, int chan /*0.. 15*/,
                               int byte1 /*0..127*/, int byte2 /*0..127*/) {
  send_midi_3bytes(oosc_dev, midi_mode, type << 4 | chan, byte1, byte2);
}

static void send_midi_byte(Oosc_dev *oosc_dev, Midi_mode const *midi_mode,
                           int x) {
  // PortMidi wants 0 and 0 for the unused bytes. Likewise, Bidule's
  // MIDI-via-OSC won't accept the message unless there are at least all 3
  // bytes, with the second 2 set to zero.
  send_midi_3bytes(oosc_dev, midi_mode, x, 0, 0);
}

staticni void //
send_midi_note_offs(Oosc_dev *oosc_dev, Midi_mode *midi_mode,
                    Susnote const *start, Susnote const *end) {
  for (; start != end; ++start) {
#if 0
    float under = start->remaining;
    if (under < 0.0) {
      fprintf(stderr, "cutoff slop: %f\n", under);
    }
#endif
    U16 chan_note = start->chan_note;
    send_midi_chan_msg(oosc_dev, midi_mode, 0x8, chan_note >> 8,
                       chan_note & 0xFF, 0);
  }
}

static void send_control_message(Oosc_dev *oosc_dev, char const *osc_address) {
  if (!oosc_dev)
    return;
  oosc_send_int32s(oosc_dev, osc_address, NULL, 0);
}

static void send_num_message(Oosc_dev *oosc_dev, char const *osc_address,
                             I32 num) {
  if (!oosc_dev)
    return;
  I32 nums[1];
  nums[0] = num;
  oosc_send_int32s(oosc_dev, osc_address, nums, ORCA_ARRAY_COUNTOF(nums));
}

staticni void apply_time_to_sustained_notes(Oosc_dev *oosc_dev,
                                            Midi_mode *midi_mode,
                                            double time_elapsed,
                                            Susnote_list *susnote_list,
                                            double *next_note_off_deadline) {
  Usz start_removed, end_removed;
  susnote_list_advance_time(susnote_list, time_elapsed, &start_removed,
                            &end_removed, next_note_off_deadline);
  if (ORCA_UNLIKELY(start_removed != end_removed)) {
    Susnote const *restrict susnotes_off = susnote_list->buffer;
    send_midi_note_offs(oosc_dev, midi_mode, susnotes_off + start_removed,
                        susnotes_off + end_removed);
  }
}

staticni void ged_stop_all_sustained_notes(Ged *a) {
  Susnote_list *sl = &a->susnote_list;
  send_midi_note_offs(a->oosc_dev, &a->midi_mode, sl->buffer,
                      sl->buffer + sl->count);
  susnote_list_clear(sl);
  a->time_to_next_note_off = 1.0;
}

// The way orca handles MIDI sustains, timing, and overlapping note-ons (plus
// the 'mono' thing being added) has changed multiple times over time. Now we
// are in a situation where this function is a complete mess and needs an
// overhaul. If you see something in the function below and think, "wait, that
// seems redundant/weird", that's because it is, not because there's a good
// reason.

staticni void send_output_events(Oosc_dev *oosc_dev, Midi_mode *midi_mode,
                                 Usz bpm, Susnote_list *susnote_list,
                                 Oevent const *events, Usz count) {
  enum { Midi_on_capacity = 512 };
  typedef struct {
    U8 channel;
    U8 note_number;
    U8 velocity;
  } Midi_note_on;
  typedef struct {
    U8 note_number;
    U8 velocity;
    U8 duration;
  } Midi_mono_on;
  Midi_note_on midi_note_ons[Midi_on_capacity];
  Midi_mono_on midi_mono_ons[16]; // Keep only a single one per channel
  Susnote new_susnotes[Midi_on_capacity];
  Usz midi_note_count = 0;
  Usz monofied_chans = 0; // bitset of channels with new mono notes
  double frame_secs = 60.0 / (double)bpm / 4.0;

  for (Usz i = 0; i < count; ++i) {
    Oevent const *e = events + i;
    switch ((Oevent_types)e->any.oevent_type) {
    case Oevent_type_midi_note: {
      if (midi_note_count == Midi_on_capacity)
        break;
      Oevent_midi_note const *em = &e->midi_note;
      Usz note_number = (Usz)(12u * em->octave + em->note);
      if (note_number > 127)
        note_number = 127;
      Usz channel = em->channel;
      if (channel > 15)
        break;
      if (em->mono) {
        // 'mono' note-ons are strange. The more typical branch you'd expect to
        // see, where you can play multiple notes per channel, is below.
        monofied_chans |= 1u << (channel & 0xFu);
        midi_mono_ons[channel] = (Midi_mono_on){.note_number = (U8)note_number,
                                                .velocity = em->velocity,
                                                .duration = em->duration};
      } else {
        midi_note_ons[midi_note_count] =
            (Midi_note_on){.channel = (U8)channel,
                           .note_number = (U8)note_number,
                           .velocity = em->velocity};
        new_susnotes[midi_note_count] =
            (Susnote){.remaining = (float)(frame_secs * (double)em->duration),
                      .chan_note = (U16)((channel << 8u) | note_number)};
        ++midi_note_count;
      }
      break;
    }
    case Oevent_type_midi_cc: {
      Oevent_midi_cc const *ec = &e->midi_cc;
      // Note that we're not preserving the exact order of MIDI events as
      // emitted by the orca VM. Notes and CCs that are emitted in the same
      // step will always have the CCs sent first. Not sure if this is OK or
      // not. If it's not OK, we can either loop again a second time to always
      // send CCs after notes, or if that's not also OK, we can make the stack
      // buffer more complicated and interleave the CCs in it.
      send_midi_chan_msg(oosc_dev, midi_mode, 0xb, ec->channel, ec->control,
                         ec->value);
      break;
    }
    case Oevent_type_midi_pb: {
      Oevent_midi_pb const *ep = &e->midi_pb;
      // Same caveat regarding ordering with MIDI CC also applies here.
      send_midi_chan_msg(oosc_dev, midi_mode, 0xe, ep->channel, ep->lsb,
                         ep->msb);
      break;
    }
    case Oevent_type_osc_ints: {
      // kinda lame
      if (!oosc_dev)
        continue;
      Oevent_osc_ints const *eo = &e->osc_ints;
      char path[] = {'/', eo->glyph, '\0'};
      I32 ints[ORCA_ARRAY_COUNTOF(eo->numbers)];
      Usz nnum = eo->count;
      for (Usz inum = 0; inum < nnum; ++inum) {
        ints[inum] = eo->numbers[inum];
      }
      oosc_send_int32s(oosc_dev, path, ints, nnum);
      break;
    }
    case Oevent_type_udp_string: {
      if (!oosc_dev)
        continue;
      Oevent_udp_string const *eo = &e->udp_string;
      oosc_send_datagram(oosc_dev, eo->chars, eo->count);
      break;
    }
    }
  }

do_note_ons:
  if (midi_note_count > 0) {
    Usz start_note_offs, end_note_offs;
    susnote_list_add_notes(susnote_list, new_susnotes, midi_note_count,
                           &start_note_offs, &end_note_offs);
    if (start_note_offs != end_note_offs) {
      Susnote const *restrict susnotes_off = susnote_list->buffer;
      send_midi_note_offs(oosc_dev, midi_mode, susnotes_off + start_note_offs,
                          susnotes_off + end_note_offs);
    }
    for (Usz i = 0; i < midi_note_count; ++i) {
      Midi_note_on mno = midi_note_ons[i];
      send_midi_chan_msg(oosc_dev, midi_mode, 0x9, mno.channel, mno.note_number,
                         mno.velocity);
    }
  }
  if (monofied_chans) {
    // The behavior we end up with is that if regular note-ons are played in
    // the same frame/step as a mono, the regular note-ons will have the actual
    // MIDI note on sent, followed immediately by a MIDI note off. I don't know
    // if this is good or not.
    Usz start_note_offs, end_note_offs;
    susnote_list_remove_by_chan_mask(susnote_list, monofied_chans,
                                     &start_note_offs, &end_note_offs);
    if (start_note_offs != end_note_offs) {
      Susnote const *restrict susnotes_off = susnote_list->buffer;
      send_midi_note_offs(oosc_dev, midi_mode, susnotes_off + start_note_offs,
                          susnotes_off + end_note_offs);
    }
    midi_note_count = 0; // We're going to use this list again. Reset it.
    for (Usz i = 0; i < 16; i++) { // Add these notes to list of note-ons
      if (!(monofied_chans & 1u << i))
        continue;
      midi_note_ons[midi_note_count] =
          (Midi_note_on){.channel = (U8)i,
                         .note_number = midi_mono_ons[i].note_number,
                         .velocity = midi_mono_ons[i].velocity};
      new_susnotes[midi_note_count] = (Susnote){
          .remaining = (float)(frame_secs * (double)midi_mono_ons[i].duration),
          .chan_note = (U16)((i << 8u) | midi_mono_ons[i].note_number)};
      midi_note_count++;
    }
    monofied_chans = false;
    goto do_note_ons; // lol super wasteful for doing susnotes again
  }
}

staticni void ged_clear_osc_udp(Ged *a) {
  if (a->oosc_dev) {
    if (a->midi_mode.any.type == Midi_mode_type_osc_bidule) {
      ged_stop_all_sustained_notes(a);
    }
    oosc_dev_destroy(a->oosc_dev);
    a->oosc_dev = NULL;
  }
}
static bool ged_is_using_osc_udp(Ged *a) { return (bool)a->oosc_dev; }
static bool ged_set_osc_udp(Ged *a, char const *dest_addr,
                            char const *dest_port) {
  ged_clear_osc_udp(a);
  if (dest_port) {
    Oosc_udp_create_error err =
        oosc_dev_create_udp(&a->oosc_dev, dest_addr, dest_port);
    if (err) {
      return false;
    }
  }
  return true;
}

static ORCA_FORCEINLINE double ms_to_sec(double ms) { return ms / 1000.0; }

static double ged_secs_to_deadline(Ged const *a) {
  if (!a->is_playing)
    return 1.0;
  double secs_span = 60.0 / (double)a->bpm / 4.0;
  // If MIDI beat clock output is enabled, we need to send an event every 24
  // parts per quarter note. Since we've already divided quarter notes into 4
  // for ORCA's timing semantics, divide it by a further 6. This same logic is
  // mirrored in ged_do_stuff().
  if (a->midi_bclock)
    secs_span /= 6.0;
  double rem = secs_span - (stm_sec(stm_since(a->clock)) + a->accum_secs);
  double next_note_off = a->time_to_next_note_off;
  if (next_note_off < rem)
    rem = next_note_off;
  if (rem < 0.0)
    rem = 0.0;
  return rem;
}

staticni void clear_and_run_vm(Glyph *restrict gbuf, Mark *restrict mbuf,
                               Usz height, Usz width, Usz tick_number,
                               Oevent_list *oevent_list, Usz random_seed) {
  mbuffer_clear(mbuf, height, width);
  oevent_list_clear(oevent_list);
  orca_run(gbuf, mbuf, height, width, tick_number, oevent_list, random_seed);
}

staticni void ged_do_stuff(Ged *a) {
  if (!a->is_playing)
    return;
  double secs_span = 60.0 / (double)a->bpm / 4.0;
  if (a->midi_bclock) // see also ged_secs_to_deadline()
    secs_span /= 6.0;
  Oosc_dev *oosc_dev = a->oosc_dev;
  Midi_mode *midi_mode = &a->midi_mode;
  bool crossed_deadline = false;
#if TIME_DEBUG
  Usz spins = 0;
  U64 spin_start = stm_now();
  (void)spin_start;
#endif
  for (;;) {
    U64 now = stm_now();
    U64 diff = stm_diff(now, a->clock);
    double sdiff = stm_sec(diff) + a->accum_secs;
    if (sdiff >= secs_span) {
      a->clock = now;
      a->accum_secs = sdiff - secs_span;
#if TIME_DEBUG
      if (a->accum_secs > 0.000001) {
        fprintf(stderr, "late: %.2f u-secs\n", a->accum_secs * 1000 * 1000);
        if (a->accum_secs > 0.00005) {
          fprintf(stderr, "guilty timeout: %d\n", spin_track_timeout);
        }
      }
#endif
      crossed_deadline = true;
      break;
    }
    if (secs_span - sdiff > ms_to_sec(0.1))
      break;
#if TIME_DEBUG
    ++spins;
#endif
  }
#if TIME_DEBUG
  if (spins > 0) {
    fprintf(stderr, "%d spins in %f us with timeout %d\n", (int)spins,
            stm_us(stm_since(spin_start)), spin_track_timeout);
  }
#endif
  if (!crossed_deadline)
    return;
  if (a->midi_bclock) {
    send_midi_byte(oosc_dev, midi_mode, 0xF8); // MIDI beat clock
    Usz sixths = a->midi_bclock_sixths;
    a->midi_bclock_sixths = (U8)((sixths + 1) % 6);
    if (sixths != 0)
      return;
  }
  apply_time_to_sustained_notes(oosc_dev, midi_mode, secs_span,
                                &a->susnote_list, &a->time_to_next_note_off);
  clear_and_run_vm(a->field.buffer, a->mbuf_r.buffer, a->field.height,
                   a->field.width, a->tick_num, &a->oevent_list,
                   a->random_seed);
  ++a->tick_num;
  a->needs_remarking = true;
  a->is_draw_dirty = true;

  Usz count = a->oevent_list.count;
  if (count > 0) {
    send_output_events(oosc_dev, midi_mode, a->bpm, &a->susnote_list,
                       a->oevent_list.buffer, count);
    a->activity_counter += count;
  }
}

static inline Isz isz_clamp(Isz x, Isz low, Isz high) {
  return x < low ? low : x > high ? high : x;
}

// todo cleanup to use proper unsigned/signed w/ overflow check
staticni Isz scroll_offset_on_axis_for_cursor_pos(Isz win_len, Isz cont_len,
                                                  Isz cursor_pos, Isz pad,
                                                  Isz cur_scroll) {
  if (win_len <= 0 || cont_len <= 0)
    return 0;
  if (cont_len <= win_len)
    return -((win_len - cont_len) / 2);
  if (pad * 2 >= win_len) {
    pad = (win_len - 1) / 2;
  }
  Isz min_vis_scroll = cursor_pos - win_len + 1 + pad;
  Isz max_vis_scroll = cursor_pos - pad;
  Isz new_scroll;
  if (cur_scroll < min_vis_scroll)
    new_scroll = min_vis_scroll;
  else if (cur_scroll > max_vis_scroll)
    new_scroll = max_vis_scroll;
  else
    new_scroll = cur_scroll;
  return isz_clamp(new_scroll, 0, cont_len - win_len);
}

staticni void ged_make_cursor_visible(Ged *a) {
  int grid_h = a->grid_h;
  int cur_scr_y = a->grid_scroll_y;
  int cur_scr_x = a->grid_scroll_x;
  int new_scr_y = (int)scroll_offset_on_axis_for_cursor_pos(
      grid_h, (Isz)a->field.height, (Isz)a->ged_cursor.y, 5, cur_scr_y);
  int new_scr_x = (int)scroll_offset_on_axis_for_cursor_pos(
      a->win_w, (Isz)a->field.width, (Isz)a->ged_cursor.x, 5, cur_scr_x);
  if (new_scr_y == cur_scr_y && new_scr_x == cur_scr_x)
    return;
  a->grid_scroll_y = new_scr_y;
  a->grid_scroll_x = new_scr_x;
  a->is_draw_dirty = true;
}

enum { Hud_height = 2 };

staticni void ged_update_internal_geometry(Ged *a) {
  int win_h = a->win_h;
  int softmargin_y = a->softmargin_y;
  bool show_hud = win_h > Hud_height + 1;
  int grid_h = show_hud ? win_h - Hud_height : win_h;
  if (grid_h > a->field.height) {
    int halfy = (grid_h - a->field.height + 1) / 2;
    grid_h -= halfy < softmargin_y ? halfy : softmargin_y;
  }
  a->grid_h = grid_h;
  a->is_draw_dirty = true;
  a->is_hud_visible = show_hud;
}

staticni void ged_set_window_size(Ged *a, int win_h, int win_w,
                                  int softmargin_y, int softmargin_x) {
  if (a->win_h == win_h && a->win_w == win_w &&
      a->softmargin_y == softmargin_y && a->softmargin_x == softmargin_x)
    return;
  a->win_h = win_h;
  a->win_w = win_w;
  a->softmargin_y = softmargin_y;
  a->softmargin_x = softmargin_x;
  ged_update_internal_geometry(a);
  ged_make_cursor_visible(a);
}

staticni void ged_draw(Ged *a, WINDOW *win, char const *filename,
                       bool use_fancy_dots, bool use_fancy_rulers) {
  // We can predictavely step the next simulation tick and then use the
  // resulting mark buffer for better UI visualization. If we don't do this,
  // after loading a fresh file or after the user performs some edit (or even
  // after a regular simulation step), the new glyph buffer won't have had
  // phase 0 of the simulation run, which means the ports and other flags won't
  // be set on the mark buffer, so the colors for disabled cells, ports, etc.
  // won't be set.
  //
  // We can just perform a simulation step using the current state, keep the
  // mark buffer that it produces, then roll back the glyph buffer to where it
  // was before. This should produce results similar to having specialized UI
  // code that looks at each glyph and figures out the ports, etc.
  if (a->needs_remarking && !a->is_playing) {
    field_resize_raw_if_necessary(&a->scratch_field, a->field.height,
                                  a->field.width);
    field_copy(&a->field, &a->scratch_field);
    mbuf_reusable_ensure_size(&a->mbuf_r, a->field.height, a->field.width);
    clear_and_run_vm(a->scratch_field.buffer, a->mbuf_r.buffer, a->field.height,
                     a->field.width, a->tick_num, &a->scratch_oevent_list,
                     a->random_seed);
    a->needs_remarking = false;
  }
  int win_w = a->win_w;
  draw_glyphs_grid_scrolled(
      win, 0, 0, a->grid_h, win_w, a->field.buffer, a->mbuf_r.buffer,
      a->field.height, a->field.width, a->grid_scroll_y, a->grid_scroll_x,
      a->ruler_spacing_y, a->ruler_spacing_x, use_fancy_dots, use_fancy_rulers);
  draw_grid_cursor(win, 0, 0, a->grid_h, win_w, a->field.buffer,
                   a->field.height, a->field.width, a->grid_scroll_y,
                   a->grid_scroll_x, a->ged_cursor.y, a->ged_cursor.x,
                   a->ged_cursor.h, a->ged_cursor.w, a->input_mode,
                   a->is_playing);
  if (a->is_hud_visible) {
    filename = filename ? filename : "unnamed";
    int hud_x = win_w > 50 + a->softmargin_x * 2 ? a->softmargin_x : 0;
    draw_hud(win, a->grid_h, hud_x, Hud_height, win_w, filename,
             a->field.height, a->field.width, a->ruler_spacing_y,
             a->ruler_spacing_x, a->tick_num, a->bpm, &a->ged_cursor,
             a->input_mode, a->activity_counter);
  }
  if (a->draw_event_list)
    draw_oevent_list(win, &a->oevent_list);
  a->is_draw_dirty = false;
}

staticni void ged_send_osc_bpm(Ged *a, I32 bpm) {
  send_num_message(a->oosc_dev, "/orca/bpm", bpm);
}

staticni void ged_adjust_bpm(Ged *a, Isz delta_bpm) {
  Isz new_bpm = (Isz)a->bpm;
  if (delta_bpm < 0 || new_bpm < INT_MAX - delta_bpm)
    new_bpm += delta_bpm;
  else
    new_bpm = INT_MAX;
  if (new_bpm < 1)
    new_bpm = 1;
  if ((Usz)new_bpm != a->bpm) {
    a->bpm = (Usz)new_bpm;
    a->is_draw_dirty = true;
    ged_send_osc_bpm(a, (I32)new_bpm);
  }
}

static void ged_move_cursor_relative(Ged *a, Isz delta_y, Isz delta_x) {
  ged_cursor_move_relative(&a->ged_cursor, a->field.height, a->field.width,
                           delta_y, delta_x);
  ged_make_cursor_visible(a);
  a->is_draw_dirty = true;
}

static Usz guarded_selection_axis_resize(Usz x, int delta) {
  if (delta < 0) {
    if (delta > INT_MIN && (Usz)(-delta) < x) {
      x -= (Usz)(-delta);
    }
  } else if (x < SIZE_MAX - (Usz)delta) {
    x += (Usz)delta;
  }
  return x;
}

staticni void ged_modify_selection_size(Ged *a, int delta_y, int delta_x) {
  Usz cur_h = a->ged_cursor.h, cur_w = a->ged_cursor.w;
  Usz new_h = guarded_selection_axis_resize(cur_h, delta_y);
  Usz new_w = guarded_selection_axis_resize(cur_w, delta_x);
  if (cur_h != new_h || cur_w != new_w) {
    a->ged_cursor.h = new_h;
    a->ged_cursor.w = new_w;
    a->is_draw_dirty = true;
  }
}

staticni bool ged_try_selection_clipped_to_field(Ged const *a, Usz *out_y,
                                                 Usz *out_x, Usz *out_h,
                                                 Usz *out_w) {
  Usz curs_y = a->ged_cursor.y, curs_x = a->ged_cursor.x;
  Usz curs_h = a->ged_cursor.h, curs_w = a->ged_cursor.w;
  Usz field_h = a->field.height, field_w = a->field.width;
  if (curs_y >= field_h || curs_x >= field_w)
    return false;
  if (field_h - curs_y < curs_h)
    curs_h = field_h - curs_y;
  if (field_w - curs_x < curs_w)
    curs_w = field_w - curs_x;
  *out_y = curs_y;
  *out_x = curs_x;
  *out_h = curs_h;
  *out_w = curs_w;
  return true;
}

staticni bool ged_slide_selection(Ged *a, int delta_y, int delta_x) {
  Usz curs_y_0, curs_x_0, curs_h_0, curs_w_0;
  Usz curs_y_1, curs_x_1, curs_h_1, curs_w_1;
  if (!ged_try_selection_clipped_to_field(a, &curs_y_0, &curs_x_0, &curs_h_0,
                                          &curs_w_0))
    return false;
  ged_move_cursor_relative(a, delta_y, delta_x);
  if (!ged_try_selection_clipped_to_field(a, &curs_y_1, &curs_x_1, &curs_h_1,
                                          &curs_w_1))
    return false;
  // Don't create a history entry if nothing is going to happen.
  if (curs_y_0 == curs_y_1 && curs_x_0 == curs_x_1 && curs_h_0 == curs_h_1 &&
      curs_w_0 == curs_w_1)
    return false;
  undo_history_push(&a->undo_hist, &a->field, a->tick_num);
  Usz field_h = a->field.height;
  Usz field_w = a->field.width;
  gbuffer_copy_subrect(a->field.buffer, a->field.buffer, field_h, field_w,
                       field_h, field_w, curs_y_0, curs_x_0, curs_y_1, curs_x_1,
                       curs_h_0, curs_w_0);
  // Erase/clear the area that was within the selection rectangle in the
  // starting position, but wasn't written to during the copy. (In other words,
  // this is the area that was 'left behind' when we moved the selection
  // rectangle, plus any area that was along the bottom and right edge of the
  // field that didn't have anything to copy to it when the selection rectangle
  // extended outside of the field.)
  Usz ey, eh, ex, ew;
  if (curs_y_1 > curs_y_0) {
    ey = curs_y_0;
    eh = curs_y_1 - curs_y_0;
  } else {
    ey = curs_y_1 + curs_h_0;
    eh = (curs_y_0 + curs_h_0) - ey;
  }
  if (curs_x_1 > curs_x_0) {
    ex = curs_x_0;
    ew = curs_x_1 - curs_x_0;
  } else {
    ex = curs_x_1 + curs_w_0;
    ew = (curs_x_0 + curs_w_0) - ex;
  }
  gbuffer_fill_subrect(a->field.buffer, field_h, field_w, ey, curs_x_0, eh,
                       curs_w_0, '.');
  gbuffer_fill_subrect(a->field.buffer, field_h, field_w, curs_y_0, ex,
                       curs_h_0, ew, '.');
  a->needs_remarking = true;
  return true;
}

typedef enum {
  Ged_dir_up,
  Ged_dir_down,
  Ged_dir_left,
  Ged_dir_right,
} Ged_dir;

staticni void ged_dir_input(Ged *a, Ged_dir dir, int step_length) {
  switch (a->input_mode) {
  case Ged_input_mode_normal:
  case Ged_input_mode_append:
    switch (dir) {
    case Ged_dir_up:
      ged_move_cursor_relative(a, -step_length, 0);
      break;
    case Ged_dir_down:
      ged_move_cursor_relative(a, step_length, 0);
      break;
    case Ged_dir_left:
      ged_move_cursor_relative(a, 0, -step_length);
      break;
    case Ged_dir_right:
      ged_move_cursor_relative(a, 0, step_length);
      break;
    }
    break;
  case Ged_input_mode_selresize:
    switch (dir) {
    case Ged_dir_up:
      ged_modify_selection_size(a, -step_length, 0);
      break;
    case Ged_dir_down:
      ged_modify_selection_size(a, step_length, 0);
      break;
    case Ged_dir_left:
      ged_modify_selection_size(a, 0, -step_length);
      break;
    case Ged_dir_right:
      ged_modify_selection_size(a, 0, step_length);
      break;
    }
    break;
  case Ged_input_mode_slide:
    switch (dir) {
    case Ged_dir_up:
      ged_slide_selection(a, -step_length, 0);
      break;
    case Ged_dir_down:
      ged_slide_selection(a, step_length, 0);
      break;
    case Ged_dir_left:
      ged_slide_selection(a, 0, -step_length);
      break;
    case Ged_dir_right:
      ged_slide_selection(a, 0, step_length);
      break;
    }
    break;
  }
}

static Usz view_to_scrolled_grid(Usz field_len, Usz visual_coord,
                                 int scroll_offset) {
  if (field_len == 0)
    return 0;
  if (scroll_offset < 0) {
    if ((Usz)(-scroll_offset) <= visual_coord) {
      visual_coord -= (Usz)(-scroll_offset);
    } else {
      visual_coord = 0;
    }
  } else {
    visual_coord += (Usz)scroll_offset;
  }
  if (visual_coord >= field_len)
    visual_coord = field_len - 1;
  return visual_coord;
}

staticni void ged_mouse_event(Ged *a, Usz vis_y, Usz vis_x,
                              mmask_t mouse_bstate) {
  if (mouse_bstate & BUTTON1_RELEASED) {
    // hard-disables tracking, but also disables further mouse stuff.
    // mousemask() with our original parameters seems to work to get into the
    // state we want, though.
    //
    // printf("\033[?1003l\n");
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    a->is_mouse_down = false;
    a->is_mouse_dragging = false;
    a->drag_start_y = 0;
    a->drag_start_x = 0;
  } else if ((mouse_bstate & BUTTON1_PRESSED) || a->is_mouse_down) {
    Usz y = view_to_scrolled_grid(a->field.height, vis_y, a->grid_scroll_y);
    Usz x = view_to_scrolled_grid(a->field.width, vis_x, a->grid_scroll_x);
    if (!a->is_mouse_down) {
      // some sequence to hopefully make terminal start reporting all further
      // mouse movement events. 'REPORT_MOUSE_POSITION' alone in the mousemask
      // doesn't seem to work, at least not for xterm. we need to set it only
      // only when needed, otherwise some terminals will send movement updates
      // when we don't want them.
      printf("\033[?1003h\n");
      // need to do this or double clicking can cause terminal state to get
      // corrupted, since we're bypassing curses here. might cause flicker.
      // wish i could figure out why report mouse position isn't working on its
      // own.
      fflush(stdout);
      wclear(stdscr);
      a->is_mouse_down = true;
      a->ged_cursor.y = y;
      a->ged_cursor.x = x;
      a->ged_cursor.h = 1;
      a->ged_cursor.w = 1;
      a->is_draw_dirty = true;
    } else {
      if (!a->is_mouse_dragging &&
          (y != a->ged_cursor.y || x != a->ged_cursor.x)) {
        a->is_mouse_dragging = true;
        a->drag_start_y = a->ged_cursor.y;
        a->drag_start_x = a->ged_cursor.x;
      }
      if (a->is_mouse_dragging) {
        Usz tcy = a->drag_start_y;
        Usz tcx = a->drag_start_x;
        Usz loy = y < tcy ? y : tcy;
        Usz lox = x < tcx ? x : tcx;
        Usz hiy = y > tcy ? y : tcy;
        Usz hix = x > tcx ? x : tcx;
        a->ged_cursor.y = loy;
        a->ged_cursor.x = lox;
        a->ged_cursor.h = hiy - loy + 1;
        a->ged_cursor.w = hix - lox + 1;
        a->is_draw_dirty = true;
      }
    }
  }
#if defined(NCURSES_MOUSE_VERSION) && NCURSES_MOUSE_VERSION >= 2
  else {
    if (mouse_bstate & BUTTON4_PRESSED) {
      a->grid_scroll_y -= 1;
      a->is_draw_dirty = true;
    } else if (mouse_bstate & BUTTON5_PRESSED) {
      a->grid_scroll_y += 1;
      a->is_draw_dirty = true;
    }
  }
#endif
}

staticni void ged_adjust_rulers_relative(Ged *a, Isz delta_y, Isz delta_x) {
  Isz new_y = (Isz)a->ruler_spacing_y + delta_y;
  Isz new_x = (Isz)a->ruler_spacing_x + delta_x;
  if (new_y < 4)
    new_y = 4;
  else if (new_y > 16)
    new_y = 16;
  if (new_x < 4)
    new_x = 4;
  else if (new_x > 16)
    new_x = 16;
  if ((Usz)new_y == a->ruler_spacing_y && (Usz)new_x == a->ruler_spacing_x)
    return;
  a->ruler_spacing_y = (Usz)new_y;
  a->ruler_spacing_x = (Usz)new_x;
  a->is_draw_dirty = true;
}

staticni void ged_resize_grid_relative(Ged *a, Isz delta_y, Isz delta_x) {
  ged_resize_grid_snap_ruler(&a->field, &a->mbuf_r, a->ruler_spacing_y,
                             a->ruler_spacing_x, delta_y, delta_x, a->tick_num,
                             &a->scratch_field, &a->undo_hist, &a->ged_cursor);
  a->needs_remarking = true; // could check if we actually resized
  a->is_draw_dirty = true;
  ged_update_internal_geometry(a);
  ged_make_cursor_visible(a);
}

staticni void ged_write_character(Ged *a, char c) {
  undo_history_push(&a->undo_hist, &a->field, a->tick_num);
  gbuffer_poke(a->field.buffer, a->field.height, a->field.width,
               a->ged_cursor.y, a->ged_cursor.x, c);
  // Indicate we want the next simulation step to be run predictavely,
  // so that we can use the reulsting mark buffer for UI visualization.
  // This is "expensive", so it could be skipped for non-interactive
  // input in situations where max throughput is necessary.
  a->needs_remarking = true;
  if (a->input_mode == Ged_input_mode_append) {
    ged_cursor_move_relative(&a->ged_cursor, a->field.height, a->field.width, 0,
                             1);
  }
  a->is_draw_dirty = true;
}

staticni bool ged_fill_selection_with_char(Ged *a, Glyph c) {
  Usz curs_y, curs_x, curs_h, curs_w;
  if (!ged_try_selection_clipped_to_field(a, &curs_y, &curs_x, &curs_h,
                                          &curs_w))
    return false;
  gbuffer_fill_subrect(a->field.buffer, a->field.height, a->field.width, curs_y,
                       curs_x, curs_h, curs_w, c);
  return true;
}

staticni bool ged_copy_selection_to_clipbard(Ged *a) {
  Usz curs_y, curs_x, curs_h, curs_w;
  if (!ged_try_selection_clipped_to_field(a, &curs_y, &curs_x, &curs_h,
                                          &curs_w))
    return false;
  Usz field_h = a->field.height;
  Usz field_w = a->field.width;
  Field *cb_field = &a->clipboard_field;
  field_resize_raw_if_necessary(cb_field, curs_h, curs_w);
  gbuffer_copy_subrect(a->field.buffer, cb_field->buffer, field_h, field_w,
                       curs_h, curs_w, curs_y, curs_x, 0, 0, curs_h, curs_w);
  return true;
}

staticni void ged_input_character(Ged *a, char c) {
  switch (a->input_mode) {
  case Ged_input_mode_append:
    ged_write_character(a, c);
    break;
  case Ged_input_mode_normal:
  case Ged_input_mode_selresize:
  case Ged_input_mode_slide:
    if (a->ged_cursor.h <= 1 && a->ged_cursor.w <= 1) {
      ged_write_character(a, c);
    } else {
      undo_history_push(&a->undo_hist, &a->field, a->tick_num);
      ged_fill_selection_with_char(a, c);
      a->needs_remarking = true;
      a->is_draw_dirty = true;
    }
    break;
  }
}

typedef enum {
  Ged_input_cmd_undo,
  Ged_input_cmd_toggle_append_mode,
  Ged_input_cmd_toggle_selresize_mode,
  Ged_input_cmd_toggle_slide_mode,
  Ged_input_cmd_step_forward,
  Ged_input_cmd_toggle_show_event_list,
  Ged_input_cmd_toggle_play_pause,
  Ged_input_cmd_cut,
  Ged_input_cmd_copy,
  Ged_input_cmd_paste,
  Ged_input_cmd_escape,
} Ged_input_cmd;

staticni void ged_set_playing(Ged *a, bool playing) {
  if (playing == a->is_playing)
    return;
  if (playing) {
    undo_history_push(&a->undo_hist, &a->field, a->tick_num);
    a->is_playing = true;
    a->clock = stm_now();
    a->midi_bclock_sixths = 0;
    // dumb'n'dirty, get us close to the next step time, but not quite
    a->accum_secs = 60.0 / (double)a->bpm / 4.0;
    if (a->midi_bclock) {
      send_midi_byte(a->oosc_dev, &a->midi_mode, 0xFA); // "start"
      a->accum_secs /= 6.0;
    }
    a->accum_secs -= 0.0001;
    send_control_message(a->oosc_dev, "/orca/started");
  } else {
    ged_stop_all_sustained_notes(a);
    a->is_playing = false;
    send_control_message(a->oosc_dev, "/orca/stopped");
    if (a->midi_bclock)
      send_midi_byte(a->oosc_dev, &a->midi_mode, 0xFC); // "stop"
  }
  a->is_draw_dirty = true;
}

staticni void ged_input_cmd(Ged *a, Ged_input_cmd ev) {
  switch (ev) {
  case Ged_input_cmd_undo:
    if (undo_history_count(&a->undo_hist) == 0)
      break;
    if (a->is_playing)
      undo_history_apply(&a->undo_hist, &a->field, &a->tick_num);
    else
      undo_history_pop(&a->undo_hist, &a->field, &a->tick_num);
    ged_cursor_confine(&a->ged_cursor, a->field.height, a->field.width);
    ged_update_internal_geometry(a);
    ged_make_cursor_visible(a);
    a->needs_remarking = true;
    a->is_draw_dirty = true;
    break;
  case Ged_input_cmd_toggle_append_mode:
    a->input_mode = a->input_mode == Ged_input_mode_append
                        ? Ged_input_mode_normal
                        : Ged_input_mode_append;
    a->is_draw_dirty = true;
    break;
  case Ged_input_cmd_toggle_selresize_mode:
    a->input_mode = a->input_mode == Ged_input_mode_selresize
                        ? Ged_input_mode_normal
                        : Ged_input_mode_selresize;
    a->is_draw_dirty = true;
    break;
  case Ged_input_cmd_toggle_slide_mode:
    a->input_mode = a->input_mode == Ged_input_mode_slide
                        ? Ged_input_mode_normal
                        : Ged_input_mode_slide;
    a->is_draw_dirty = true;
    break;
  case Ged_input_cmd_step_forward:
    undo_history_push(&a->undo_hist, &a->field, a->tick_num);
    clear_and_run_vm(a->field.buffer, a->mbuf_r.buffer, a->field.height,
                     a->field.width, a->tick_num, &a->oevent_list,
                     a->random_seed);
    ++a->tick_num;
    a->activity_counter += a->oevent_list.count;
    a->needs_remarking = true;
    a->is_draw_dirty = true;
    break;
  case Ged_input_cmd_toggle_play_pause:
    ged_set_playing(a, !a->is_playing);
    break;
  case Ged_input_cmd_toggle_show_event_list:
    a->draw_event_list = !a->draw_event_list;
    a->is_draw_dirty = true;
    break;
  case Ged_input_cmd_cut:
    if (ged_copy_selection_to_clipbard(a)) {
      undo_history_push(&a->undo_hist, &a->field, a->tick_num);
      ged_fill_selection_with_char(a, '.');
      a->needs_remarking = true;
      a->is_draw_dirty = true;
    }
    break;
  case Ged_input_cmd_copy:
    ged_copy_selection_to_clipbard(a);
    break;
  case Ged_input_cmd_paste: {
    Usz field_h = a->field.height;
    Usz field_w = a->field.width;
    Usz curs_y = a->ged_cursor.y;
    Usz curs_x = a->ged_cursor.x;
    if (curs_y >= field_h || curs_x >= field_w)
      break;
    Field *cb_field = &a->clipboard_field;
    Usz cbfield_h = cb_field->height;
    Usz cbfield_w = cb_field->width;
    Usz cpy_h = cbfield_h;
    Usz cpy_w = cbfield_w;
    if (field_h - curs_y < cpy_h)
      cpy_h = field_h - curs_y;
    if (field_w - curs_x < cpy_w)
      cpy_w = field_w - curs_x;
    if (cpy_h == 0 || cpy_w == 0)
      break;
    undo_history_push(&a->undo_hist, &a->field, a->tick_num);
    gbuffer_copy_subrect(cb_field->buffer, a->field.buffer, cbfield_h,
                         cbfield_w, field_h, field_w, 0, 0, curs_y, curs_x,
                         cpy_h, cpy_w);
    a->ged_cursor.h = cpy_h;
    a->ged_cursor.w = cpy_w;
    a->needs_remarking = true;
    a->is_draw_dirty = true;
    break;
  }
  case Ged_input_cmd_escape:
    if (a->input_mode != Ged_input_mode_normal) {
      a->input_mode = Ged_input_mode_normal;
      a->is_draw_dirty = true;
    } else if (a->ged_cursor.h != 1 || a->ged_cursor.w != 1) {
      a->ged_cursor.h = 1;
      a->ged_cursor.w = 1;
      a->is_draw_dirty = true;
    }
    break;
  }
}

static bool hacky_try_save(Field *field, char const *filename) {
  if (!filename)
    return false;
  if (field->height == 0 || field->width == 0)
    return false;
  FILE *f = fopen(filename, "w");
  if (!f)
    return false;
  field_fput(field, f);
  fclose(f);
  return true;
}

//
// menu stuff
//

enum {
  Main_menu_id = 1,
  Open_form_id,
  Save_as_form_id,
  Set_tempo_form_id,
  Set_grid_dims_form_id,
  Autofit_menu_id,
  Confirm_new_file_menu_id,
  Cosmetics_menu_id,
  Osc_menu_id,
  Osc_output_address_form_id,
  Osc_output_port_form_id,
  Playback_menu_id,
  Set_soft_margins_form_id,
  Set_fancy_grid_dots_menu_id,
  Set_fancy_grid_rulers_menu_id,
#ifdef FEAT_PORTMIDI
  Portmidi_output_device_menu_id,
#endif
};
enum {
  Single_form_item_id = 1,
};
enum {
  Autofit_nicely_id = 1,
  Autofit_tightly_id,
};
enum {
  Confirm_new_file_reject_id = 1,
  Confirm_new_file_accept_id,
};
enum {
  Main_menu_quit = 1,
  Main_menu_controls,
  Main_menu_opers_guide,
  Main_menu_new,
  Main_menu_open,
  Main_menu_save,
  Main_menu_save_as,
  Main_menu_set_tempo,
  Main_menu_set_grid_dims,
  Main_menu_autofit_grid,
  Main_menu_about,
  Main_menu_cosmetics,
  Main_menu_playback,
  Main_menu_osc,
#ifdef FEAT_PORTMIDI
  Main_menu_choose_portmidi_output,
#endif
};

static void push_main_menu(void) {
  Qmenu *qm = qmenu_create(Main_menu_id);
  qmenu_set_title(qm, "ORCA");
  qmenu_add_choice(qm, Main_menu_new, "New");
  qmenu_add_choice(qm, Main_menu_open, "Open...");
  qmenu_add_choice(qm, Main_menu_save, "Save");
  qmenu_add_choice(qm, Main_menu_save_as, "Save As...");
  qmenu_add_spacer(qm);
  qmenu_add_choice(qm, Main_menu_set_tempo, "Set BPM...");
  qmenu_add_choice(qm, Main_menu_set_grid_dims, "Set Grid Size...");
  qmenu_add_choice(qm, Main_menu_autofit_grid, "Auto-fit Grid");
  qmenu_add_spacer(qm);
  qmenu_add_choice(qm, Main_menu_osc, "OSC Output...");
#ifdef FEAT_PORTMIDI
  qmenu_add_choice(qm, Main_menu_choose_portmidi_output, "MIDI Output...");
#endif
  qmenu_add_spacer(qm);
  qmenu_add_choice(qm, Main_menu_playback, "Clock & Timing...");
  qmenu_add_choice(qm, Main_menu_cosmetics, "Appearance...");
  qmenu_add_spacer(qm);
  qmenu_add_choice(qm, Main_menu_controls, "Controls...");
  qmenu_add_choice(qm, Main_menu_opers_guide, "Operators...");
  qmenu_add_choice(qm, Main_menu_about, "About ORCA...");
  qmenu_add_spacer(qm);
  qmenu_add_choice(qm, Main_menu_quit, "Quit");
  qmenu_push_to_nav(qm);
}

staticni void pop_qnav_if_main_menu(void) {
  Qblock *qb = qnav_top_block();
  if (qb && qb->tag == Qblock_type_qmenu &&
      qmenu_id(qmenu_of(qb)) == Main_menu_id)
    qnav_stack_pop();
}

static void push_confirm_new_file_menu(void) {
  Qmenu *qm = qmenu_create(Confirm_new_file_menu_id);
  qmenu_set_title(qm, "Are you sure?");
  qmenu_add_choice(qm, Confirm_new_file_reject_id, "Cancel");
  qmenu_add_choice(qm, Confirm_new_file_accept_id, "Create New File");
  qmenu_push_to_nav(qm);
}

static void push_autofit_menu(void) {
  Qmenu *qm = qmenu_create(Autofit_menu_id);
  qmenu_set_title(qm, "Auto-fit Grid");
  qmenu_add_choice(qm, Autofit_nicely_id, "Nicely");
  qmenu_add_choice(qm, Autofit_tightly_id, "Tightly");
  qmenu_push_to_nav(qm);
}

enum {
  Cosmetics_soft_margins_id = 1,
  Cosmetics_grid_dots_id,
  Cosmetics_grid_rulers_id,
};
static void push_cosmetics_menu(void) {
  Qmenu *qm = qmenu_create(Cosmetics_menu_id);
  qmenu_set_title(qm, "Appearance");
  qmenu_add_choice(qm, Cosmetics_soft_margins_id, "Margins...");
  qmenu_add_choice(qm, Cosmetics_grid_dots_id, "Grid dots...");
  qmenu_add_choice(qm, Cosmetics_grid_rulers_id, "Grid rulers...");
  qmenu_push_to_nav(qm);
}
static void push_soft_margins_form(int init_y, int init_x) {
  Qform *qf = qform_create(Set_soft_margins_form_id);
  char buff[128];
  int snres = snprintf(buff, sizeof buff, "%dx%d", init_x, init_y);
  char const *inistr = snres > 0 && (Usz)snres < sizeof buff ? buff : "2x1";
  qform_set_title(qf, "Set Margins");
  qform_add_text_line(qf, Single_form_item_id, inistr);
  qform_push_to_nav(qf);
}
static void push_plainorfancy_menu(int menu_id, char const *title,
                                   bool initial_fancy) {
  Qmenu *qm = qmenu_create(menu_id);
  qmenu_set_title(qm, title);
  qmenu_add_printf(qm, 1, "(%c) Fancy", initial_fancy ? '*' : ' ');
  qmenu_add_printf(qm, 2, "(%c) Plain", !initial_fancy ? '*' : ' ');
  if (!initial_fancy)
    qmenu_set_current_item(qm, 2);
  qmenu_push_to_nav(qm);
}
enum {
  Osc_menu_output_enabledisable = 1,
  Osc_menu_output_address,
  Osc_menu_output_port,
};
static void push_osc_menu(bool output_enabled) {
  Qmenu *qm = qmenu_create(Osc_menu_id);
  qmenu_set_title(qm, "OSC Output");
  qmenu_add_printf(qm, Osc_menu_output_enabledisable, "[%c] OSC Output Enabled",
                   output_enabled ? '*' : ' ');
  qmenu_add_choice(qm, Osc_menu_output_address, "OSC Output Address...");
  qmenu_add_choice(qm, Osc_menu_output_port, "OSC Output Port...");
  qmenu_push_to_nav(qm);
}
static void push_osc_output_address_form(char const *initial) {
  Qform *qf = qform_create(Osc_output_address_form_id);
  qform_set_title(qf, "Set OSC Output Address");
  qform_add_text_line(qf, Single_form_item_id, initial);
  qform_push_to_nav(qf);
}
static void push_osc_output_port_form(char const *initial) {
  Qform *qf = qform_create(Osc_output_port_form_id);
  qform_set_title(qf, "Set OSC Output Port");
  qform_add_text_line(qf, Single_form_item_id, initial);
  qform_push_to_nav(qf);
}
enum {
  Playback_menu_midi_bclock = 1,
};
static void push_playback_menu(bool midi_bclock_enabled) {
  Qmenu *qm = qmenu_create(Playback_menu_id);
  qmenu_set_title(qm, "Clock & Timing");
  qmenu_add_printf(qm, Playback_menu_midi_bclock, "[%c] Send MIDI Beat Clock",
                   midi_bclock_enabled ? '*' : ' ');
  qmenu_push_to_nav(qm);
}
static void push_about_msg(void) {
  // clang-format off
  static char const* logo[] = {
  "lqqqk|lqqqk|lqqqk|lqqqk",
  "x   x|x   j|x    |lqqqu",
  "mqqqj|m    |mqqqj|m   j",
  };
  static char const* footer =
  "Live Programming Environment";
  // clang-format on
  int rows = (int)ORCA_ARRAY_COUNTOF(logo);
  int cols = (int)strlen(logo[0]);
  int hpad = 5, tpad = 2, bpad = 2;
  int sep = 1;
  int footer_len = (int)strlen(footer);
  int width = footer_len;
  if (cols > width)
    width = cols;
  width += hpad * 2;
  int logo_left_pad = (width - cols) / 2;
  int footer_left_pad = (width - footer_len) / 2;
  Qmsg *qm = qmsg_push(tpad + rows + sep + 1 + bpad, width);
  WINDOW *w = qmsg_window(qm);
  for (int row = 0; row < rows; ++row) {
    wmove(w, row + tpad, logo_left_pad);
    wattrset(w, A_BOLD);
    for (int col = 0; col < cols; ++col) {
      char c = logo[row][col];
      chtype ch;
      if (c == ' ')
        ch = (chtype)' ';
      else if (c == '|')
        ch = ACS_VLINE | (chtype)fg_bg(C_black, C_natural) | A_BOLD;
      else
        ch = NCURSES_ACS(c) | A_BOLD;
      waddch(w, ch);
    }
  }
  wattrset(w, A_DIM);
  wmove(w, tpad + rows + sep, footer_left_pad);
  waddstr(w, footer);
}
static void push_controls_msg(void) {
  struct Ctrl_item {
    char const *input;
    char const *desc;
  };
  static struct Ctrl_item items[] = {
      {"Ctrl+Q", "Quit"},
      {"Arrow Keys", "Move Cursor"},
      {"Ctrl+D or F1", "Open Main Menu"},
      {"0-9, A-Z, a-z,", "Insert Character"},
      {"! : % / = # *", NULL},
      {"Spacebar", "Play/Pause"},
      {"Ctrl+Z or Ctrl+U", "Undo"},
      {"Ctrl+X", "Cut"},
      {"Ctrl+C", "Copy"},
      {"Ctrl+V", "Paste"},
      {"Ctrl+S", "Save"},
      {"Ctrl+F", "Frame Step Forward"},
      {"Ctrl+R", "Reset Frame Number"},
      {"Ctrl+I or Insert", "Append/Overwrite Mode"},
      // {"/", "Key Trigger Mode"},
      {"' (quote)", "Rectangle Selection Mode"},
      {"Shift+Arrow Keys", "Adjust Rectangle Selection"},
      {"Alt+Arrow Keys", "Slide Selection"},
      {"` (grave) or ~", "Slide Selection Mode"},
      {"Escape", "Return to Normal Mode or Deselect"},
      {"( ) _ + [ ] { }", "Adjust Grid Size and Rulers"},
      {"< and >", "Adjust BPM"},
      {"?", "Controls (this message)"},
  };
  int w_input = 0;
  int w_desc = 0;
  for (Usz i = 0; i < ORCA_ARRAY_COUNTOF(items); ++i) {
    // use wcswidth instead of strlen if you need wide char support. but note
    // that won't be useful for UTF-8 or unicode chars in higher plane (emoji,
    // complex zwj, etc.)
    if (items[i].input) {
      int wl = (int)strlen(items[i].input);
      if (wl > w_input)
        w_input = wl;
    }
    if (items[i].desc) {
      int wr = (int)strlen(items[i].desc);
      if (wr > w_desc)
        w_desc = wr;
    }
  }
  int mid_pad = 2;
  int total_width = 1 + w_input + mid_pad + w_desc + 1;
  Qmsg *qm = qmsg_push(ORCA_ARRAY_COUNTOF(items), total_width);
  qmsg_set_title(qm, "Controls");
  WINDOW *w = qmsg_window(qm);
  for (int i = 0; i < (int)ORCA_ARRAY_COUNTOF(items); ++i) {
    if (items[i].input) {
      wmove(w, i, 1 + w_input - (int)strlen(items[i].input));
      waddstr(w, items[i].input);
    }
    if (items[i].desc) {
      wmove(w, i, 1 + w_input + mid_pad);
      waddstr(w, items[i].desc);
    }
  }
}
static void push_opers_guide_msg(void) {
  struct Guide_item {
    char glyph;
    char const *name;
    char const *desc;
  };
  static struct Guide_item items[] = {
      {'A', "add", "Outputs sum of inputs."},
      {'B', "subtract", "Outputs difference of inputs."},
      {'C', "clock", "Outputs modulo of frame."},
      {'D', "delay", "Bangs on modulo of frame."},
      {'E', "east", "Moves eastward, or bangs."},
      {'F', "if", "Bangs if inputs are equal."},
      {'G', "generator", "Writes operands with offset."},
      {'H', "halt", "Halts southward operand."},
      {'I', "increment", "Increments southward operand."},
      {'J', "jumper", "Outputs northward operand."},
      {'K', "konkat", "Reads multiple variables."},
      {'L', "lesser", "Outputs smallest input."},
      {'M', "multiply", "Outputs product of inputs."},
      {'N', "north", "Moves Northward, or bangs."},
      {'O', "read", "Reads operand with offset."},
      {'P', "push", "Writes eastward operand."},
      {'Q', "query", "Reads operands with offset."},
      {'R', "random", "Outputs random value."},
      {'S', "south", "Moves southward, or bangs."},
      {'T', "track", "Reads eastward operand."},
      {'U', "uclid", "Bangs on Euclidean rhythm."},
      {'V', "variable", "Reads and writes variable."},
      {'W', "west", "Moves westward, or bangs."},
      {'X', "write", "Writes operand with offset."},
      {'Y', "jymper", "Outputs westward operand."},
      {'Z', "lerp", "Transitions operand to target."},
      {'*', "bang", "Bangs neighboring operands."},
      {'#', "comment", "Halts line."},
      // {'*', "self", "Sends ORCA command."},
      {':', "midi", "Sends MIDI note."},
      {'!', "cc", "Sends MIDI control change."},
      {'?', "pb", "Sends MIDI pitch bend."},
      // {'%', "mono", "Sends MIDI monophonic note."},
      {'=', "osc", "Sends OSC message."},
      {';', "udp", "Sends UDP message."},
  };
  int w_desc = 0;
  for (Usz i = 0; i < ORCA_ARRAY_COUNTOF(items); ++i) {
    if (items[i].desc) {
      int wr = (int)strlen(items[i].desc);
      if (wr > w_desc)
        w_desc = wr;
    }
  }
  int left_pad = 1, mid_pad = 1, right_pad = 1;
  int total_width = left_pad + 1 + mid_pad + w_desc + right_pad;
  Qmsg *qm = qmsg_push(ORCA_ARRAY_COUNTOF(items), total_width);
  qmsg_set_title(qm, "Operators");
  WINDOW *w = qmsg_window(qm);
  for (int i = 0; i < (int)ORCA_ARRAY_COUNTOF(items); ++i) {
    wmove(w, i, left_pad);
    waddch(w, (chtype)items[i].glyph | A_bold);
    wmove(w, i, left_pad + 1 + mid_pad);
    wattrset(w, A_normal);
    waddstr(w, items[i].desc);
  }
}
static void push_open_form(char const *initial) {
  Qform *qf = qform_create(Open_form_id);
  qform_set_title(qf, "Open");
  qform_add_text_line(qf, Single_form_item_id, initial);
  qform_push_to_nav(qf);
}
staticni bool try_save_with_msg(Field *field, oso const *str) {
  if (!osolen(str))
    return false;
  bool ok = hacky_try_save(field, osoc(str));
  if (ok) {
    Qmsg *qm = qmsg_printf_push(NULL, "Saved to:\n%s", osoc(str));
    qmsg_set_dismiss_mode(qm, Qmsg_dismiss_mode_passthrough);
  } else {
    qmsg_printf_push("Error Saving File", "Unable to save file to:\n%s",
                     osoc(str));
  }
  return ok;
}
static void push_save_as_form(char const *initial) {
  Qform *qf = qform_create(Save_as_form_id);
  qform_set_title(qf, "Save As");
  qform_add_text_line(qf, Single_form_item_id, initial);
  qform_push_to_nav(qf);
}
static void push_set_tempo_form(Usz initial) {
  Qform *qf = qform_create(Set_tempo_form_id);
  char buff[64];
  int snres = snprintf(buff, sizeof buff, "%zu", initial);
  char const *inistr = snres > 0 && (Usz)snres < sizeof buff ? buff : "120";
  qform_set_title(qf, "Set BPM");
  qform_add_text_line(qf, Single_form_item_id, inistr);
  qform_push_to_nav(qf);
}
static void push_set_grid_dims_form(Usz init_height, Usz init_width) {
  Qform *qf = qform_create(Set_grid_dims_form_id);
  char buff[128];
  int snres = snprintf(buff, sizeof buff, "%zux%zu", init_width, init_height);
  char const *inistr = snres > 0 && (Usz)snres < sizeof buff ? buff : "57x25";
  qform_set_title(qf, "Set Grid Size");
  qform_add_text_line(qf, Single_form_item_id, inistr);
  qform_push_to_nav(qf);
}

#ifdef FEAT_PORTMIDI
staticni void push_portmidi_output_device_menu(Midi_mode const *midi_mode) {
  Qmenu *qm = qmenu_create(Portmidi_output_device_menu_id);
  qmenu_set_title(qm, "PortMidi Device Selection");
  PmError e = portmidi_init_if_necessary();
  if (e) {
    qmenu_destroy(qm);
    qmsg_printf_push("PortMidi Error",
                     "PortMidi error during initialization:\n%s",
                     Pm_GetErrorText(e));
    return;
  }
  int num = Pm_CountDevices();
  int output_devices = 0;
  int cur_dev_id = 0;
  bool has_cur_dev_id = false;
  if (midi_mode->any.type == Midi_mode_type_portmidi) {
    cur_dev_id = midi_mode->portmidi.device_id;
    has_cur_dev_id = true;
  }
  for (int i = 0; i < num; ++i) {
    PmDeviceInfo const *info = Pm_GetDeviceInfo(i);
    if (!info || !info->output)
      continue;
    bool is_cur_dev_id = has_cur_dev_id && cur_dev_id == i;
    qmenu_add_printf(qm, i, "(%c) #%d - %s", is_cur_dev_id ? '*' : ' ', i,
                     info->name);
    ++output_devices;
  }
  if (output_devices == 0) {
    qmenu_destroy(qm);
    qmsg_printf_push("No PortMidi Devices",
                     "No PortMidi output devices found.");
    return;
  }
  if (has_cur_dev_id) {
    qmenu_set_current_item(qm, cur_dev_id);
  }
  qmenu_push_to_nav(qm);
}
#endif

staticni oso *get_nonempty_singular_form_text(Qform *qf) {
  oso *s = NULL;
  if (qform_get_text_line(qf, Single_form_item_id, &s) && osolen(s) > 0)
    return s;
  osofree(s);
  return NULL;
}

staticni bool read_int(char const *str, int *out) {
  int a;
  int res = sscanf(str, "%d", &a);
  if (res != 1)
    return false;
  *out = a;
  return true;
}

// Reads something like '5x3' or '5'. Writes the same value to both outputs if
// only one is specified. Returns false on error.
staticni bool read_nxn_or_n(char const *str, int *out_a, int *out_b) {
  int a, b;
  int res = sscanf(str, "%dx%d", &a, &b);
  if (res == EOF)
    return false;
  if (res == 1) {
    *out_a = a;
    *out_b = a;
    return true;
  }
  if (res == 2) {
    *out_a = a;
    *out_b = b;
    return true;
  }
  return false;
}

typedef enum {
  Brackpaste_seq_none = 0,
  Brackpaste_seq_begin,
  Brackpaste_seq_end,
} Brackpaste_seq;

// Try to getch to complete the sequence of a start or end of brackpet paste
// escape sequence. If it doesn't turn out to be one, unwind it back into the
// event queue with ungetch. Yeah this is golfed let me have fun.
staticni Brackpaste_seq brackpaste_seq_getungetch(WINDOW *win) {
  int chs[5], n = 0, begorend; // clang-format off
  if ((chs[n++] = wgetch(win)) != '[') goto unwind;
  if ((chs[n++] = wgetch(win)) != '2') goto unwind;
  if ((chs[n++] = wgetch(win)) != '0') goto unwind;
  chs[n++] = begorend = wgetch(win);
  if (begorend != '0' && begorend != '1') goto unwind;
  if ((chs[n++] = wgetch(win)) != '~') goto unwind;
  return begorend == '0' ? Brackpaste_seq_begin : Brackpaste_seq_end;
unwind:
  while (n > 0) ungetch(chs[--n]);
  return Brackpaste_seq_none; // clang-format on
}

staticni void try_send_to_gui_clipboard(Ged const *a,
                                        bool *io_use_gui_clipboard) {
  if (!*io_use_gui_clipboard)
    return;
#if 0 // If we want to use grid directly
  Usz curs_y, curs_x, curs_h, curs_w;
  if (!ged_try_selection_clipped_to_field(a, &curs_y, &curs_x, &curs_h,
                                          &curs_w))
    return;
  Cboard_error cberr =
      cboard_copy(a->clipboard_field.buffer, a->clipboard_field.height,
                  a->clipboard_field.width, curs_y, curs_x, curs_h, curs_w);
#endif
  Usz cb_h = a->clipboard_field.height, cb_w = a->clipboard_field.width;
  if (cb_h < 1 || cb_w < 1)
    return;
  Cboard_error cberr =
      cboard_copy(a->clipboard_field.buffer, cb_h, cb_w, 0, 0, cb_h, cb_w);
  if (cberr)
    *io_use_gui_clipboard = false;
}

static char const *const conf_file_name = "orca.conf";
#define CONFOPT_STRING(x) #x,
#define CONFOPT_ENUM(x) Confopt_##x,
#define CONFOPTS(_)                                                            \
  _(portmidi_output_device)                                                    \
  _(osc_output_address)                                                        \
  _(osc_output_port)                                                           \
  _(osc_output_enabled)                                                        \
  _(midi_beat_clock)                                                           \
  _(margins)                                                                   \
  _(grid_dot_type)                                                             \
  _(grid_ruler_type)
char const *const confopts[] = {CONFOPTS(CONFOPT_STRING)};
enum { Confoptslen = ORCA_ARRAY_COUNTOF(confopts) };
enum { CONFOPTS(CONFOPT_ENUM) };
#undef CONFOPTS
#undef CONFOPT_STRING
#undef CONFOPT_ENUM

// Use this to create a bitflag out of a Confopt_. These flags are used to
// indicate that a setting has been touched by the user. In other words, these
// flags are used to check whether or not a particular setting should be
// written back to the conf file during a conf file save.
#define TOUCHFLAG(_confopt) (1u << (Usz)_confopt)

char const *const prefval_plain = "plain";
char const *const prefval_fancy = "fancy";

staticni bool plainorfancy(char const *val, bool *out) {
  if (strcmp(val, prefval_plain) == 0) {
    *out = false;
    return true;
  }
  if (strcmp(val, prefval_fancy) == 0) {
    *out = true;
    return true;
  }
  return false;
}

staticni bool conf_read_boolish(char const *val, bool *out) {
  static char const *const trues[] = {"1", "true", "yes"};
  static char const *const falses[] = {"0", "false", "no"};
  for (Usz i = 0; i < ORCA_ARRAY_COUNTOF(trues); i++) {
    if (strcmp(val, trues[i]) != 0)
      continue;
    *out = true;
    return true;
  }
  for (Usz i = 0; i < ORCA_ARRAY_COUNTOF(falses); i++) {
    if (strcmp(val, falses[i]) != 0)
      continue;
    *out = false;
    return true;
  }
  return false;
}

typedef struct {
  Ged ged;
  oso *file_name;
  oso *osc_address, *osc_port, *osc_midi_bidule_path;
  int undo_history_limit;
  int softmargin_y, softmargin_x;
  int hardmargin_y, hardmargin_x;
  U32 prefs_touched;
  bool use_gui_cboard; // not bitfields due to taking address of
  bool strict_timing;
  bool osc_output_enabled;
  bool fancy_grid_dots, fancy_grid_rulers;
} Tui;

ORCA_OK_IF_UNUSED staticni void print_loading_message(char const *s) {
  Usz len = strlen(s);
  if (len > INT_MAX)
    return;
  int h, w;
  getmaxyx(stdscr, h, w);
  int y = h / 2;
  int x = (int)len < w ? (w - (int)len) / 2 : 0;
  werase(stdscr);
  wmove(stdscr, y, x);
  waddstr(stdscr, s);
  refresh();
}

staticni void tui_load_conf(Tui *t) {
  oso *portmidi_output_device = NULL, *osc_output_address = NULL,
      *osc_output_port = NULL;
  U32 touched = 0;
  Ezconf_r ez;
  for (ezconf_r_start(&ez, conf_file_name);
       ezconf_r_step(&ez, confopts, Confoptslen);) {
    switch (ez.index) {
    case Confopt_portmidi_output_device:
      osoput(&portmidi_output_device, ez.value);
      break;
    case Confopt_osc_output_address: {
      // Don't actually allocate heap string if string is empty
      Usz len = strlen(ez.value);
      if (len > 0)
        osoputlen(&osc_output_address, ez.value, len);
      touched |= TOUCHFLAG(Confopt_osc_output_address);
      break;
    }
    case Confopt_osc_output_port: {
      osoput(&osc_output_port, ez.value);
      touched |= TOUCHFLAG(Confopt_osc_output_port);
      break;
    }
    case Confopt_osc_output_enabled: {
      bool enabled;
      if (conf_read_boolish(ez.value, &enabled)) {
        t->osc_output_enabled = enabled;
        touched |= TOUCHFLAG(Confopt_osc_output_enabled);
      }
      break;
    }
    case Confopt_midi_beat_clock: {
      bool enabled;
      if (conf_read_boolish(ez.value, &enabled)) {
        t->ged.midi_bclock = enabled;
        touched |= TOUCHFLAG(Confopt_midi_beat_clock);
      }
      break;
    }
    case Confopt_margins: {
      int softmargin_y, softmargin_x;
      if (read_nxn_or_n(ez.value, &softmargin_x, &softmargin_y) &&
          softmargin_y >= 0 && softmargin_x >= 0) {
        t->softmargin_y = softmargin_y;
        t->softmargin_x = softmargin_x;
        touched |= TOUCHFLAG(Confopt_margins);
      }
      break;
    }
    case Confopt_grid_dot_type: {
      bool fancy;
      if (plainorfancy(ez.value, &fancy)) {
        t->fancy_grid_dots = fancy;
        touched |= TOUCHFLAG(Confopt_grid_dot_type);
      }
      break;
    }
    case Confopt_grid_ruler_type: {
      bool fancy;
      if (plainorfancy(ez.value, &fancy)) {
        t->fancy_grid_rulers = fancy;
        touched |= TOUCHFLAG(Confopt_grid_ruler_type);
      }
      break;
    }
    }
  }

  if (touched & TOUCHFLAG(Confopt_osc_output_address)) {
    ososwap(&t->osc_address, &osc_output_address);
  } else {
    // leave null
  }
  if (touched & TOUCHFLAG(Confopt_osc_output_port)) {
    ososwap(&t->osc_port, &osc_output_port);
  } else {
    osoput(&t->osc_port, "49162");
  }

#ifdef FEAT_PORTMIDI
  if (t->ged.midi_mode.any.type == Midi_mode_type_null &&
      osolen(portmidi_output_device)) {
    // PortMidi can be hilariously slow to initialize. Since it will be
    // initialized automatically if the user has a prefs entry for PortMidi
    // devices, we should show a message to the user letting them know why
    // orca is locked up/frozen. (When it's done via menu action, that's
    // fine, since they can figure out why.)
    print_loading_message("Waiting on PortMidi...");
    PmError pmerr;
    PmDeviceID devid;
    if (portmidi_find_device_id_by_name(osoc(portmidi_output_device),
                                        osolen(portmidi_output_device), &pmerr,
                                        &devid)) {
      midi_mode_deinit(&t->ged.midi_mode);
      pmerr = midi_mode_init_portmidi(&t->ged.midi_mode, devid);
      if (pmerr) {
        // todo stuff
      }
    }
  }
#endif
  t->prefs_touched |= touched;
  osofree(portmidi_output_device);
  osofree(osc_output_address);
  osofree(osc_output_port);
}

staticni void tui_save_prefs(Tui *t) {
  Ezconf_opt optsbuff[Confoptslen];
  Ezconf_w ez;
  ezconf_w_start(&ez, optsbuff, ORCA_ARRAY_COUNTOF(optsbuff), conf_file_name);
  oso *midi_output_device_name = NULL;
  switch (t->ged.midi_mode.any.type) {
  case Midi_mode_type_null:
    break;
  case Midi_mode_type_osc_bidule:
    // TODO
    break;
#ifdef FEAT_PORTMIDI
  case Midi_mode_type_portmidi: {
    PmError pmerror;
    if (!portmidi_find_name_of_device_id(t->ged.midi_mode.portmidi.device_id,
                                         &pmerror, &midi_output_device_name) ||
        osolen(midi_output_device_name) < 1) {
      osowipe(&midi_output_device_name);
      break;
    }
    ezconf_w_addopt(&ez, confopts[Confopt_portmidi_output_device],
                    Confopt_portmidi_output_device);
    break;
  }
#endif
  }
  // Add all conf items touched by user that we want to write to config file.
  // "Touched" items include conf items that were present on disk when we first
  // loaded the config file, plus the items that the user has modified by
  // interacting with the menus.
  for (int i = 0; i < Confoptslen; i++) {
    if (i == Confopt_portmidi_output_device)
      // This has its own special logic
      continue;
    if (t->prefs_touched & TOUCHFLAG(i))
      ezconf_w_addopt(&ez, confopts[i], i);
  }
  while (ezconf_w_step(&ez)) {
    switch (ez.optid) {
    case Confopt_osc_output_address:
      // Fine to not write anything here
      if (osolen(t->osc_address))
        fputs(osoc(t->osc_address), ez.file);
      break;
    case Confopt_osc_output_port:
      if (osolen(t->osc_port))
        fputs(osoc(t->osc_port), ez.file);
      break;
    case Confopt_osc_output_enabled:
      fputc(t->osc_output_enabled ? '1' : '0', ez.file);
      break;
#ifdef FEAT_PORTMIDI
    case Confopt_portmidi_output_device:
      fputs(osoc(midi_output_device_name), ez.file);
      break;
#endif
    case Confopt_midi_beat_clock:
      fputc(t->ged.midi_bclock ? '1' : '0', ez.file);
      break;
    case Confopt_margins:
      fprintf(ez.file, "%dx%d", t->softmargin_x, t->softmargin_y);
      break;
    case Confopt_grid_dot_type:
      fputs(t->fancy_grid_dots ? prefval_fancy : prefval_plain, ez.file);
      break;
    case Confopt_grid_ruler_type:
      fputs(t->fancy_grid_rulers ? prefval_fancy : prefval_plain, ez.file);
      break;
    }
  }
  osofree(midi_output_device_name);
  if (ez.error) {
    char const *msg = ezconf_w_errorstring(ez.error);
    qmsg_printf_push("Config Error",
                     "Error when writing configuration file:\n%s", msg);
  }
}

staticni bool tui_suggest_nice_grid_size(Tui *t, int win_h, int win_w,
                                         Usz *out_grid_h, Usz *out_grid_w) {
  int softmargin_y = t->softmargin_y, softmargin_x = t->softmargin_x;
  int ruler_spacing_y = (int)t->ged.ruler_spacing_y,
      ruler_spacing_x = (int)t->ged.ruler_spacing_x;
  if (win_h < 1 || win_w < 1 || softmargin_y < 0 || softmargin_x < 0 ||
      ruler_spacing_y < 1 || ruler_spacing_x < 1)
    return false;
  // TODO overflow checks
  int h = (win_h - softmargin_y - Hud_height - 1) / ruler_spacing_y;
  h *= ruler_spacing_y;
  int w = (win_w - softmargin_x * 2 - 1) / ruler_spacing_x;
  w *= ruler_spacing_x;
  if (h < ruler_spacing_y)
    h = ruler_spacing_y;
  if (w < ruler_spacing_x)
    w = ruler_spacing_x;
  h++;
  w++;
  if (h >= ORCA_Y_MAX || w >= ORCA_X_MAX)
    return false;
  *out_grid_h = (Usz)h;
  *out_grid_w = (Usz)w;
  return true;
}

staticni bool tui_suggest_tight_grid_size(Tui *t, int win_h, int win_w,
                                          Usz *out_grid_h, Usz *out_grid_w) {
  int softmargin_y = t->softmargin_y, softmargin_x = t->softmargin_x;
  if (win_h < 1 || win_w < 1 || softmargin_y < 0 || softmargin_x < 0)
    return false;
  // TODO overflow checks
  int h = win_h - softmargin_y - Hud_height;
  int w = win_w - softmargin_x * 2;
  if (h < 1 || w < 1 || h >= ORCA_Y_MAX || w >= ORCA_X_MAX)
    return false;
  *out_grid_h = (Usz)h;
  *out_grid_w = (Usz)w;
  return true;
}

staticni void plainorfancy_menu_was_picked(Tui *t, int picked_id,
                                           bool *p_is_fancy,
                                           U32 pref_touch_flag) {
  bool is_fancy = picked_id == 1; // 1 -> fancy, 2 -> plain
  qnav_stack_pop();
  // ^- doesn't actually matter when we do this, with our current code
  if (is_fancy == *p_is_fancy)
    return;
  *p_is_fancy = is_fancy;
  t->prefs_touched |= pref_touch_flag;
  tui_save_prefs(t);
  t->ged.is_draw_dirty = true;
}

staticni bool tui_restart_osc_udp_if_enabled_diderror(Tui *t) {
  bool error = false;
  if (t->osc_output_enabled && t->osc_port) {
    error = !ged_set_osc_udp(&t->ged, osoc(t->osc_address) /* null ok here */,
                             osoc(t->osc_port));
  } else {
    ged_clear_osc_udp(&t->ged);
  }
  return error;
}
staticni void tui_restart_osc_udp_showerror(void) {
  qmsg_printf_push("OSC Networking Error", "Failed to set up OSC networking");
}
staticni void tui_restart_osc_udp_if_enabled(Tui *t) {
  bool old_inuse = ged_is_using_osc_udp(&t->ged);
  bool did_error = tui_restart_osc_udp_if_enabled_diderror(t);
  bool new_inuse = ged_is_using_osc_udp(&t->ged);
  if (old_inuse != new_inuse) {
    Qblock *qb = qnav_top_block();
    if (qb && qb->tag == Qblock_type_qmenu &&
        qmenu_id(qmenu_of(qb)) == Osc_menu_id) {
      int itemid = qmenu_current_item(qmenu_of(qb));
      qnav_stack_pop();
      push_osc_menu(new_inuse);
      qmenu_set_current_item(qmenu_of(qnav_top_block()), itemid);
    }
  }
  if (did_error)
    tui_restart_osc_udp_showerror();
}
staticni void tui_adjust_term_size(Tui *t, WINDOW **cont_window) {
  int term_h, term_w;
  getmaxyx(stdscr, term_h, term_w);
  assert(term_h >= 0 && term_w >= 0);
  int content_y = 0, content_x = 0;
  int content_h = term_h, content_w = term_w;
  if (t->hardmargin_y > 0 && term_h > t->hardmargin_y * 2 + 2) {
    content_y += t->hardmargin_y;
    content_h -= t->hardmargin_y * 2;
  }
  if (t->hardmargin_x > 0 && term_w > t->hardmargin_x * 2 + 2) {
    content_x += t->hardmargin_x;
    content_w -= t->hardmargin_x * 2;
  }
  bool remake_window = true;
  if (*cont_window) {
    int cwin_y, cwin_x, cwin_h, cwin_w;
    getbegyx(*cont_window, cwin_y, cwin_x);
    getmaxyx(*cont_window, cwin_h, cwin_w);
    remake_window = cwin_y != content_y || cwin_x != content_x ||
                    cwin_h != content_h || cwin_w != content_w;
  }
  if (remake_window) {
    if (*cont_window)
      delwin(*cont_window);
    wclear(stdscr);
    *cont_window = derwin(stdscr, content_h, content_w, content_y, content_x);
    t->ged.is_draw_dirty = true;
  }
  // OK to call this unconditionally -- deriving the sub-window areas is
  // more than a single comparison, and we don't want to split up or
  // duplicate the math and checks for it, so this routine will calculate
  // the stuff it needs to and then early-out if there's no further work.
  ged_set_window_size(&t->ged, content_h, content_w, t->softmargin_y,
                      t->softmargin_x);
}

static void tui_try_save(Tui *t) {
  if (osolen(t->file_name) > 0)
    try_save_with_msg(&t->ged.field, t->file_name);
  else
    push_save_as_form("");
}

typedef enum {
  Tui_menus_nothing = 0,
  Tui_menus_quit,
  Tui_menus_consumed_input,
} Tui_menus_result;

staticni Tui_menus_result tui_drive_menus(Tui *t, int key) {
  Qblock *qb = qnav_top_block();
  if (!qb)
    return Tui_menus_nothing;
  if (key == CTRL_PLUS('q'))
    return Tui_menus_quit;
  switch (qb->tag) {
  case Qblock_type_qmsg: {
    Qmsg *qm = qmsg_of(qb);
    Qmsg_action act;
    if (qmsg_drive(qm, key, &act)) {
      if (act.dismiss)
        qnav_stack_pop();
      if (act.passthrough)
        return Tui_menus_nothing;
    }
    break;
  }
  case Qblock_type_qmenu: {
    Qmenu *qm = qmenu_of(qb);
    Qmenu_action act;
    // special case for main menu: pressing the key to open it will close
    // it again.
    if (qmenu_id(qm) == Main_menu_id &&
        (key == CTRL_PLUS('d') || key == KEY_F(1))) {
      qnav_stack_pop();
      break;
    }
    if (!qmenu_drive(qm, key, &act))
      break;
    switch (act.any.type) {
    case Qmenu_action_type_canceled:
      qnav_stack_pop();
      break;
    case Qmenu_action_type_picked:
      switch (qmenu_id(qm)) {
      case Main_menu_id:
        switch (act.picked.id) {
        case Main_menu_quit:
          return Tui_menus_quit;
        case Main_menu_playback:
          push_playback_menu(t->ged.midi_bclock);
          break;
        case Main_menu_cosmetics:
          push_cosmetics_menu();
          break;
        case Main_menu_osc:
          push_osc_menu(ged_is_using_osc_udp(&t->ged));
          break;
        case Main_menu_controls:
          push_controls_msg();
          break;
        case Main_menu_opers_guide:
          push_opers_guide_msg();
          break;
        case Main_menu_about:
          push_about_msg();
          break;
        case Main_menu_new:
          push_confirm_new_file_menu();
          break;
        case Main_menu_open:
          push_open_form(osoc(t->file_name));
          break;
        case Main_menu_save:
          tui_try_save(t);
          break;
        case Main_menu_save_as:
          push_save_as_form(osoc(t->file_name));
          break;
        case Main_menu_set_tempo:
          push_set_tempo_form(t->ged.bpm);
          break;
        case Main_menu_set_grid_dims:
          push_set_grid_dims_form(t->ged.field.height, t->ged.field.width);
          break;
        case Main_menu_autofit_grid:
          push_autofit_menu();
          break;
#ifdef FEAT_PORTMIDI
        case Main_menu_choose_portmidi_output:
          push_portmidi_output_device_menu(&t->ged.midi_mode);
          break;
#endif
        }
        break;
      case Autofit_menu_id: {
        Usz new_field_h, new_field_w;
        bool did_get_ok_size = false;
        switch (act.picked.id) {
        case Autofit_nicely_id:
          did_get_ok_size = tui_suggest_nice_grid_size(
              t, t->ged.win_h, t->ged.win_w, &new_field_h, &new_field_w);
          break;
        case Autofit_tightly_id:
          did_get_ok_size = tui_suggest_tight_grid_size(
              t, t->ged.win_h, t->ged.win_w, &new_field_h, &new_field_w);
          break;
        }
        if (did_get_ok_size) {
          ged_resize_grid(&t->ged.field, &t->ged.mbuf_r, new_field_h,
                          new_field_w, t->ged.tick_num, &t->ged.scratch_field,
                          &t->ged.undo_hist, &t->ged.ged_cursor);
          ged_update_internal_geometry(&t->ged);
          t->ged.needs_remarking = true;
          t->ged.is_draw_dirty = true;
          ged_make_cursor_visible(&t->ged);
        }
        qnav_stack_pop();
        pop_qnav_if_main_menu();
        break;
      }
      case Confirm_new_file_menu_id:
        switch (act.picked.id) {
        case Confirm_new_file_reject_id:
          qnav_stack_pop();
          break;
        case Confirm_new_file_accept_id: {
          Usz new_field_h, new_field_w;
          if (tui_suggest_nice_grid_size(t, t->ged.win_h, t->ged.win_w,
                                         &new_field_h, &new_field_w)) {
            undo_history_push(&t->ged.undo_hist, &t->ged.field,
                              t->ged.tick_num);
            field_resize_raw(&t->ged.field, new_field_h, new_field_w);
            memset(t->ged.field.buffer, '.',
                   new_field_h * new_field_w * sizeof(Glyph));
            ged_cursor_confine(&t->ged.ged_cursor, new_field_h, new_field_w);
            mbuf_reusable_ensure_size(&t->ged.mbuf_r, new_field_h, new_field_w);
            ged_update_internal_geometry(&t->ged);
            ged_make_cursor_visible(&t->ged);
            t->ged.needs_remarking = true;
            t->ged.is_draw_dirty = true;
            osoclear(&t->file_name);
            qnav_stack_pop();
            pop_qnav_if_main_menu();
          }
          break;
        }
        }
        break;
      case Cosmetics_menu_id:
        switch (act.picked.id) {
        case Cosmetics_soft_margins_id:
          push_soft_margins_form(t->softmargin_y, t->softmargin_x);
          break;
        case Cosmetics_grid_dots_id:
          push_plainorfancy_menu(Set_fancy_grid_dots_menu_id, "Grid Dots",
                                 t->fancy_grid_dots);
          break;
        case Cosmetics_grid_rulers_id:
          push_plainorfancy_menu(Set_fancy_grid_rulers_menu_id, "Grid Rulers",
                                 t->fancy_grid_rulers);
          break;
        }
        break;
      case Playback_menu_id:
        switch (act.picked.id) {
        case Playback_menu_midi_bclock: {
          bool new_enabled = !t->ged.midi_bclock;
          t->ged.midi_bclock = new_enabled;
          if (t->ged.is_playing) {
            int msgbyte = new_enabled ? 0xFA /* start */ : 0xFC /* stop */;
            send_midi_byte(t->ged.oosc_dev, &t->ged.midi_mode, msgbyte);
            // TODO timing judder will be experienced here, because the
            // deadline calculation conditions will have been changed by
            // toggling the midi_bclock flag. We would have to transfer the
            // current remaining time from the reference clock point into the
            // accum time, and mutiply or divide it.
          }
          t->prefs_touched |= TOUCHFLAG(Confopt_midi_beat_clock);
          qnav_stack_pop();
          push_playback_menu(new_enabled);
          tui_save_prefs(t);
          break;
        }
        }
        break;
      case Set_fancy_grid_dots_menu_id:
        plainorfancy_menu_was_picked(t, act.picked.id, &t->fancy_grid_dots,
                                     TOUCHFLAG(Confopt_grid_dot_type));
        break;
      case Set_fancy_grid_rulers_menu_id:
        plainorfancy_menu_was_picked(t, act.picked.id, &t->fancy_grid_rulers,
                                     TOUCHFLAG(Confopt_grid_ruler_type));
        break;
      case Osc_menu_id:
        switch (act.picked.id) {
        case Osc_menu_output_enabledisable: {
          qnav_stack_pop();
          t->osc_output_enabled = !ged_is_using_osc_udp(&t->ged);
          // Funny dance to keep the qnav stack in good order
          bool diderror = tui_restart_osc_udp_if_enabled_diderror(t);
          push_osc_menu(ged_is_using_osc_udp(&t->ged));
          if (diderror) {
            t->osc_output_enabled = false;
            tui_restart_osc_udp_showerror();
          }
          t->prefs_touched |= TOUCHFLAG(Confopt_osc_output_enabled);
          tui_save_prefs(t);
          break;
        }
        case Osc_menu_output_address:
          push_osc_output_address_form(osoc(t->osc_address) /* null ok */);
          break;
        case Osc_menu_output_port:
          push_osc_output_port_form(osoc(t->osc_port) /* null ok */);
          break;
        }
        break;
#ifdef FEAT_PORTMIDI
      case Portmidi_output_device_menu_id: {
        ged_stop_all_sustained_notes(&t->ged);
        midi_mode_deinit(&t->ged.midi_mode);
        PmError pme = midi_mode_init_portmidi(&t->ged.midi_mode, act.picked.id);
        qnav_stack_pop();
        if (pme) {
          qmsg_printf_push("PortMidi Error",
                           "Error setting PortMidi output device:\n%s",
                           Pm_GetErrorText(pme));
        } else {
          tui_save_prefs(t);
        }
        break;
      }
#endif
      }
      break;
    }
    break;
  }
  case Qblock_type_qform: {
    Qform *qf = qform_of(qb);
    Qform_action act;
    if (qform_drive(qf, key, &act)) {
      switch (act.any.type) {
      case Qform_action_type_canceled:
        qnav_stack_pop();
        break;
      case Qform_action_type_submitted: {
        switch (qform_id(qf)) {
        case Open_form_id: {
          oso *temp_name = get_nonempty_singular_form_text(qf);
          if (!temp_name)
            break;
          expand_home_tilde(&temp_name);
          if (!temp_name)
            break;
          bool added_hist = undo_history_push(&t->ged.undo_hist, &t->ged.field,
                                              t->ged.tick_num);
          Field_load_error fle =
              field_load_file(osoc(temp_name), &t->ged.field);
          if (fle == Field_load_error_ok) {
            qnav_stack_pop();
            osoputoso(&t->file_name, temp_name);
            mbuf_reusable_ensure_size(&t->ged.mbuf_r, t->ged.field.height,
                                      t->ged.field.width);
            ged_cursor_confine(&t->ged.ged_cursor, t->ged.field.height,
                               t->ged.field.width);
            ged_update_internal_geometry(&t->ged);
            ged_make_cursor_visible(&t->ged);
            t->ged.needs_remarking = true;
            t->ged.is_draw_dirty = true;
            pop_qnav_if_main_menu();
          } else {
            if (added_hist)
              undo_history_pop(&t->ged.undo_hist, &t->ged.field,
                               &t->ged.tick_num);
            qmsg_printf_push("Error Loading File", "%s:\n%s", osoc(temp_name),
                             field_load_error_string(fle));
          }
          osofree(temp_name);
          break;
        }
        case Save_as_form_id: {
          oso *temp_name = get_nonempty_singular_form_text(qf);
          if (!temp_name)
            break;
          qnav_stack_pop();
          bool saved_ok = try_save_with_msg(&t->ged.field, temp_name);
          if (saved_ok)
            osoputoso(&t->file_name, temp_name);
          osofree(temp_name);
          break;
        }
        case Set_tempo_form_id: {
          oso *tmpstr = get_nonempty_singular_form_text(qf);
          if (!tmpstr)
            break;
          int newbpm = atoi(osoc(tmpstr));
          if (newbpm > 0) {
            t->ged.bpm = (Usz)newbpm;
            qnav_stack_pop();
          }
          osofree(tmpstr);
          break;
        }
        case Osc_output_address_form_id: {
          oso *addr = NULL;
          // Empty string is OK here
          if (qform_get_text_line(qf, Single_form_item_id, &addr)) {
            if (osolen(addr))
              ososwap(&t->osc_address, &addr);
            else
              osowipe(&t->osc_address);
            qnav_stack_pop();
            tui_restart_osc_udp_if_enabled(t);
            t->prefs_touched |= TOUCHFLAG(Confopt_osc_output_address);
            tui_save_prefs(t);
          }
          osofree(addr);
          break;
        }
        case Osc_output_port_form_id: {
          oso *portstr = get_nonempty_singular_form_text(qf);
          if (!portstr)
            break;
          qnav_stack_pop();
          ososwap(&t->osc_port, &portstr);
          tui_restart_osc_udp_if_enabled(t);
          osofree(portstr);
          t->prefs_touched |= TOUCHFLAG(Confopt_osc_output_port);
          tui_save_prefs(t);
          break;
        }
        case Set_grid_dims_form_id: {
          oso *tmpstr = get_nonempty_singular_form_text(qf);
          if (!tmpstr)
            break;
          int newheight, newwidth;
          if (sscanf(osoc(tmpstr), "%dx%d", &newwidth, &newheight) == 2 &&
              newheight > 0 && newwidth > 0 && newheight < ORCA_Y_MAX &&
              newwidth < ORCA_X_MAX) {
            if (t->ged.field.height != (Usz)newheight ||
                t->ged.field.width != (Usz)newwidth) {
              ged_resize_grid(&t->ged.field, &t->ged.mbuf_r, (Usz)newheight,
                              (Usz)newwidth, t->ged.tick_num,
                              &t->ged.scratch_field, &t->ged.undo_hist,
                              &t->ged.ged_cursor);
              ged_update_internal_geometry(&t->ged);
              t->ged.needs_remarking = true;
              t->ged.is_draw_dirty = true;
              ged_make_cursor_visible(&t->ged);
            }
            qnav_stack_pop();
          }
          osofree(tmpstr);
          break;
        }
        case Set_soft_margins_form_id: {
          oso *tmpstr = get_nonempty_singular_form_text(qf);
          if (!tmpstr)
            break;
          bool do_save = false;
          int newy, newx;
          if (read_nxn_or_n(osoc(tmpstr), &newx, &newy) && newy >= 0 &&
              newx >= 0 &&
              (newy != t->softmargin_y || newx != t->softmargin_x)) {
            t->prefs_touched |= TOUCHFLAG(Confopt_margins);
            t->softmargin_y = newy;
            t->softmargin_x = newx;
            ungetch(KEY_RESIZE); // kinda lame but whatever
            do_save = true;
          }
          qnav_stack_pop();
          // Might push message, so gotta pop old guy first
          if (do_save)
            tui_save_prefs(t);
          osofree(tmpstr);
          break;
        }
        }
        break;
      }
      }
    }
    break;
  }
  }
  return Tui_menus_consumed_input;
}

//
// main
//

enum {
  Argopt_hardmargins = UCHAR_MAX + 1,
  Argopt_undo_limit,
  Argopt_init_grid_size,
  Argopt_osc_midi_bidule,
  Argopt_strict_timing,
  Argopt_bpm,
  Argopt_seed,
  Argopt_portmidi_deprecated,
  Argopt_osc_deprecated,
};

int main(int argc, char **argv) {
  static struct option tui_options[] = {
      {"hard-margins", required_argument, 0, Argopt_hardmargins},
      {"undo-limit", required_argument, 0, Argopt_undo_limit},
      {"initial-size", required_argument, 0, Argopt_init_grid_size},
      {"help", no_argument, 0, 'h'},
      {"osc-midi-bidule", required_argument, 0, Argopt_osc_midi_bidule},
      {"strict-timing", no_argument, 0, Argopt_strict_timing},
      {"bpm", required_argument, 0, Argopt_bpm},
      {"seed", required_argument, 0, Argopt_seed},
      {"portmidi-list-devices", no_argument, 0, Argopt_portmidi_deprecated},
      {"portmidi-output-device", required_argument, 0,
       Argopt_portmidi_deprecated},
      {"osc-server", required_argument, 0, Argopt_osc_deprecated},
      {"osc-port", required_argument, 0, Argopt_osc_deprecated},
      {NULL, 0, NULL, 0}};
  int init_bpm = 120;
  int init_seed = 1;
  int init_grid_dim_y = 25, init_grid_dim_x = 57;
  bool explicit_initial_grid_size = false;

  Tui t = {.file_name = NULL}; // Weird because of clang warning
  t.undo_history_limit = 100;
  t.softmargin_y = 1;
  t.softmargin_x = 2;
  t.use_gui_cboard = true;
  t.fancy_grid_dots = true;
  t.fancy_grid_rulers = true;

  int longindex = 0;
  for (;;) {
    int c = getopt_long(argc, argv, "h", tui_options, &longindex);
    if (c == -1)
      break;
    switch (c) {
    case 'h':
      usage();
      exit(0);
    case '?':
      usage();
      exit(1);
#define OPTFAIL(...)                                                           \
  {                                                                            \
    fprintf(stderr, "Bad %s argument: %s\n", tui_options[longindex].name,      \
            optarg);                                                           \
    fprintf(stderr, __VA_ARGS__);                                              \
    fputc('\n', stderr);                                                       \
    exit(1);                                                                   \
  }
    case Argopt_hardmargins:
      if (read_nxn_or_n(optarg, &t.hardmargin_x, &t.hardmargin_y) &&
          t.hardmargin_x >= 0 && t.hardmargin_y >= 0)
        break;
      OPTFAIL("Must be 0 or positive integer.");
    case Argopt_undo_limit:
      if (read_int(optarg, &t.undo_history_limit) && t.undo_history_limit >= 0)
        break;
      OPTFAIL("Must be 0 or positive integer.");
    case Argopt_bpm:
      if (read_int(optarg, &init_bpm) && init_bpm >= 1)
        break;
      OPTFAIL("Must be positive integer.");
    case Argopt_seed:
      if (read_int(optarg, &init_seed) && init_seed >= 0)
        break;
      OPTFAIL("Must be 0 or positive integer.");
    case Argopt_init_grid_size:
      if (sscanf(optarg, "%dx%d", &init_grid_dim_x, &init_grid_dim_y) != 2)
        OPTFAIL("Bad format or count. Expected something like: 40x30");
      if (init_grid_dim_x <= 0 || init_grid_dim_x > ORCA_X_MAX)
        OPTFAIL("X dimension for initial-size must be 1 <= n <= %d, was %d.",
                ORCA_X_MAX, init_grid_dim_x);
      if (init_grid_dim_y <= 0 || init_grid_dim_y > ORCA_Y_MAX)
        OPTFAIL("Y dimension for initial-size must be 1 <= n <= %d, was %d.",
                ORCA_Y_MAX, init_grid_dim_y);
      explicit_initial_grid_size = true;
      break;
    case Argopt_osc_midi_bidule:
      osoput(&t.osc_midi_bidule_path, optarg);
      break;
    case Argopt_strict_timing:
      t.strict_timing = true;
      break;
    case Argopt_portmidi_deprecated:
      fprintf(stderr,
              "Option \"--%s\" has been removed.\nInstead, choose "
              "your MIDI output device from within the ORCA menu.\n"
              "This new menu allows you to pick your MIDI device "
              "interactively\n",
              tui_options[longindex].name);
      exit(1);
    case Argopt_osc_deprecated:
      fprintf(
          stderr,
          "Options --osc-server and --osc-port have been removed.\n"
          "Instead, set the OSC server and port from within the ORCA menu.\n");
      exit(1);
    }
  }
#undef OPTFAIL
  if (optind == argc - 1) {
    osoput(&t.file_name, argv[optind]);
  } else if (optind < argc - 1) {
    fprintf(stderr, "Expected only 1 file argument.\n");
    exit(1);
  }
  qnav_init(); // Initialize the menu/navigation global state
  // Initialize the 'Grid EDitor' stuff. This sits underneath the TUI.
  ged_init(&t.ged, (Usz)t.undo_history_limit, (Usz)init_bpm, (Usz)init_seed);
  // This will need to be changed to work with conf/menu
  if (osolen(t.osc_midi_bidule_path) > 0) {
    midi_mode_deinit(&t.ged.midi_mode);
    midi_mode_init_osc_bidule(&t.ged.midi_mode, osoc(t.osc_midi_bidule_path));
  }
  stm_setup(); // Set up timer lib
  // Enable UTF-8 by explicitly initializing our locale before initializing
  // ncurses. Only needed (maybe?) if using libncursesw/wide-chars or UTF-8.
  // Using it unguarded will mess up box drawing chars in Linux virtual
  // consoles unless using libncursesw.
  setlocale(LC_ALL, "");
  initscr(); // Initialize ncurses
  // Allow ncurses to control newline translation. Fine to use with any modern
  // terminal, and will let ncurses run faster.
  nonl();
  // Set interrupt keys (interrupt, break, quit...) to not flush. Helps keep
  // ncurses state consistent, at the cost of less responsive terminal
  // interrupt. (This will rarely happen.)
  intrflush(stdscr, FALSE);
  // Receive keyboard input immediately without line buffering, and receive
  // ctrl+z, ctrl+c etc. as input instead of having a signal generated. We need
  // to do this even with wtimeout() if we don't want ctrl+z etc. to interrupt
  // the program.
  raw();
  noecho();                // Don't echo keyboard input
  keypad(stdscr, TRUE);    // Also receive arrow keys, etc.
  curs_set(0);             // Hide the terminal cursor
  set_escdelay(1);         // Short delay before triggering escape
  term_util_init_colors(); // Our color init routine
  mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
  if (has_mouse()) // no waiting for distinguishing click from press
    mouseinterval(0);
  printf("\033[?2004h\n"); // Ask terminal to use bracketed paste.

  tui_load_conf(&t);                  // load orca.conf (if it exists)
  tui_restart_osc_udp_if_enabled(&t); // start udp if conf enabled it

  wtimeout(stdscr, 0);
  int cur_timeout = 0;
  Usz brackpaste_starting_x = 0, brackpaste_y = 0, brackpaste_x = 0,
      brackpaste_max_y = 0, brackpaste_max_x = 0;
  bool is_in_brackpaste = false;

  WINDOW *cont_window = NULL;
  tui_adjust_term_size(&t, &cont_window);

  bool grid_initialized = false;
  if (osolen(t.file_name)) {
    Field_load_error fle = field_load_file(osoc(t.file_name), &t.ged.field);
    switch (fle) {
    case Field_load_error_ok:
      if (t.ged.field.height < 1 || t.ged.field.width < 1) {
        // Opening an empty file or attempting to open a directory can lead us
        // here.
        field_deinit(&t.ged.field);
        qmsg_printf_push("Unusable File", "Not a usable file:\n%s",
                         (osoc(t.file_name)));
        break;
      }
      grid_initialized = true;
      break;
    case Field_load_error_cant_open_file: {
      // Probably a new file, though TODO we should add an explicit
      // differentiation between "file exists and can't open it" and "file
      // doesn't seem to exist."
      Qmsg *qm = qmsg_printf_push(NULL, "New file: %s", osoc(t.file_name));
      qmsg_set_dismiss_mode(qm, Qmsg_dismiss_mode_passthrough);
      break;
    }
    default:
      qmsg_printf_push("File Load Error", "File load error:\n%s.",
                       field_load_error_string(fle));
      break;
    }
  }
  // If we haven't yet initialized the grid, because we were waiting for the
  // terminal size, do it now.
  if (!grid_initialized) {
    Usz new_field_h, new_field_w;
    if (explicit_initial_grid_size ||
        !tui_suggest_nice_grid_size(&t, t.ged.win_h, t.ged.win_w, &new_field_h,
                                    &new_field_w)) {
      new_field_h = (Usz)init_grid_dim_y;
      new_field_w = (Usz)init_grid_dim_x;
    }
    field_init_fill(&t.ged.field, (Usz)new_field_h, (Usz)new_field_w, '.');
  }
  mbuf_reusable_ensure_size(&t.ged.mbuf_r, t.ged.field.height,
                            t.ged.field.width);
  ged_make_cursor_visible(&t.ged);
  ged_send_osc_bpm(&t.ged, (I32)t.ged.bpm); // Send initial BPM
  ged_set_playing(&t.ged, true);            // Auto-play
  // Enter main loop. Process events as they arrive.
event_loop:;
  int key = wgetch(stdscr);
  if (cur_timeout != 0) {
    wtimeout(stdscr, 0); // Until we run out, don't wait between events.
    cur_timeout = 0;
  }
  switch (key) {
  case ERR: { // ERR indicates no more events.
    ged_do_stuff(&t.ged);
    bool drew_any = false;
    if (ged_is_draw_dirty(&t.ged) || qnav_stack.occlusion_dirty) {
      werase(cont_window);
      ged_draw(&t.ged, cont_window, osoc(t.file_name), t.fancy_grid_dots,
               t.fancy_grid_rulers);
      wnoutrefresh(cont_window);
      drew_any = true;
    }
    drew_any |= qnav_draw(); // clears qnav_stack.occlusion_dirty
    if (drew_any)
      doupdate();
    double secs_to_d = ged_secs_to_deadline(&t.ged);
#define DEADTIME(_millisecs, _new_timeout)                                     \
  else if (secs_to_d < ms_to_sec(_millisecs)) new_timeout = _new_timeout;
    int new_timeout;
    // These values are tuned to work OK with the normal scheduling behavior
    // on Linux, Mac, and Windows. All of the usual caveats of trying to
    // guess what the scheduler will do apply.
    //
    // Of course, there's no guarantee about how the scheduler will work, so
    // if you are using a modified kernel or something, or the measurements
    // here are bad, or it's some OS that behaves differently than expected,
    // this won't be very good. But there's not really much we can do about
    // it, and it's better than doing nothing and burning up the CPU!
    if (t.strict_timing) { // clang-format off
      if (false) {}
      // "If there's less than 1.5 milliseconds to the deadline, use a curses
      // timeout value of 0."
      DEADTIME(  1.5,  0)
      DEADTIME(  3.0,  1)
      DEADTIME(  5.0,  2)
      DEADTIME(  7.0,  3)
      DEADTIME(  9.0,  4)
      DEADTIME( 11.0,  5)
      DEADTIME( 13.0,  6)
      DEADTIME( 15.0,  7)
      DEADTIME( 25.0, 12)
      DEADTIME( 50.0, 20)
      DEADTIME(100.0, 40)
      else new_timeout = 50;
    } else {
      if (false) {}
      DEADTIME(  1.0,  0)
      DEADTIME(  2.0,  1)
      DEADTIME(  7.0,  2)
      DEADTIME( 15.0,  5)
      DEADTIME( 25.0, 10)
      DEADTIME( 50.0, 20)
      DEADTIME(100.0, 40)
      else new_timeout = 50;
    } // clang-format on
#undef DEADTIME
    if (new_timeout != cur_timeout) {
      wtimeout(stdscr, new_timeout);
      cur_timeout = new_timeout;
#if TIME_DEBUG
      spin_track_timeout = cur_timeout;
#endif
    }
    goto event_loop;
  }
  case KEY_RESIZE:
    tui_adjust_term_size(&t, &cont_window);
    goto event_loop;
#ifndef FEAT_NOMOUSE
  case KEY_MOUSE: {
    MEVENT mevent;
    if (cont_window && getmouse(&mevent) == OK) {
      int win_y, win_x;
      int win_h, win_w;
      getbegyx(cont_window, win_y, win_x);
      getmaxyx(cont_window, win_h, win_w);
      int inwin_y = mevent.y - win_y;
      int inwin_x = mevent.x - win_x;
      if (inwin_y >= win_h)
        inwin_y = win_h - 1;
      if (inwin_y < 0)
        inwin_y = 0;
      if (inwin_x >= win_w)
        inwin_x = win_w - 1;
      if (inwin_x < 0)
        inwin_x = 0;
      ged_mouse_event(&t.ged, (Usz)inwin_y, (Usz)inwin_x, mevent.bstate);
    }
    goto event_loop;
  }
#endif
  }

  // If we have the menus open, we'll let the menus do what they want with
  // the input before the regular editor (which will be displayed
  // underneath.) The menus may tell us to quit, that they didn't do anything
  // with the input, or that they consumed the input and therefore we
  // shouldn't pass the input key to the rest of the editing system.
  switch (tui_drive_menus(&t, key)) {
  case Tui_menus_nothing:
    break;
  case Tui_menus_quit:
    goto quit;
  case Tui_menus_consumed_input:
    goto event_loop;
  }

  // If this key input is intended to reach the grid, check to see if we're
  // in bracketed paste and use alternate 'filtered input for characters'
  // mode. We'll ignore most control sequences here.
  if (is_in_brackpaste) {
    if (key == 27 /* escape */) {
      if (brackpaste_seq_getungetch(stdscr) == Brackpaste_seq_end) {
        is_in_brackpaste = false;
        if (brackpaste_max_y > t.ged.ged_cursor.y)
          t.ged.ged_cursor.h = brackpaste_max_y - t.ged.ged_cursor.y + 1;
        if (brackpaste_max_x > t.ged.ged_cursor.x)
          t.ged.ged_cursor.w = brackpaste_max_x - t.ged.ged_cursor.x + 1;
        t.ged.needs_remarking = true;
        t.ged.is_draw_dirty = true;
      }
      goto event_loop;
    }
    if (key == KEY_ENTER)
      key = '\r';
    if (key >= CHAR_MIN && key <= CHAR_MAX) {
      if ((char)key == '\r' || (char)key == '\n') {
        brackpaste_x = brackpaste_starting_x;
        ++brackpaste_y;
        goto event_loop;
      }
      if (key != ' ') {
        char cleaned = (char)key;
        if (!orca_is_valid_glyph((Glyph)key))
          cleaned = '.';
        if (brackpaste_y < t.ged.field.height &&
            brackpaste_x < t.ged.field.width) {
          gbuffer_poke(t.ged.field.buffer, t.ged.field.height,
                       t.ged.field.width, brackpaste_y, brackpaste_x, cleaned);
          // Could move this out one level if we wanted the final selection
          // size to reflect even the pasted area which didn't fit on the
          // grid.
          if (brackpaste_y > brackpaste_max_y)
            brackpaste_max_y = brackpaste_y;
          if (brackpaste_x > brackpaste_max_x)
            brackpaste_max_x = brackpaste_x;
        }
      }
      ++brackpaste_x;
    }
    goto event_loop;
  }

  // Regular inputs when we're not in a menu and not in bracketed paste.
  switch (key) {
  // Checking again for 'quit' here, because it's only listened for if we're
  // in the menus or *not* in bracketed paste mode.
  case CTRL_PLUS('q'):
    goto quit;
  case CTRL_PLUS('o'):
    push_open_form(osoc(t.file_name));
    break;
  case 127: // backspace in terminal.app, apparently
  case KEY_BACKSPACE:
    if (t.ged.input_mode == Ged_input_mode_append) {
      ged_dir_input(&t.ged, Ged_dir_left, 1);
      ged_input_character(&t.ged, '.');
      ged_dir_input(&t.ged, Ged_dir_left, 1);
    } else {
      ged_input_character(&t.ged, '.');
    }
    break;
  case CTRL_PLUS('z'):
  case CTRL_PLUS('u'):
    ged_input_cmd(&t.ged, Ged_input_cmd_undo);
    break;
  case CTRL_PLUS('r'):
    t.ged.tick_num = 0;
    t.ged.needs_remarking = true;
    t.ged.is_draw_dirty = true;
    break;
  case '[':
    ged_adjust_rulers_relative(&t.ged, 0, -1);
    break;
  case ']':
    ged_adjust_rulers_relative(&t.ged, 0, 1);
    break;
  case '{':
    ged_adjust_rulers_relative(&t.ged, -1, 0);
    break;
  case '}':
    ged_adjust_rulers_relative(&t.ged, 1, 0);
    break;
  case '(':
    ged_resize_grid_relative(&t.ged, 0, -1);
    break;
  case ')':
    ged_resize_grid_relative(&t.ged, 0, 1);
    break;
  case '_':
    ged_resize_grid_relative(&t.ged, -1, 0);
    break;
  case '+':
    ged_resize_grid_relative(&t.ged, 1, 0);
    break;
  case '\r':
  case KEY_ENTER:
    break; // Currently unused.
  case CTRL_PLUS('i'):
  case KEY_IC:
    ged_input_cmd(&t.ged, Ged_input_cmd_toggle_append_mode);
    break;
  case '/':
    // Formerly 'piano'/trigger mode toggle. We're repurposing it here to
    // input a '?' instead of a '/' because '?' opens the help guide, and it
    // might be a bad idea to take that away, since orca will take over the
    // TTY and may leave users confused. I know of at least 1 person who was
    // saved by pressing '?' after they didn't know what to do. Hmm.
    ged_input_character(&t.ged, '?');
    break;
  case '<':
    ged_adjust_bpm(&t.ged, -1);
    break;
  case '>':
    ged_adjust_bpm(&t.ged, 1);
    break;
  case CTRL_PLUS('f'):
    ged_input_cmd(&t.ged, Ged_input_cmd_step_forward);
    break;
  case CTRL_PLUS('e'):
    ged_input_cmd(&t.ged, Ged_input_cmd_toggle_show_event_list);
    break;
  case CTRL_PLUS('x'):
    ged_input_cmd(&t.ged, Ged_input_cmd_cut);
    try_send_to_gui_clipboard(&t.ged, &t.use_gui_cboard);
    break;
  case CTRL_PLUS('c'):
    ged_input_cmd(&t.ged, Ged_input_cmd_copy);
    try_send_to_gui_clipboard(&t.ged, &t.use_gui_cboard);
    break;
  case CTRL_PLUS('v'):
    if (t.use_gui_cboard) {
      bool added_hist =
          undo_history_push(&t.ged.undo_hist, &t.ged.field, t.ged.tick_num);
      Usz pasted_h, pasted_w;
      Cboard_error cberr = cboard_paste(
          t.ged.field.buffer, t.ged.field.height, t.ged.field.width,
          t.ged.ged_cursor.y, t.ged.ged_cursor.x, &pasted_h, &pasted_w);
      if (cberr) {
        if (added_hist)
          undo_history_pop(&t.ged.undo_hist, &t.ged.field, &t.ged.tick_num);
        t.use_gui_cboard = false;
        ged_input_cmd(&t.ged, Ged_input_cmd_paste);
      } else {
        if (pasted_h > 0 && pasted_w > 0) {
          t.ged.ged_cursor.h = pasted_h;
          t.ged.ged_cursor.w = pasted_w;
        }
      }
      t.ged.needs_remarking = true;
      t.ged.is_draw_dirty = true;
    } else {
      ged_input_cmd(&t.ged, Ged_input_cmd_paste);
    }
    break;
  case '\'':
    ged_input_cmd(&t.ged, Ged_input_cmd_toggle_selresize_mode);
    break;
  case '`':
  case '~':
    ged_input_cmd(&t.ged, Ged_input_cmd_toggle_slide_mode);
    break;
  case ' ':
    if (t.ged.input_mode == Ged_input_mode_append)
      ged_input_character(&t.ged, '.');
    else
      ged_input_cmd(&t.ged, Ged_input_cmd_toggle_play_pause);
    break;
  case 27: // Escape
    // Check for escape sequences we're interested in that ncurses didn't
    // handle. Such as bracketed paste.
    if (brackpaste_seq_getungetch(stdscr) == Brackpaste_seq_begin) {
      is_in_brackpaste = true;
      undo_history_push(&t.ged.undo_hist, &t.ged.field, t.ged.tick_num);
      brackpaste_y = t.ged.ged_cursor.y;
      brackpaste_x = t.ged.ged_cursor.x;
      brackpaste_starting_x = brackpaste_x;
      brackpaste_max_y = brackpaste_y;
      brackpaste_max_x = brackpaste_x;
      break;
    }
    ged_input_cmd(&t.ged, Ged_input_cmd_escape);
    break;

  case 330: // delete?
    ged_input_character(&t.ged, '.');
    break;

  // Cursor movement
  case KEY_UP:
  case CTRL_PLUS('k'):
    ged_dir_input(&t.ged, Ged_dir_up, 1);
    break;
  case CTRL_PLUS('j'):
  case KEY_DOWN:
    ged_dir_input(&t.ged, Ged_dir_down, 1);
    break;
  case CTRL_PLUS('h'):
  case KEY_LEFT:
    ged_dir_input(&t.ged, Ged_dir_left, 1);
    break;
  case CTRL_PLUS('l'):
  case KEY_RIGHT:
    ged_dir_input(&t.ged, Ged_dir_right, 1);
    break;

  // Selection size modification. These may not work in all terminals. (Only
  // tested in xterm so far.)
  case 337: // shift-up
    ged_modify_selection_size(&t.ged, -1, 0);
    break;
  case 336: // shift-down
    ged_modify_selection_size(&t.ged, 1, 0);
    break;
  case 393: // shift-left
    ged_modify_selection_size(&t.ged, 0, -1);
    break;
  case 402: // shift-right
    ged_modify_selection_size(&t.ged, 0, 1);
    break;
  case 567: // shift-control-up
    ged_modify_selection_size(&t.ged, -(int)t.ged.ruler_spacing_y, 0);
    break;
  case 526: // shift-control-down
    ged_modify_selection_size(&t.ged, (int)t.ged.ruler_spacing_y, 0);
    break;
  case 546: // shift-control-left
    ged_modify_selection_size(&t.ged, 0, -(int)t.ged.ruler_spacing_x);
    break;
  case 561: // shift-control-right
    ged_modify_selection_size(&t.ged, 0, (int)t.ged.ruler_spacing_x);
    break;

  // Move cursor further if control is held
  case 566: // control-up
    ged_dir_input(&t.ged, Ged_dir_up, (int)t.ged.ruler_spacing_y);
    break;
  case 525: // control-down
    ged_dir_input(&t.ged, Ged_dir_down, (int)t.ged.ruler_spacing_y);
    break;
  case 545: // control-left
    ged_dir_input(&t.ged, Ged_dir_left, (int)t.ged.ruler_spacing_x);
    break;
  case 560: // control-right
    ged_dir_input(&t.ged, Ged_dir_right, (int)t.ged.ruler_spacing_x);
    break;

  // Slide selection on alt-arrow
  case 564: // alt-up
    ged_slide_selection(&t.ged, -1, 0);
    break;
  case 523: // alt-down
    ged_slide_selection(&t.ged, 1, 0);
    break;
  case 543: // alt-left
    ged_slide_selection(&t.ged, 0, -1);
    break;
  case 558: // alt-right
    ged_slide_selection(&t.ged, 0, 1);
    break;

  case CTRL_PLUS('d'):
  case KEY_F(1):
    push_main_menu();
    break;
  case '?':
    push_controls_msg();
    break;
  case CTRL_PLUS('g'):
    push_opers_guide_msg();
    break;
  case CTRL_PLUS('s'):
    tui_try_save(&t);
    break;

  default:
    if (key >= CHAR_MIN && key <= CHAR_MAX && orca_is_valid_glyph((Glyph)key))
      ged_input_character(&t.ged, (char)key);
#if 0
      else
        fprintf(stderr, "Unknown key number: %d\n", key);
#endif
    break;
  }
  goto event_loop;
quit:
  ged_stop_all_sustained_notes(&t.ged);
  qnav_deinit();
  if (cont_window)
    delwin(cont_window);
  printf("\033[?2004h\n"); // Tell terminal to not use bracketed paste
  endwin();
  ged_deinit(&t.ged);
  osofree(t.file_name);
  osofree(t.osc_address);
  osofree(t.osc_port);
  osofree(t.osc_midi_bidule_path);
#ifdef FEAT_PORTMIDI
  if (portmidi_is_initialized)
    Pm_Terminate();
#endif
}

#undef TOUCHFLAG
#undef staticni
