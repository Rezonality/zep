#include "term_util.h"
#include "oso.h"
#include <ctype.h>
#include <form.h>
#include <menu.h>

void term_util_init_colors() {
  if (has_colors()) {
    // Enable color
    start_color();
    use_default_colors();
    for (int ifg = 0; ifg < Colors_count; ++ifg) {
      for (int ibg = 0; ibg < Colors_count; ++ibg) {
        int res = init_pair((short int)(1 + ifg * Colors_count + ibg),
                            (short int)(ifg - 1), (short int)(ibg - 1));
        (void)res;
        // Might fail on Linux virtual console/terminal for a couple of colors.
        // Just ignore.
#if 0
        if (res == ERR) {
          endwin();
          fprintf(stderr, "Error initializing color pair: %d %d\n", ifg - 1,
                  ibg - 1);
          exit(1);
        }
#endif
      }
    }
  }
}

#define ORCA_CONTAINER_OF(ptr, type, member)                                   \
  ((type *)((char *)(1 ? (ptr) : &((type *)0)->member) -                       \
            offsetof(type, member)))

struct Qmsg {
  Qblock qblock;
  Qmsg_dismiss_mode dismiss_mode;
};

struct Qmenu_item_extra {
  int user_id;
  U8 owns_string : 1, is_spacer : 1;
};

struct Qmenu {
  Qblock qblock;
  MENU *ncurses_menu;
  ITEM **ncurses_items;
  Usz items_count, items_cap;
  ITEM *initial_item;
  int id;
  // Flag for right-padding hack. Temp until we do our own menus
  U8 has_submenu_item : 1;
};

struct Qform {
  Qblock qblock;
  FORM *ncurses_form;
  FIELD *ncurses_fields[32];
  Usz fields_count;
  int id;
};

Qnav_stack qnav_stack;

void qnav_init() { qnav_stack = (Qnav_stack){.blocks = {0}}; }
void qnav_deinit() {
  while (qnav_stack.count != 0)
    qnav_stack_pop();
}
static ORCA_NOINLINE void qnav_stack_push(Qblock *qb, int height, int width) {
#ifndef NDEBUG
  for (Usz i = 0; i < qnav_stack.count; ++i) {
    assert(qnav_stack.blocks[i] != qb);
  }
#endif
  int top = 0, left = 0;
  int total_h = height + 2, total_w = width + 2;
  if (qnav_stack.count > 0) {
    WINDOW *w = qnav_stack.blocks[qnav_stack.count - 1]->outer_window;
    int prev_y, prev_x, prev_h, prev_w;
    getbegyx(w, prev_y, prev_x);
    getmaxyx(w, prev_h, prev_w);
    // Start by trying to position the item to the right of the previous item.
    left = prev_x + prev_w + 0;
    int term_h, term_w;
    getmaxyx(stdscr, term_h, term_w);
    // Check if we'll run out of room if we position the new item to the right
    // of the existing item (with the same Y position.)
    if (left + total_w > term_w) {
      // If we have enough room if we position just below the previous item in
      // the stack, do that instead of positioning to the right of it.
      if (prev_x + total_w <= term_w && total_h < term_h - (prev_y + prev_h)) {
        top = prev_y + prev_h;
        left = prev_x;
      }
      // If the item doesn't fit there, but it's less wide than the terminal,
      // right-align it to the edge of the terminal.
      else if (total_w < term_w) {
        left = term_w - total_w;
      }
      // Otherwise, just start the layout over at Y=0,X=0
      else {
        left = 0;
      }
    }
  }
  qnav_stack.blocks[qnav_stack.count] = qb;
  ++qnav_stack.count;
  qb->outer_window = newpad(total_h, total_w);
  // This used to be derwin when when used newwin instead of newpad -- not sure
  // if we should use derwin or subpad now. subpad is probably more compatible.
  // ncurses docs state that it handles it correctly, unlike some others?
  qb->content_window = subpad(qb->outer_window, height, width, 1, 1);
  qb->y = top;
  qb->x = left;
  qnav_stack.occlusion_dirty = true;
}

