#include "sysmisc.h"
#include "gbuffer.h"
#include "thirdparty/oso.h"
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#ifndef WIN32

static char const *const xdg_config_home_env = "XDG_CONFIG_HOME";
static char const *const home_env = "HOME";

void expand_home_tilde(oso **path) {
  oso *s = *path;
  size_t n = osolen(s);
  if (n < 2)
    return;
  if (osoc(s)[0] != '~' || osoc(s)[1] != '/')
    return;
  char const *homedir = getenv(home_env);
  if (!homedir)
    return;
  size_t hlen = strlen(homedir);
  osoensurecap(&s, n + hlen - 1);
  if (!s)
    goto done;
  memmove((char *)s + hlen, (char *)s + 1, n); // includes '\0'
  memcpy((char *)s, homedir, hlen);
  osopokelen(s, n + hlen - 1);
done:
  *path = s;
}

ORCA_NOINLINE
Cboard_error cboard_copy(Glyph const *gbuffer, Usz field_height,
                         Usz field_width, Usz rect_y, Usz rect_x, Usz rect_h,
                         Usz rect_w) {
#ifndef WIN32
  (void)field_height;
  FILE *fp =
#ifdef ORCA_OS_MAC
      popen("pbcopy -pboard general 2>/dev/null", "w");
#else
      popen("xclip -i -selection clipboard 2>/dev/null", "w");
#endif
  if (!fp)
    return Cboard_error_popen_failed;
  for (Usz iy = 0; iy < rect_h; iy++) {
    Glyph const *row = gbuffer + (rect_y + iy) * field_width + rect_x;
    fwrite(row, sizeof(Glyph), rect_w, fp);
    if (iy + 1 < rect_h)
      fputc('\n', fp);
  }
  int status = pclose(fp);
  return status ? Cboard_error_process_exit_error : Cboard_error_none;
#endif
}

ORCA_NOINLINE
Cboard_error cboard_paste(Glyph *gbuffer, Usz height, Usz width, Usz y, Usz x,
                          Usz *out_h, Usz *out_w) {
#ifndef WIN32
  FILE *fp =
#ifdef ORCA_OS_MAC
      popen("pbpaste -pboard general -Prefer txt 2>/dev/null", "r");
#else
      popen("xclip -o -selection clipboard 2>/dev/null", "r");
#endif
  Usz start_y = y, start_x = x, max_y = y, max_x = x;
  if (!fp)
    return Cboard_error_popen_failed;
  char inbuff[512];
  for (;;) {
    size_t n = fread(inbuff, 1, sizeof inbuff, fp);
    for (size_t i = 0; i < n; i++) {
      char c = inbuff[i];
      if (c == '\r' || c == '\n') {
        y++;
        x = start_x;
        continue;
      }
      if (c != ' ' && y < height && x < width) {
        Glyph g = orca_is_valid_glyph(c) ? (Glyph)c : '.';
        gbuffer_poke(gbuffer, height, width, y, x, g);
        if (x > max_x)
          max_x = x;
        if (y > max_y)
          max_y = y;
      }
      x++;
    }
    if (n < sizeof inbuff)
      break;
  }
  int status = pclose(fp);
  *out_h = max_y - start_y + 1;
  *out_w = max_x - start_x + 1;
  return status ? Cboard_error_process_exit_error : Cboard_error_none;
#endif
}

