#include "gbuffer.h"

void gbuffer_copy_subrect(Glyph *src, Glyph *dest, Usz src_height,
                          Usz src_width, Usz dest_height, Usz dest_width,
                          Usz src_y, Usz src_x, Usz dest_y, Usz dest_x,
                          Usz height, Usz width) {
  if (src_height <= src_y || src_width <= src_x || dest_height <= dest_y ||
      dest_width <= dest_x)
    return;
  Usz ny_0 = src_height - src_y;
  Usz ny_1 = dest_height - dest_y;
  Usz ny = height;
  if (ny_0 < ny)
    ny = ny_0;
  if (ny_1 < ny)
    ny = ny_1;
  if (ny == 0)
    return;
  Usz row_copy_0 = src_width - src_x;
  Usz row_copy_1 = dest_width - dest_x;
  Usz row_copy = width;
  if (row_copy_0 < row_copy)
    row_copy = row_copy_0;
  if (row_copy_1 < row_copy)
    row_copy = row_copy_1;
  Usz copy_bytes = row_copy * sizeof(Glyph);
  Glyph *src_p = src + src_y * src_width + src_x;
  Glyph *dest_p = dest + dest_y * dest_width + dest_x;
  Isz src_stride;
  Isz dest_stride;
  if (src_y >= dest_y) {
    src_stride = (Isz)src_width;
    dest_stride = (Isz)dest_width;
  } else {
    src_p += (ny - 1) * src_width;
    dest_p += (ny - 1) * dest_width;
    src_stride = -(Isz)src_width;
    dest_stride = -(Isz)dest_width;
  }
  Usz iy = 0;
  for (;;) {
    memmove(dest_p, src_p, copy_bytes);
    ++iy;
    if (iy == ny)
      break;
    src_p += src_stride;
    dest_p += dest_stride;
  }
}

void gbuffer_fill_subrect(Glyph *gbuffer, Usz f_height, Usz f_width, Usz y,
                          Usz x, Usz height, Usz width, Glyph fill_char) {
  if (y >= f_height || x >= f_width)
    return;
  Usz rows_0 = f_height - y;
  Usz rows = height;
  if (rows_0 < rows)
    rows = rows_0;
  if (rows == 0)
    return;
  Usz columns_0 = f_width - x;
  Usz columns = width;
  if (columns_0 < columns)
    columns = columns_0;
  Usz fill_bytes = columns * sizeof(Glyph);
  Glyph *p = gbuffer + y * f_width + x;
  Usz iy = 0;
  for (;;) {
    memset(p, fill_char, fill_bytes);
    ++iy;
    if (iy == rows)
      break;
    p += f_width;
  }
}

void mbuffer_clear(Mark *mbuf, Usz height, Usz width) {
  Usz cleared_size = height * width;
  memset(mbuf, 0, cleared_size);
}