Qblock *qnav_top_block() {
  if (qnav_stack.count == 0)
    return NULL;
  return qnav_stack.blocks[qnav_stack.count - 1];
}

void qblock_init(Qblock *qb, Qblock_type_tag tag) {
  *qb = (Qblock){0};
  qb->tag = tag;
}

void qmenu_free(Qmenu *qm);
void qform_free(Qform *qf);

void qnav_free_block(Qblock *qb) {
  switch (qb->tag) {
  case Qblock_type_qmsg: {
    Qmsg *qm = qmsg_of(qb);
    free(qm);
    break;
  }
  case Qblock_type_qmenu:
    qmenu_free(qmenu_of(qb));
    break;
  case Qblock_type_qform:
    qform_free(qform_of(qb));
    break;
  }
}

void qnav_stack_pop(void) {
  assert(qnav_stack.count > 0);
  if (qnav_stack.count == 0)
    return;
  Qblock *qb = qnav_stack.blocks[qnav_stack.count - 1];
  WINDOW *content_window = qb->content_window;
  WINDOW *outer_window = qb->outer_window;
  // erase any stuff underneath where this window is, in case it's outside of
  // the grid in an area that isn't actively redraw
  werase(outer_window);
  wnoutrefresh(outer_window);
  qnav_free_block(qb);
  delwin(content_window);
  delwin(outer_window);
  --qnav_stack.count;
  qnav_stack.blocks[qnav_stack.count] = NULL;
  qnav_stack.occlusion_dirty = true;
}

bool qnav_draw(void) {
  bool drew_any = false;
  if (qnav_stack.count < 1)
    goto done;
  int term_h, term_w;
  getmaxyx(stdscr, term_h, term_w);
  for (Usz i = 0; i < qnav_stack.count; ++i) {
    Qblock *qb = qnav_stack.blocks[i];
    if (qnav_stack.occlusion_dirty) {
      bool is_frontmost = i == qnav_stack.count - 1;
      qblock_print_frame(qb, is_frontmost);
      switch (qb->tag) {
      case Qblock_type_qmsg:
        break;
      case Qblock_type_qmenu:
        qmenu_set_displayed_active(qmenu_of(qb), is_frontmost);
        break;
      case Qblock_type_qform:
        break;
      }
    }
    touchwin(qb->outer_window); // here? or after continue?
    if (term_h < 1 || term_w < 1)
      continue;
    int qbwin_h, qbwin_w;
    getmaxyx(qb->outer_window, qbwin_h, qbwin_w);
    int qbwin_endy = qb->y + qbwin_h;
    int qbwin_endx = qb->x + qbwin_w;
    if (qbwin_endy >= term_h)
      qbwin_endy = term_h - 1;
    if (qbwin_endx >= term_w)
      qbwin_endx = term_w - 1;
    if (qb->y >= qbwin_endy || qb->x >= qbwin_endx)
      continue;
    pnoutrefresh(qb->outer_window, 0, 0, qb->y, qb->x, qbwin_endy, qbwin_endx);
    drew_any = true;
  }
done:
  qnav_stack.occlusion_dirty = false;
  return drew_any;
}

void qblock_print_border(Qblock *qb, unsigned int attr) {
  wborder(qb->outer_window, ACS_VLINE | attr, ACS_VLINE | attr,
          ACS_HLINE | attr, ACS_HLINE | attr, ACS_ULCORNER | attr,
          ACS_URCORNER | attr, ACS_LLCORNER | attr, ACS_LRCORNER | attr);
}

void qblock_print_title(Qblock *qb, char const *title, int attr) {
  wmove(qb->outer_window, 0, 1);
  attr_t attrs = A_NORMAL;
  short pair = 0;
  wattr_get(qb->outer_window, &attrs, &pair, NULL);
  wattrset(qb->outer_window, attr);
  waddch(qb->outer_window, ' ');
  wprintw(qb->outer_window, title);
  waddch(qb->outer_window, ' ');
  wattr_set(qb->outer_window, attrs, pair, NULL);
}