ORCA_NOINLINE
Conf_read_result conf_read_line(FILE *file, char *buf, Usz bufsize,
                                char **out_left, Usz *out_leftsize,
                                char **out_right, Usz *out_rightsize) {
  // a0 and a1 are the start and end positions of the left side of an "foo=bar"
  // pair. b0 and b1 are the positions right side. Leading and trailing spaces
  // will be removed.
  Usz len, a0, a1, b0, b1;
  char *s;
  if (bufsize < 2)
    goto insufficient_buffer;
#if SIZE_MAX > INT_MAX
  if (bufsize > (Usz)INT_MAX)
    exit(1); // he boot too big
#endif
  s = fgets(buf, (int)bufsize, file);
  if (!s) {
    if (feof(file))
      goto eof;
    goto ioerror;
  }
  len = strlen(buf);
  if (len == bufsize - 1 && buf[len - 1] != '\n' && !feof(file))
    goto insufficient_buffer;
  a0 = 0;
  for (;;) { // scan for first non-space in "   foo=bar"
    if (a0 == len)
      goto ignore;
    char c = s[a0];
    if (c == ';' || c == '#') // comment line, ignore
      goto ignore;
    if (c == '=') // '=' before any other char, bad
      goto ignore;
    if (!isspace(c))
      break;
    a0++;
  }
  a1 = a0;
  for (;;) { // scan for '='
    a1++;
    if (a1 == len)
      goto ignore;
    char c = s[a1];
    Usz x = a1; // don't include any whitespace preceeding the '='
    while (isspace(c)) {
      x++;
      if (x == len)
        goto ignore;
      c = s[x];
    }
    if (c == '=') {
      b0 = x;
      break;
    }
    a1 = x;
  }
  for (;;) { // scan for first non-whitespace after '='
    b0++;
    if (b0 == len) { // empty right side, but still valid pair
      b1 = b0;
      goto ok;
    }
    char c = s[b0];
    if (!isspace(c))
      break;
  }
  b1 = b0;
  for (;;) { // scan for end of useful stuff for right-side value
    b1++;
    if (b1 == len)
      goto ok;
    char c = s[b1];
    Usz x = b1; // don't include any whitespace preceeding the EOL
    while (isspace(c)) {
      x++;
      if (x == len)
        goto ok;
      c = s[x];
    }
    b1 = x;
  }
  Conf_read_result err;
insufficient_buffer:
  err = Conf_read_buffer_too_small;
  goto fail;
eof:
  err = Conf_read_eof;
  goto fail;
ioerror:
  err = Conf_read_io_error;
  goto fail;
fail:
  *out_left = NULL;
  *out_leftsize = 0;
  goto null_right;
ignore:
  s[len - 1] = '\0';
  *out_left = s;
  *out_leftsize = len;
  err = Conf_read_irrelevant;
  goto null_right;
null_right:
  *out_right = NULL;
  *out_rightsize = 0;
  return err;
ok:
  s[a1] = '\0';
  s[b1] = '\0';
  *out_left = s + a0;
  *out_leftsize = a1 - a0;
  *out_right = s + b0;
  *out_rightsize = b1 - b0;
  return Conf_read_left_and_right;
}

bool conf_read_match(FILE **pfile, char const *const *names, Usz nameslen,
                     char *buf, Usz bufsize, Usz *out_index, char **out_value) {
  FILE *file = *pfile;
  if (!file)
    return false;
  char *left;
  Usz leftsz, rightsz;
next_line:;
  Conf_read_result res =
      conf_read_line(file, buf, bufsize, &left, &leftsz, out_value, &rightsz);
  switch (res) {
  case Conf_read_left_and_right:
    for (Usz i = 0; i < nameslen; i++) {
      if (strcmp(names[i], left) == 0) {
        *out_index = i;
        return true;
      }
    }
    goto next_line;
  case Conf_read_irrelevant:
    goto next_line;
  case Conf_read_buffer_too_small:
  case Conf_read_eof:
  case Conf_read_io_error:
    break;
  }
  fclose(file);
  *pfile = NULL;
  return false;
}

typedef enum {
  Conf_dir_ok = 0,
  Conf_dir_no_home,
} Conf_dir_error;

static Conf_dir_error try_get_conf_dir(oso **out) {
  char const *xdgcfgdir = getenv(xdg_config_home_env);
  if (xdgcfgdir) {
    Usz xdgcfgdirlen = strlen(xdgcfgdir);
    if (xdgcfgdirlen > 0) {
      osoputlen(out, xdgcfgdir, xdgcfgdirlen);
      return Conf_dir_ok;
    }
  }
  char const *homedir = getenv(home_env);
  if (homedir) {
    Usz homedirlen = strlen(homedir);
    if (homedirlen > 0) {
      osoputprintf(out, "%s/.config", homedir);
      return Conf_dir_ok;
    }
  }
  return Conf_dir_no_home;
}

