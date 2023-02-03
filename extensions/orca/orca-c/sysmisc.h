#pragma once
#include "base.h"
#include <stdio.h> // FILE cannot be forward declared
struct oso;

void expand_home_tilde(struct oso **path);

typedef enum {
  Cboard_error_none = 0,
  Cboard_error_unavailable,
  Cboard_error_popen_failed,
  Cboard_error_process_exit_error,
} Cboard_error;

Cboard_error cboard_copy(Glyph const *gbuffer, Usz field_height,
                         Usz field_width, Usz rect_y, Usz rect_x, Usz rect_h,
                         Usz rect_w);

Cboard_error cboard_paste(Glyph *gbuffer, Usz height, Usz width, Usz y, Usz x,
                          Usz *out_h, Usz *out_w);

typedef enum {
  Conf_read_left_and_right = 0, // left and right will be set
  Conf_read_irrelevant,         // only left will be set
  Conf_read_buffer_too_small,   // neither will be set
  Conf_read_eof,                // "
  Conf_read_io_error,           // "
} Conf_read_result;

Conf_read_result conf_read_line(FILE *file, char *buf, Usz bufsize,
                                char **out_left, Usz *out_leftlen,
                                char **out_right, Usz *out_rightlen);

bool conf_read_match(FILE **pfile, char const *const *names, Usz nameslen,
                     char *buf, Usz bufsize, Usz *out_index, char **out_value);

FILE *conf_file_open_for_reading(char const *conf_file_name);

typedef struct {
  FILE *origfile, *tempfile;
  struct oso *canonpath, *temppath;
} Conf_save;

typedef enum {
  Conf_save_start_ok = 0,
  Conf_save_start_bad_conf_name,
  Conf_save_start_alloc_failed,
  Conf_save_start_no_home,
  Conf_save_start_mkdir_failed,
  Conf_save_start_conf_dir_not_dir,
  Conf_save_start_temp_file_perm_denied,
  Conf_save_start_old_temp_file_stuck,
  Conf_save_start_temp_file_open_failed,
} Conf_save_start_error;

typedef enum {
  Conf_save_commit_ok = 0,
  Conf_save_commit_temp_fsync_failed,
  Conf_save_commit_temp_close_failed,
  Conf_save_commit_rename_failed,
} Conf_save_commit_error;

Conf_save_start_error conf_save_start(Conf_save *p, char const *conf_file_name);
// `*p` may be passed in uninitialized or zeroed -- either is fine. If the
// return value is `Conf_save_start_ok`, then you must call either
// `conf_save_cancel()` or `conf_save_commit()`, otherwise file handles and
// strings will be leaked. If the return value is not `Conf_save_start_ok`,
// then the contents of `*p` are zeroed, and nothing further has to be called.
//
// `conf_file_name` should be a C string like "myprogram.conf"
//
// Note that `origfile` in the `struct Conf_save` may be null even if the call
// succeeded and didn't return an error. This is because it's possible for
// there to be no existing config file. It might be the first time a config
// file is being written.

void conf_save_cancel(Conf_save *p);
// Cancels a config save. Closes any file handles and frees any necessary
// strings. Calling with a zeroed `*p` is fine, but don't call it with
// uninitialized data. Afterwards, `*p` will be zeroed.

Conf_save_commit_error conf_save_commit(Conf_save *p);
// Finishes. Do not call this with a zeroed `*p`. Afterwards, `*p` will be
// zeroed.

// Just playing around with this design
typedef struct {
  FILE *file;
  Usz index;
  char *value;
  char buffer[1024];
} Ezconf_r;

void ezconf_r_start(Ezconf_r *ezcr, char const *conf_file_name);
bool ezconf_r_step(Ezconf_r *ezcr, char const *const *names, Usz nameslen);

typedef enum {
  Ezconf_w_ok = 0,
  Ezconf_w_bad_conf_name,
  Ezconf_w_oom,
  Ezconf_w_no_home,
  Ezconf_w_mkdir_failed,
  Ezconf_w_conf_dir_not_dir,
  Ezconf_w_old_temp_file_stuck,
  Ezconf_w_temp_file_perm_denied,
  Ezconf_w_temp_open_failed,
  Ezconf_w_temp_fsync_failed,
  Ezconf_w_temp_close_failed,
  Ezconf_w_rename_failed,
  Ezconf_w_line_too_long,
  Ezconf_w_existing_read_error,
  Ezconf_w_unknown_error,
} Ezconf_w_error;

char const *ezconf_w_errorstring(Ezconf_w_error error);

typedef struct {
  char const *name;
  intptr_t id;
  uint8_t flags;
} Ezconf_opt;

typedef struct {
  Conf_save save;
  Ezconf_opt *opts;
  size_t optscount, optscap;
  intptr_t optid;
  FILE *file;
  Ezconf_w_error error;
  uint32_t stateflags;
} Ezconf_w;

void ezconf_w_start(Ezconf_w *ezcw, Ezconf_opt *optsbuffer, size_t buffercap,
                    char const *conf_file_name);
void ezconf_w_addopt(Ezconf_w *ezcw, char const *key, intptr_t id);
bool ezconf_w_step(Ezconf_w *ezcw);