void qblock_set_title(Qblock *qb, char const *title) { qb->title = title; }

void qblock_print_frame(Qblock *qb, bool active) {
  qblock_print_border(qb, active ? A_NORMAL : A_DIM);
  if (qb->title) {
    qblock_print_title(qb, qb->title, active ? A_NORMAL : A_DIM);
  }
  if (qb->tag == Qblock_type_qform) {
    Qform *qf = qform_of(qb);
    if (qf->ncurses_form) {
      pos_form_cursor(qf->ncurses_form);
    }
  }
}

WINDOW *qmsg_window(Qmsg *qm) { return qm->qblock.content_window; }

void qmsg_set_title(Qmsg *qm, char const *title) {
  qblock_set_title(&qm->qblock, title);
}

void qmsg_set_dismiss_mode(Qmsg *qm, Qmsg_dismiss_mode mode) {
  if (qm->dismiss_mode == mode)
    return;
  qm->dismiss_mode = mode;
}

Qmsg *qmsg_push(int height, int width) {
  Qmsg *qm = malloc(sizeof(Qmsg));
  qblock_init(&qm->qblock, Qblock_type_qmsg);
  qm->dismiss_mode = Qmsg_dismiss_mode_explicitly;
  qnav_stack_push(&qm->qblock, height, width);
  return qm;
}

Qmsg *qmsg_printf_push(char const *title, char const *fmt, ...) {
  int titlewidth = title ? (int)strlen(title) : 0;
  va_list ap;
  va_start(ap, fmt);
  int msgbytes = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  char *buffer = malloc((Usz)msgbytes + 1);
  if (!buffer)
    exit(1);
  va_start(ap, fmt);
  int printedbytes = vsnprintf(buffer, (Usz)msgbytes + 1, fmt, ap);
  va_end(ap);
  if (printedbytes != msgbytes)
    exit(1); // todo better handling?
  int lines = 1;
  int curlinewidth = 0;
  int maxlinewidth = 0;
  for (int i = 0; i < msgbytes; i++) {
    if (buffer[i] == '\n') {
      buffer[i] = '\0'; // This is terrifying :)
      lines++;
      if (curlinewidth > maxlinewidth)
        maxlinewidth = curlinewidth;
      curlinewidth = 0;
    } else {
      curlinewidth++;
    }
  }
  if (curlinewidth > maxlinewidth)
    maxlinewidth = curlinewidth;
  int width = titlewidth > maxlinewidth ? titlewidth : maxlinewidth;
  width += 2;                          // 1 padding on left and right each
  Qmsg *msg = qmsg_push(lines, width); // no wrapping yet, no real wcwidth, etc
  WINDOW *msgw = qmsg_window(msg);
  int i = 0;
  int offset = 0;
  for (;;) {
    if (offset == msgbytes + 1)
      break;
    int numbytes = (int)strlen(buffer + offset);
    wmove(msgw, i, 1);
    waddstr(msgw, buffer + offset);
    offset += numbytes + 1;
    i++;
  }
  free(buffer);
  if (title)
    qmsg_set_title(msg, title);
  return msg;
}

bool qmsg_drive(Qmsg *qm, int key, Qmsg_action *out_action) {
  *out_action = (Qmsg_action){0};
  Qmsg_dismiss_mode dm = qm->dismiss_mode;
  switch (dm) {
  case Qmsg_dismiss_mode_explicitly:
    break;
  case Qmsg_dismiss_mode_easily:
    out_action->dismiss = true;
    return true;
  case Qmsg_dismiss_mode_passthrough:
    out_action->dismiss = true;
    out_action->passthrough = true;
    return true;
  }
  switch (key) {
  case ' ':
  case 27:
  case '\r':
  case KEY_ENTER:
    out_action->dismiss = true;
    return true;
  }
  return false;
}

Qmsg *qmsg_of(Qblock *qb) { return ORCA_CONTAINER_OF(qb, Qmsg, qblock); }