static void conf_impl_catconfpath(oso **p, char const *conf_file_name,
                                  size_t conflen) {
  oso *path = *p;
  size_t n = osolen(path);
  osoensurecap(&path, n + 1 + conflen);
  if (!path)
    goto done;
  ((char *)path)[n] = '/';
  memcpy((char *)path + n + 1, conf_file_name, conflen);
  ((char *)path)[n + 1 + conflen] = '\0';
  osopokelen(path, n + 1 + conflen);
done:
  *p = path;
}

FILE *conf_file_open_for_reading(char const *conf_file_name) {
  if (!conf_file_name)
    return NULL;
  oso *path = NULL;
  if (try_get_conf_dir(&path))
    return NULL;
  size_t conflen = strlen(conf_file_name);
  if (conflen == 0)
    return NULL;
  conf_impl_catconfpath(&path, conf_file_name, conflen);
  if (!path)
    return NULL;
  FILE *file = fopen(osoc(path), "r");
  osofree(path);
  return file;
}

Conf_save_start_error conf_save_start(Conf_save *p,
                                      char const *conf_file_name) {
  *p = (Conf_save){0};
  oso *dir = NULL;
  Conf_save_start_error err;
  if (!conf_file_name) {
    err = Conf_save_start_bad_conf_name;
    goto cleanup;
  }
  if (try_get_conf_dir(&dir)) {
    err = Conf_save_start_no_home;
    goto cleanup;
  }
  if (!dir)
    goto allocfail;
  osoputoso(&p->canonpath, dir);
  if (!p->canonpath)
    goto allocfail;
  size_t namelen = strlen(conf_file_name);
  if (namelen == 0) {
    err = Conf_save_start_bad_conf_name;
    goto cleanup;
  }
  conf_impl_catconfpath(&p->canonpath, conf_file_name, namelen);
  if (!p->canonpath)
    goto allocfail;
  osoputoso(&p->temppath, p->canonpath);
  if (!p->temppath)
    goto allocfail;
  osocat(&p->temppath, ".tmp");
  if (!p->temppath)
    goto allocfail;
  // Remove old temp file if it exists. If it exists and we can't remove it,
  // error.
  if (unlink(osoc(p->temppath)) == -1 && errno != ENOENT) {
    switch (errno) {
    case ENOTDIR:
      err = Conf_save_start_conf_dir_not_dir;
      break;
    case EACCES:
      err = Conf_save_start_temp_file_perm_denied;
      break;
    default:
      err = Conf_save_start_old_temp_file_stuck;
      break;
    }
    goto cleanup;
  }
  p->tempfile = fopen(osoc(p->temppath), "w");
  if (!p->tempfile) {
    // Try to create config dir, in case it doesn't exist. (XDG says we should
    // do this, and use mode 0700.)
    mkdir(osoc(dir), 0700);
    p->tempfile = fopen(osoc(p->temppath), "w");
  }
  if (!p->tempfile) {
    err = Conf_save_start_temp_file_open_failed;
    goto cleanup;
  }
  // This may be left as NULL.
  p->origfile = fopen(osoc(p->canonpath), "r");
  // We did it, boys.
  osofree(dir);
  return Conf_save_start_ok;

allocfail:
  err = Conf_save_start_alloc_failed;
cleanup:
  osofree(dir);
  conf_save_cancel(p);
  return err;
}

void conf_save_cancel(Conf_save *p) {
  osofree(p->canonpath);
  osofree(p->temppath);
  if (p->origfile)
    fclose(p->origfile);
  if (p->tempfile)
    fclose(p->tempfile);
  *p = (Conf_save){0};
}

Conf_save_commit_error conf_save_commit(Conf_save *p) {
  Conf_save_commit_error err;
  fclose(p->tempfile);
  p->tempfile = NULL;
  if (p->origfile) {
    fclose(p->origfile);
    p->origfile = NULL;
  }
  // This isn't really atomic. But if we want to close and move a file
  // simultaneously, I think we have to use OS-specific facilities. So I guess
  // this is the best we can do for now. I could be wrong, though. But I
  // couldn't find any good information about it.
  if (rename(osoc(p->temppath), osoc(p->canonpath)) == -1) {
    err = Conf_save_commit_rename_failed;
    goto cleanup;
  }
  err = Conf_save_commit_ok;
cleanup:
  conf_save_cancel(p);
  return err;
}

