#include "field.h"
#include "gbuffer.h"
#include <ctype.h>

void field_init(Field *f) {
  f->buffer = NULL;
  f->height = 0;
  f->width = 0;
}

void field_init_fill(Field *f, Usz height, Usz width, Glyph fill_char) {
  assert(height <= ORCA_Y_MAX && width <= ORCA_X_MAX);
  Usz num_cells = height * width;
  f->buffer = malloc(num_cells * sizeof(Glyph));
  memset(f->buffer, fill_char, num_cells);
  f->height = (U16)height;
  f->width = (U16)width;
}

void field_deinit(Field *f) { free(f->buffer); }

void field_resize_raw(Field *f, Usz height, Usz width) {
  assert(height <= ORCA_Y_MAX && width <= ORCA_X_MAX);
  Usz cells = height * width;
  f->buffer = realloc(f->buffer, cells * sizeof(Glyph));
  f->height = (U16)height;
  f->width = (U16)width;
}

void field_resize_raw_if_necessary(Field *field, Usz height, Usz width) {
  if (field->height != height || field->width != width) {
    field_resize_raw(field, height, width);
  }
}

void field_copy(Field *src, Field *dest) {
  field_resize_raw_if_necessary(dest, src->height, src->width);
  gbuffer_copy_subrect(src->buffer, dest->buffer, src->height, src->width,
                       dest->height, dest->width, 0, 0, 0, 0, src->height,
                       src->width);
}

static inline bool glyph_char_is_valid(char c) { return c >= '!' && c <= '~'; }

void field_fput(Field *f, FILE *stream) {
  enum { Column_buffer_count = 4096 };
  char out_buffer[Column_buffer_count];
  Usz f_height = f->height;
  Usz f_width = f->width;
  Glyph *f_buffer = f->buffer;
  if (f_width > Column_buffer_count - 2)
    return;
  for (Usz iy = 0; iy < f_height; ++iy) {
    Glyph *row_p = f_buffer + f_width * iy;
    for (Usz ix = 0; ix < f_width; ++ix) {
      char c = row_p[ix];
      out_buffer[ix] = glyph_char_is_valid(c) ? c : '?';
    }
    out_buffer[f_width] = '\n';
    out_buffer[f_width + 1] = '\0';
    fputs(out_buffer, stream);
  }
}

Field_load_error field_load_file(char const *filepath, Field *field) {
  FILE *file = fopen(filepath, "r");
  if (file == NULL) {
    return Field_load_error_cant_open_file;
  }
  enum { Bufsize = 4096 };
  char buf[Bufsize];
  Usz first_row_columns = 0;
  Usz rows = 0;
  for (;;) {
    char *s = fgets(buf, Bufsize, file);
    if (s == NULL)
      break;
    if (rows == ORCA_Y_MAX) {
      fclose(file);
      return Field_load_error_too_many_rows;
    }
    Usz len = strlen(buf);
    if (len == Bufsize - 1 && buf[len - 1] != '\n' && !feof(file)) {
      fclose(file);
      return Field_load_error_too_many_columns;
    }
    for (;;) {
      if (len == 0)
        break;
      if (!isspace(buf[len - 1]))
        break;
      --len;
    }
    if (len == 0)
      continue;
    if (len >= ORCA_X_MAX) {
      fclose(file);
      return Field_load_error_too_many_columns;
    }
    // quick hack until we use a proper scanner
    if (rows == 0) {
      first_row_columns = len;
    } else if (len != first_row_columns) {
      fclose(file);
      return Field_load_error_not_a_rectangle;
    }
    field_resize_raw(field, rows + 1, first_row_columns);
    Glyph *rowbuff = field->buffer + first_row_columns * rows;
    for (Usz i = 0; i < len; ++i) {
      char c = buf[i];
      rowbuff[i] = glyph_char_is_valid(c) ? c : '.';
    }
    ++rows;
  }
  fclose(file);
  return Field_load_error_ok;
}

char const *field_load_error_string(Field_load_error fle) {
  char const *errstr = "Unknown";
  switch (fle) {
  case Field_load_error_ok:
    errstr = "OK";
    break;
  case Field_load_error_cant_open_file:
    errstr = "Unable to open file";
    break;
  case Field_load_error_too_many_columns:
    errstr = "Grid file has too many columns";
    break;
  case Field_load_error_too_many_rows:
    errstr = "Grid file has too many rows";
    break;
  case Field_load_error_no_rows_read:
    errstr = "Grid file has no rows";
    break;
  case Field_load_error_not_a_rectangle:
    errstr = "Grid file is not a rectangle";
    break;
  }
  return errstr;
}

void mbuf_reusable_init(Mbuf_reusable *mbr) {
  mbr->buffer = NULL;
  mbr->capacity = 0;
}

void mbuf_reusable_ensure_size(Mbuf_reusable *mbr, Usz height, Usz width) {
  Usz capacity = height * width;
  if (mbr->capacity < capacity) {
    mbr->buffer = realloc(mbr->buffer, capacity);
    mbr->capacity = capacity;
  }
}

void mbuf_reusable_deinit(Mbuf_reusable *mbr) { free(mbr->buffer); }