Qmenu *qmenu_create(int id) {
  Qmenu *qm = (Qmenu *)malloc(sizeof(Qmenu));
  qblock_init(&qm->qblock, Qblock_type_qmenu);
  qm->ncurses_menu = NULL;
  qm->ncurses_items = NULL;
  qm->items_count = 0;
  qm->items_cap = 0;
  qm->initial_item = NULL;
  qm->id = id;
  qm->has_submenu_item = 0;
  return qm;
}
void qmenu_destroy(Qmenu *qm) { qmenu_free(qm); }
int qmenu_id(Qmenu const *qm) { return qm->id; }
static ORCA_NOINLINE void
qmenu_allocitems(Qmenu *qm, Usz count, Usz *out_idx, ITEM ***out_items,
                 struct Qmenu_item_extra **out_extras) {
  Usz old_count = qm->items_count;
  // Add 1 for the extra null terminator guy
  Usz new_count = old_count + count + 1;
  Usz items_cap = qm->items_cap;
  ITEM **items = qm->ncurses_items;
  if (new_count > items_cap) {
    // todo overflow check, realloc fail check
    Usz old_cap = items_cap;
    Usz new_cap = new_count < 32 ? 32 : orca_round_up_power2(new_count);
    Usz new_size = new_cap * (sizeof(ITEM *) + sizeof(struct Qmenu_item_extra));
    ITEM **new_items = (ITEM **)realloc(items, new_size);
    if (!new_items)
      exit(1);
    items = new_items;
    items_cap = new_cap;
    // Move old extras data to new position
    Usz old_extras_offset = sizeof(ITEM *) * old_cap;
    Usz new_extras_offset = sizeof(ITEM *) * new_cap;
    Usz old_extras_size = sizeof(struct Qmenu_item_extra) * old_count;
    memmove((char *)items + new_extras_offset,
            (char *)items + old_extras_offset, old_extras_size);
    qm->ncurses_items = new_items;
    qm->items_cap = new_cap;
  }
  // Not using new_count here in order to leave an extra 1 for the null
  // terminator as required by ncurses.
  qm->items_count = old_count + count;
  Usz extras_offset = sizeof(ITEM *) * items_cap;
  *out_idx = old_count;
  *out_items = items + old_count;
  *out_extras =
      (struct Qmenu_item_extra *)((char *)items + extras_offset) + old_count;
}
ORCA_FORCEINLINE static struct Qmenu_item_extra *
qmenu_item_extras_ptr(Qmenu *qm) {
  Usz offset = sizeof(ITEM *) * qm->items_cap;
  return (struct Qmenu_item_extra *)((char *)qm->ncurses_items + offset);
}
// Get the curses menu item user pointer out, turn it to an int, and use it as
// an index into the 'extras' arrays.
ORCA_FORCEINLINE static struct Qmenu_item_extra *
qmenu_itemextra(struct Qmenu_item_extra *extras, ITEM *item) {
  return extras + (int)(intptr_t)(item_userptr(item));
}
void qmenu_set_title(Qmenu *qm, char const *title) {
  qblock_set_title(&qm->qblock, title);
}
void qmenu_add_choice(Qmenu *qm, int id, char const *text) {
  assert(id != 0);
  Usz idx;
  ITEM **items;
  struct Qmenu_item_extra *extras;
  qmenu_allocitems(qm, 1, &idx, &items, &extras);
  items[0] = new_item(text, NULL);
  set_item_userptr(items[0], (void *)(uintptr_t)idx);
  extras[0].user_id = id;
  extras[0].owns_string = false;
  extras[0].is_spacer = false;
}
void qmenu_add_submenu(Qmenu *qm, int id, char const *text) {
  assert(id != 0);
  qm->has_submenu_item = true; // don't add +1 right padding to subwindow
  Usz idx;
  ITEM **items;
  struct Qmenu_item_extra *extras;
  qmenu_allocitems(qm, 1, &idx, &items, &extras);
  items[0] = new_item(text, ">");
  set_item_userptr(items[0], (void *)(uintptr_t)idx);
  extras[0].user_id = id;
  extras[0].owns_string = false;
  extras[0].is_spacer = false;
}
void qmenu_add_printf(Qmenu *qm, int id, char const *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int textsize = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  char *buffer = malloc((Usz)textsize + 1);
  if (!buffer)
    exit(1);
  va_start(ap, fmt);
  int printedsize = vsnprintf(buffer, (Usz)textsize + 1, fmt, ap);
  va_end(ap);
  if (printedsize != textsize)
    exit(1); // todo better handling?
  Usz idx;
  ITEM **items;
  struct Qmenu_item_extra *extras;
  qmenu_allocitems(qm, 1, &idx, &items, &extras);
  items[0] = new_item(buffer, NULL);
  set_item_userptr(items[0], (void *)(uintptr_t)idx);
  extras[0].user_id = id;
  extras[0].owns_string = true;
  extras[0].is_spacer = false;
}
void qmenu_add_spacer(Qmenu *qm) {
  Usz idx;
  ITEM **items;
  struct Qmenu_item_extra *extras;
  qmenu_allocitems(qm, 1, &idx, &items, &extras);
  items[0] = new_item(" ", NULL);
  item_opts_off(items[0], O_SELECTABLE);
  set_item_userptr(items[0], (void *)(uintptr_t)idx);
  extras[0].user_id = 0;
  extras[0].owns_string = false;
  extras[0].is_spacer = true;
}
void qmenu_set_current_item(Qmenu *qm, int id) {
  ITEM **items = qm->ncurses_items;
  struct Qmenu_item_extra *extras = qmenu_item_extras_ptr(qm);
  ITEM *found = NULL;
  for (Usz i = 0, n = qm->items_count; i < n; i++) {
    ITEM *item = items[i];
    if (qmenu_itemextra(extras, item)->user_id == id) {
      found = item;
      break;
    }
  }
  if (!found)
    return;
  if (qm->ncurses_menu) {
    set_current_item(qm->ncurses_menu, found);
  } else {
    qm->initial_item = found;
  }
}
int qmenu_current_item(Qmenu *qm) {
  ITEM *item = NULL;
  if (qm->ncurses_menu)
    item = current_item(qm->ncurses_menu);
  if (!item)
    item = qm->initial_item;
  if (!item)
    return 0;
  struct Qmenu_item_extra *extras = qmenu_item_extras_ptr(qm);
  return qmenu_itemextra(extras, item)->user_id;
}
void qmenu_set_displayed_active(Qmenu *qm, bool active) {
  // Could add a flag in the Qmenu to avoid redundantly changing this stuff.
  set_menu_fore(qm->ncurses_menu, active ? A_BOLD : A_DIM);
  set_menu_back(qm->ncurses_menu, active ? A_NORMAL : A_DIM);
  set_menu_grey(qm->ncurses_menu, active ? A_DIM : A_DIM);
}
void qmenu_push_to_nav(Qmenu *qm) {
  // new_menu() will get angry if there are no items in the menu. We'll get a
  // null pointer back, and our code will get angry. Instead, just add an empty
  // spacer item. This will probably only ever occur as a programming error,
  // but we should try to avoid having to deal with qmenu_push_to_nav()
  // returning a non-ignorable error for now.
  if (qm->items_count == 0)
    qmenu_add_spacer(qm);
  // Allocating items always leaves an extra available item at the end. This is
  // so we can assign a NULL to it here, since ncurses requires the array to be
  // null terminated instead of using a count.
  qm->ncurses_items[qm->items_count] = NULL;
  qm->ncurses_menu = new_menu(qm->ncurses_items);
  set_menu_mark(qm->ncurses_menu, " > ");
  set_menu_fore(qm->ncurses_menu, A_BOLD);
  set_menu_grey(qm->ncurses_menu, A_DIM);
  set_menu_format(qm->ncurses_menu, 30, 1); // temp to allow large Y
  int menu_min_h, menu_min_w;
  scale_menu(qm->ncurses_menu, &menu_min_h, &menu_min_w);
  if (!qm->has_submenu_item)
    menu_min_w += 1; // temp hack
  if (qm->qblock.title) {
    // Stupid lack of wcswidth() means we can't know how wide this string is
    // actually displayed. Just fake it for now, until we have Unicode strings
    // in the UI. Then we get sad.
    int title_w = (int)strlen(qm->qblock.title) + 2;
    if (title_w > menu_min_w)
      menu_min_w = title_w;
  }
  if (qm->initial_item)
    set_current_item(qm->ncurses_menu, qm->initial_item);
  qnav_stack_push(&qm->qblock, menu_min_h, menu_min_w);
  set_menu_win(qm->ncurses_menu, qm->qblock.outer_window);
  set_menu_sub(qm->ncurses_menu, qm->qblock.content_window);
  // TODO use this to set how "big" the menu is, visually, for scrolling.
  // (ncurses can't figure that out on its own, aparently...)
  // We'll need to split apart some work chunks so that we calculate the size
  // beforehand.
  // set_menu_format(qm->ncurses_menu, 5, 1);
  post_menu(qm->ncurses_menu);
}