char const *ezconf_w_errorstring(Ezconf_w_error error) {
  switch (error) {
  case Ezconf_w_ok:
    return "No error";
  case Ezconf_w_bad_conf_name:
    return "Bad config file name";
  case Ezconf_w_oom:
    return "Out of memory";
  case Ezconf_w_no_home:
    return "Unable to resolve $XDG_CONFIG_HOME or $HOME";
  case Ezconf_w_mkdir_failed:
    return "Unable to create $XDG_CONFIG_HOME or $HOME/.config directory";
  case Ezconf_w_conf_dir_not_dir:
    return "Config directory path is not a directory";
  case Ezconf_w_old_temp_file_stuck:
    return "Unable to remove old .conf.tmp file";
  case Ezconf_w_temp_file_perm_denied:
    return "Permission denied for config directory";
  case Ezconf_w_temp_open_failed:
    return "Unable to open .conf.tmp for writing";
  case Ezconf_w_temp_fsync_failed:
    return "fsync() reported an when writing temp file.\n"
           "Refusing to continue.";
  case Ezconf_w_temp_close_failed:
    return "Unable to close temp file";
  case Ezconf_w_rename_failed:
    return "Unable to rename .conf.tmp to .conf";
  case Ezconf_w_line_too_long:
    return "Line in file is too long";
  case Ezconf_w_existing_read_error:
    return "Error when reading existing configuration file";
  case Ezconf_w_unknown_error:
    break;
  }
  return "Unknown";
}

void ezconf_r_start(Ezconf_r *ezcr, char const *conf_file_name) {
  ezcr->file = conf_file_open_for_reading(conf_file_name);
  ezcr->index = 0;
  ezcr->value = NULL;
}

bool ezconf_r_step(Ezconf_r *ezcr, char const *const *names, size_t nameslen) {
  return conf_read_match(&ezcr->file, names, nameslen, ezcr->buffer,
                         sizeof ezcr->buffer, &ezcr->index, &ezcr->value);
}

enum {
  Confwflag_add_newline = 1 << 0,
  Ezconf_opt_written = 1 << 0,
};