void qmenu_free(Qmenu *qm) {
  unpost_menu(qm->ncurses_menu);
  free_menu(qm->ncurses_menu);
  struct Qmenu_item_extra *extras = qmenu_item_extras_ptr(qm);
  for (Usz i = 0; i < qm->items_count; ++i) {
    ITEM *item = qm->ncurses_items[i];
    struct Qmenu_item_extra *extra = qmenu_itemextra(extras, item);
    char const *freed_str = NULL;
    if (extra->owns_string)
      freed_str = item_name(item);
    free_item(qm->ncurses_items[i]);
    if (freed_str)
      free((void *)freed_str);
  }
  free(qm->ncurses_items);
  free(qm);
}

ORCA_NOINLINE
static void qmenu_drive_upordown(Qmenu *qm, int req_up_or_down) {
  struct Qmenu_item_extra *extras = qmenu_item_extras_ptr(qm);
  ITEM *starting = current_item(qm->ncurses_menu);
  menu_driver(qm->ncurses_menu, req_up_or_down);
  ITEM *cur = current_item(qm->ncurses_menu);
  for (;;) {
    if (!cur || cur == starting)
      break;
    if (!qmenu_itemextra(extras, cur)->is_spacer)
      break;
    ITEM *prev = cur;
    menu_driver(qm->ncurses_menu, req_up_or_down);
    cur = current_item(qm->ncurses_menu);
    if (cur == prev)
      break;
  }
}