void ezconf_w_start(Ezconf_w *ezcw, Ezconf_opt *optsbuffer, size_t buffercap,
                    char const *conf_file_name) {
  *ezcw = (Ezconf_w){.save = {0}}; // Weird to silence clang warning
  ezcw->opts = optsbuffer;
  ezcw->optscap = buffercap;
  Ezconf_w_error error = Ezconf_w_unknown_error;
  switch (conf_save_start(&ezcw->save, conf_file_name)) {
  case Conf_save_start_ok:
    error = Ezconf_w_ok;
    ezcw->file = ezcw->save.tempfile;
    break;
  case Conf_save_start_bad_conf_name:
    error = Ezconf_w_bad_conf_name;
    break;
  case Conf_save_start_alloc_failed:
    error = Ezconf_w_oom;
    break;
  case Conf_save_start_no_home:
    error = Ezconf_w_no_home;
    break;
  case Conf_save_start_mkdir_failed:
    error = Ezconf_w_mkdir_failed;
    break;
  case Conf_save_start_conf_dir_not_dir:
    error = Ezconf_w_conf_dir_not_dir;
    break;
  case Conf_save_start_old_temp_file_stuck:
    error = Ezconf_w_old_temp_file_stuck;
    break;
  case Conf_save_start_temp_file_perm_denied:
    error = Ezconf_w_temp_file_perm_denied;
    break;
  case Conf_save_start_temp_file_open_failed:
    error = Ezconf_w_temp_open_failed;
    break;
  }
  ezcw->error = error;
}
void ezconf_w_addopt(Ezconf_w *ezcw, char const *key, intptr_t id) {
  size_t count = ezcw->optscount, cap = ezcw->optscap;
  if (count == cap)
    return;
  ezcw->opts[count] = (Ezconf_opt){.name = key, .id = id, .flags = 0};
  ezcw->optscount = count + 1;
}
bool ezconf_w_step(Ezconf_w *ezcw) {
  uint32_t stateflags = ezcw->stateflags;
  FILE *origfile = ezcw->save.origfile, *tempfile = ezcw->save.tempfile;
  Ezconf_opt *opts = ezcw->opts, *chosen = NULL;
  size_t optscount = ezcw->optscount;
  if (ezcw->error || !tempfile) // Already errored or finished ok
    return false;
  // If we set a flag to write a closing newline the last time we were called,
  // write it now.
  if (stateflags & Confwflag_add_newline) {
    fputs("\n", tempfile);
    stateflags &= ~(uint32_t)Confwflag_add_newline;
  }
  if (!optscount)
    goto commit;
  if (!origfile)
    goto write_leftovers;
  for (;;) { // Scan through file looking for known keys in key=value lines
    char linebuff[1024];
    char *left, *right;
    size_t leftsz, rightsz;
    Conf_read_result res = conf_read_line(origfile, linebuff, sizeof linebuff,
                                          &left, &leftsz, &right, &rightsz);
    switch (res) {
    case Conf_read_left_and_right: {
      for (size_t i = 0; i < optscount; i++) {
        char const *name = opts[i].name;
        if (!name)
          continue;
        if (strcmp(name, left) != 0)
          continue;
        // If we already wrote this one, comment out the line instead, and move
        // on to the next line.
        if (opts[i].flags & (uint8_t)Ezconf_opt_written) {
          fputs("# ", tempfile);
          goto write_landr;
        }
        chosen = opts + i;
        goto return_for_writing;
      }
    write_landr:
      fputs(left, tempfile);
      fputs(" = ", tempfile);
      fputs(right, tempfile);
      fputs("\n", tempfile);
      continue;
    }
    case Conf_read_irrelevant:
      fputs(left, tempfile);
      fputs("\n", tempfile);
      continue;
    case Conf_read_eof:
      goto end_original;
    case Conf_read_buffer_too_small:
      ezcw->error = Ezconf_w_line_too_long;
      goto cancel;
    case Conf_read_io_error:
      ezcw->error = Ezconf_w_existing_read_error;
      goto cancel;
    }
  }
end_original: // Don't need original file anymore
  fclose(origfile);
  ezcw->save.origfile = origfile = NULL;
write_leftovers: // Write out any guys that weren't in original file.
  for (;;) {     // Find the first guy that wasn't already written.
    if (!optscount)
      goto commit;
    chosen = opts;
    // Drop the guy from the front of the list. This is to reduce super-linear
    // complexity growth as the number of conf key-value pairs are increased.
    // (Otherwise, we iterate the full set of guys on each call during the
    // "write the leftovers" phase.)
    opts++;
    optscount--;
    if (!(chosen->flags & (uint8_t)Ezconf_opt_written))
      break;
  }
  // Once control has reached here, we're going to return true to the caller.
  // Which means we expect to be called at least one more time. So update the
  // pointers stored in the persistent state, so that we don't have to scan
  // through as much of this list next time. (This might even end up finishing
  // it off, making it empty.)
  ezcw->opts = opts;
  ezcw->optscount = optscount;
return_for_writing:
  chosen->flags |= (uint8_t)Ezconf_opt_written;
  fputs(chosen->name, tempfile);
  fputs(" = ", tempfile);
  ezcw->optid = chosen->id;
  stateflags |= (uint32_t)Confwflag_add_newline;
  ezcw->stateflags = stateflags;
  return true;
cancel:
  conf_save_cancel(&ezcw->save);
  // ^- Sets tempfile to null, which we use as a guard at the top of this
  //    function.
  ezcw->file = NULL;
  ezcw->stateflags = 0;
  return false;
commit:;
  Ezconf_w_error error = Ezconf_w_unknown_error;
  switch (conf_save_commit(&ezcw->save)) {
  case Conf_save_commit_ok:
    error = Ezconf_w_ok;
    break;
  case Conf_save_commit_temp_fsync_failed:
    error = Ezconf_w_temp_fsync_failed;
    break;
  case Conf_save_commit_temp_close_failed:
    error = Ezconf_w_temp_close_failed;
    break;
  case Conf_save_commit_rename_failed:
    error = Ezconf_w_rename_failed;
    break;
  }
  ezcw->file = NULL;
  ezcw->error = error;
  ezcw->stateflags = 0;
  return false;
}

#endif