bool qmenu_drive(Qmenu *qm, int key, Qmenu_action *out_action) {
  struct Qmenu_item_extra *extras = qmenu_item_extras_ptr(qm);
  switch (key) {
  case 27: {
    out_action->any.type = Qmenu_action_type_canceled;
    return true;
  }
  case ' ':
  case '\r':
  case KEY_ENTER: {
    ITEM *cur = current_item(qm->ncurses_menu);
    out_action->picked.type = Qmenu_action_type_picked;
    out_action->picked.id = cur ? qmenu_itemextra(extras, cur)->user_id : 0;
    return true;
  }
  case KEY_UP:
    qmenu_drive_upordown(qm, REQ_UP_ITEM);
    return false;
  case KEY_DOWN:
    qmenu_drive_upordown(qm, REQ_DOWN_ITEM);
    return false;
  }
  return false;
}

Qmenu *qmenu_of(Qblock *qb) { return ORCA_CONTAINER_OF(qb, Qmenu, qblock); }

bool qmenu_top_is_menu(int id) {
  Qblock *qb = qnav_top_block();
  if (!qb)
    return false;
  if (qb->tag != Qblock_type_qmenu)
    return false;
  Qmenu *qm = qmenu_of(qb);
  return qm->id == id;
}

Qform *qform_create(int id) {
  Qform *qf = (Qform *)malloc(sizeof(Qform));
  qblock_init(&qf->qblock, Qblock_type_qform);
  qf->ncurses_form = NULL;
  qf->ncurses_fields[0] = NULL;
  qf->fields_count = 0;
  qf->id = id;
  return qf;
}

Qform *qform_of(Qblock *qb) { return ORCA_CONTAINER_OF(qb, Qform, qblock); }

int qform_id(Qform const *qf) { return qf->id; }

void qform_add_text_line(Qform *qf, int id, char const *initial) {
  FIELD *f = new_field(1, 30, 0, 0, 0, 0);
  if (initial)
    set_field_buffer(f, 0, initial);
  set_field_userptr(f, (void *)(intptr_t)(id));
  field_opts_off(f, O_WRAP | O_BLANK | O_STATIC);
  qf->ncurses_fields[qf->fields_count] = f;
  ++qf->fields_count;
  qf->ncurses_fields[qf->fields_count] = NULL;
}

void qform_push_to_nav(Qform *qf) {
  qf->ncurses_form = new_form(qf->ncurses_fields);
  int form_min_h, form_min_w;
  scale_form(qf->ncurses_form, &form_min_h, &form_min_w);
  qnav_stack_push(&qf->qblock, form_min_h, form_min_w);
  set_form_win(qf->ncurses_form, qf->qblock.outer_window);
  set_form_sub(qf->ncurses_form, qf->qblock.content_window);
  post_form(qf->ncurses_form);
  // quick'n'dirty cursor change for now
  curs_set(1);
  form_driver(qf->ncurses_form, REQ_END_LINE);
}

void qform_set_title(Qform *qf, char const *title) {
  qblock_set_title(&qf->qblock, title);
}

void qform_free(Qform *qf) {
  curs_set(0);
  unpost_form(qf->ncurses_form);
  free_form(qf->ncurses_form);
  for (Usz i = 0; i < qf->fields_count; ++i) {
    free_field(qf->ncurses_fields[i]);
  }
  free(qf);
}

bool qform_drive(Qform *qf, int key, Qform_action *out_action) {
  switch (key) {
  case 27:
    out_action->any.type = Qform_action_type_canceled;
    return true;
  case CTRL_PLUS('a'):
    form_driver(qf->ncurses_form, REQ_BEG_LINE);
    return false;
  case CTRL_PLUS('e'):
    form_driver(qf->ncurses_form, REQ_END_LINE);
    return false;
  case CTRL_PLUS('b'):
    form_driver(qf->ncurses_form, REQ_PREV_CHAR);
    return false;
  case CTRL_PLUS('f'):
    form_driver(qf->ncurses_form, REQ_NEXT_CHAR);
    return false;
  case CTRL_PLUS('k'):
    form_driver(qf->ncurses_form, REQ_CLR_EOL);
    return false;
  case KEY_RIGHT:
    form_driver(qf->ncurses_form, REQ_RIGHT_CHAR);
    return false;
  case KEY_LEFT:
    form_driver(qf->ncurses_form, REQ_LEFT_CHAR);
    return false;
  case 127: // backspace in terminal.app, apparently
  case KEY_BACKSPACE:
  case CTRL_PLUS('h'):
    form_driver(qf->ncurses_form, REQ_DEL_PREV);
    return false;
  case '\r':
  case KEY_ENTER:
    out_action->any.type = Qform_action_type_submitted;
    return true;
  }
  form_driver(qf->ncurses_form, key);
  return false;
}

static Usz size_without_trailing_spaces(char const *str) {
  Usz size = strlen(str);
  for (;;) {
    if (size == 0)
      break;
    if (!isspace(str[size - 1]))
      break;
    --size;
  }
  return size;
}

FIELD *qform_find_field(Qform const *qf, int id) {
  Usz count = qf->fields_count;
  for (Usz i = 0; i < count; ++i) {
    FIELD *f = qf->ncurses_fields[i];
    if ((int)(intptr_t)field_userptr(f) == id)
      return f;
  }
  return NULL;
}

bool qform_get_text_line(Qform const *qf, int id, oso **out) {
  FIELD *f = qform_find_field(qf, id);
  if (!f)
    return false;
  form_driver(qf->ncurses_form, REQ_VALIDATION);
  char *buf = field_buffer(f, 0);
  if (!buf)
    return false;
  Usz trimmed = size_without_trailing_spaces(buf);
  osoputlen(out, buf, trimmed);
  return true;
}
