// Clip Library
// Copyright (c) 2015-2018 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "clip.h"
#include "clip_lock_impl.h"

#include <cassert>
#include <vector>
#include <windows.h>

#ifndef LCS_WINDOWS_COLOR_SPACE
#define LCS_WINDOWS_COLOR_SPACE 'Win '
#endif

#ifndef CF_DIBV5
#define CF_DIBV5            17
#endif

#pragma warning(disable: 4267)
namespace clip {

namespace {

unsigned long get_shift_from_mask(unsigned long mask) {
  unsigned long shift = 0;
  for (shift=0; shift<sizeof(unsigned long)*8; ++shift)
    if (mask & (1 << shift))
      return shift;
  return shift;
}

class Hglobal {
public:
  Hglobal() : m_handle(nullptr) {
  }

  explicit Hglobal(HGLOBAL handle) : m_handle(handle) {
  }

  explicit Hglobal(size_t len) : m_handle(GlobalAlloc(GHND, len)) {
  }

  ~Hglobal() {
    if (m_handle)
      GlobalFree(m_handle);
  }

  void release() {
    m_handle = nullptr;
  }

  operator HGLOBAL() {
    return m_handle;
  }

private:
  HGLOBAL m_handle;
};

}

lock::impl::impl(void* hwnd) : m_locked(false) {
  for (int i=0; i<5; ++i) {
    if (OpenClipboard((HWND)hwnd)) {
      m_locked = true;
      break;
    }
    Sleep(20);
  }

  if (!m_locked) {
    error_handler e = get_error_handler();
    if (e)
      e(ErrorCode::CannotLock);
  }
}

lock::impl::~impl() {
  if (m_locked)
    CloseClipboard();
}

bool lock::impl::clear() {
  return (EmptyClipboard() ? true: false);
}

bool lock::impl::is_convertible(format f) const {
  if (f == text_format()) {
    return
      (IsClipboardFormatAvailable(CF_TEXT) ||
       IsClipboardFormatAvailable(CF_UNICODETEXT) ||
       IsClipboardFormatAvailable(CF_OEMTEXT));
  }
  else if (f == image_format()) {
    return (IsClipboardFormatAvailable(CF_DIB) ? true: false);
  }
  else if (IsClipboardFormatAvailable(f))
    return true;
  else
    return false;
}

bool lock::impl::set_data(format f, const char* buf, size_t len) {
  bool result = false;

  if (f == text_format()) {
    if (len > 0) {
      int reqsize = MultiByteToWideChar(CP_UTF8, 0, buf, len, NULL, 0);
      if (reqsize > 0) {
        ++reqsize;

        Hglobal hglobal(sizeof(WCHAR)*reqsize);
        LPWSTR lpstr = static_cast<LPWSTR>(GlobalLock(hglobal));
        MultiByteToWideChar(CP_UTF8, 0, buf, len, lpstr, reqsize);
        GlobalUnlock(hglobal);

        result = (SetClipboardData(CF_UNICODETEXT, hglobal)) ? true: false;
        if (result)
          hglobal.release();
      }
    }
  }
  else {
    Hglobal hglobal(len+sizeof(size_t));
    if (hglobal) {
      size_t* dst = (size_t*)GlobalLock(hglobal);
      if (dst) {
        *dst = len;
        memcpy(dst+1, buf, len);
        GlobalUnlock(hglobal);
        result = (SetClipboardData(f, hglobal) ? true: false);
        if (result)
          hglobal.release();
      }
    }
  }

  return result;
}

bool lock::impl::get_data(format f, char* buf, size_t len) const {
  assert(buf);

  if (!buf || !is_convertible(f))
    return false;

  bool result = false;

  if (f == text_format()) {
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
      HGLOBAL hglobal = GetClipboardData(CF_UNICODETEXT);
      if (hglobal) {
        LPWSTR lpstr = static_cast<LPWSTR>(GlobalLock(hglobal));
        if (lpstr) {
          size_t reqsize =
            WideCharToMultiByte(CP_UTF8, 0, lpstr, -1,
                                nullptr, 0, nullptr, nullptr);

          assert(reqsize <= len);
          if (reqsize <= len) {
            WideCharToMultiByte(CP_UTF8, 0, lpstr, -1,
                                buf, reqsize, nullptr, nullptr);
            result = true;
          }
          GlobalUnlock(hglobal);
        }
      }
    }
    else if (IsClipboardFormatAvailable(CF_TEXT)) {
      HGLOBAL hglobal = GetClipboardData(CF_TEXT);
      if (hglobal) {
        LPSTR lpstr = static_cast<LPSTR>(GlobalLock(hglobal));
        if (lpstr) {
          // TODO check length
          memcpy(buf, lpstr, len);
          result = true;
          GlobalUnlock(hglobal);
        }
      }
    }
  }
  else {
    if (IsClipboardFormatAvailable(f)) {
      HGLOBAL hglobal = GetClipboardData(f);
      if (hglobal) {
        const size_t* ptr = (const size_t*)GlobalLock(hglobal);
        if (ptr) {
          size_t reqsize = *ptr;
          assert(reqsize <= len);
          if (reqsize <= len) {
            memcpy(buf, ptr+1, reqsize);
            result = true;
          }
          GlobalUnlock(hglobal);
        }
      }
    }
  }

  return result;
}

size_t lock::impl::get_data_length(format f) const {
  size_t len = 0;

  if (f == text_format()) {
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
      HGLOBAL hglobal = GetClipboardData(CF_UNICODETEXT);
      if (hglobal) {
        LPWSTR lpstr = static_cast<LPWSTR>(GlobalLock(hglobal));
        if (lpstr) {
          len =
            WideCharToMultiByte(CP_UTF8, 0, lpstr, -1,
                                nullptr, 0, nullptr, nullptr);
          GlobalUnlock(hglobal);
        }
      }
    }
    else if (IsClipboardFormatAvailable(CF_TEXT)) {
      HGLOBAL hglobal = GetClipboardData(CF_TEXT);
      if (hglobal) {
        LPSTR lpstr = (LPSTR)GlobalLock(hglobal);
        if (lpstr) {
          len = strlen(lpstr) + 1;
          GlobalUnlock(hglobal);
        }
      }
    }
  }
  // TODO check if it's a registered custom format
  else if (f != empty_format()) {
    if (IsClipboardFormatAvailable(f)) {
      HGLOBAL hglobal = GetClipboardData(f);
      if (hglobal) {
        const size_t* ptr = (const size_t*)GlobalLock(hglobal);
        if (ptr) {
          len = *ptr;
          GlobalUnlock(hglobal);
        }
      }
    }
  }

  return len;
}

bool lock::impl::set_image(const image& image) {
  const image_spec& spec = image.spec();
  image_spec out_spec = spec;

  int palette_colors = 0;
  int padding = 0;
  switch (spec.bits_per_pixel) {
    case 24: padding = (4-((spec.width*3)&3))&3; break;
    case 16: padding = ((4-((spec.width*2)&3))&3)/2; break;
    case 8:  padding = (4-(spec.width&3))&3; break;
  }
  out_spec.bytes_per_row += padding;

  // Create the BITMAPV5HEADER structure
  Hglobal hmem(
    GlobalAlloc(
      GHND,
      sizeof(BITMAPV5HEADER)
      + palette_colors*sizeof(RGBQUAD)
      + out_spec.bytes_per_row*out_spec.height));
  if (!hmem)
    return false;

  out_spec.red_mask    = 0x00ff0000;
  out_spec.green_mask  = 0xff00;
  out_spec.blue_mask   = 0xff;
  out_spec.alpha_mask  = 0xff000000;
  out_spec.red_shift   = 16;
  out_spec.green_shift = 8;
  out_spec.blue_shift  = 0;
  out_spec.alpha_shift = 24;

  BITMAPV5HEADER* bi = (BITMAPV5HEADER*)GlobalLock(hmem);
  bi->bV5Size = sizeof(BITMAPV5HEADER);
  bi->bV5Width = out_spec.width;
  bi->bV5Height = out_spec.height;
  bi->bV5Planes = 1;
  bi->bV5BitCount = (WORD)out_spec.bits_per_pixel;
  bi->bV5Compression = BI_RGB;
  bi->bV5SizeImage = out_spec.bytes_per_row*spec.height;
  bi->bV5RedMask   = out_spec.red_mask;
  bi->bV5GreenMask = out_spec.green_mask;
  bi->bV5BlueMask  = out_spec.blue_mask;
  bi->bV5AlphaMask = out_spec.alpha_mask;
  bi->bV5CSType = LCS_WINDOWS_COLOR_SPACE;
  bi->bV5Intent = LCS_GM_GRAPHICS;
  bi->bV5ClrUsed = 0;

  switch (spec.bits_per_pixel) {
    case 32: {
      const char* src = image.data();
      char* dst = (((char*)bi)+bi->bV5Size)
        + (out_spec.height-1)*out_spec.bytes_per_row;
      for (long y=spec.height-1; y>=0; --y) {
        // std::copy(src, src+spec.bytes_per_row, dst);
        const uint32_t* src_x = (const uint32_t*)src;
        uint32_t* dst_x = (uint32_t*)dst;

        for (unsigned long x=0; x<spec.width; ++x, ++src_x, ++dst_x) {
          uint32_t c = *src_x;
          int r = ((c & spec.red_mask  ) >> spec.red_shift  );
          int g = ((c & spec.green_mask) >> spec.green_shift);
          int b = ((c & spec.blue_mask ) >> spec.blue_shift );
          int a = ((c & spec.alpha_mask) >> spec.alpha_shift);

          // Windows requires premultiplied RGBA values
          r = r * a / 255;
          g = g * a / 255;
          b = b * a / 255;

          *dst_x =
            (r << out_spec.red_shift  ) |
            (g << out_spec.green_shift) |
            (b << out_spec.blue_shift ) |
            (a << out_spec.alpha_shift);
        }

        src += spec.bytes_per_row;
        dst -= out_spec.bytes_per_row;
      }
      break;
    }
    default:
      error_handler e = get_error_handler();
      if (e)
        e(ErrorCode::ImageNotSupported);
      return false;
  }

  GlobalUnlock(hmem);
  SetClipboardData(CF_DIBV5, hmem);
  return true;
}

bool lock::impl::get_image(image& output_img) const {
  image_spec spec;
  if (!get_image_spec(spec))
    return false;

  BITMAPINFO* bi = (BITMAPINFO*)GetClipboardData(CF_DIB);
  if (!bi)
    return false;

  if (bi->bmiHeader.biCompression != BI_RGB &&
      bi->bmiHeader.biCompression != BI_BITFIELDS) {
    error_handler e = get_error_handler();
    if (e)
      e(ErrorCode::ImageNotSupported);
    return false;
  }

  image img(spec);

  switch (bi->bmiHeader.biBitCount) {

    case 32:
    case 24:
    case 16: {
      const int src_bytes_per_row = spec.width*((bi->bmiHeader.biBitCount+7)/8);
      const int padding = (4-(src_bytes_per_row&3))&3;
      const char* src = (((char*)bi)+bi->bmiHeader.biSize);

      if (bi->bmiHeader.biCompression == BI_BITFIELDS) {
        src += sizeof(RGBQUAD)*3;

        for (long y=spec.height-1; y>=0; --y, src+=src_bytes_per_row+padding) {
          char* dst = img.data()+y*spec.bytes_per_row;
          std::copy(src, src+src_bytes_per_row, dst);
        }
      }
      else if (bi->bmiHeader.biCompression == BI_RGB) {
        for (long y=spec.height-1; y>=0; --y, src+=src_bytes_per_row+padding) {
          char* dst = img.data()+y*spec.bytes_per_row;
          std::copy(src, src+src_bytes_per_row, dst);
        }
      }

      // Windows uses premultiplied RGB values, and we use straight
      // alpha. So we have to divide all RGB values by its alpha.
      if (bi->bmiHeader.biBitCount == 32 &&
          spec.alpha_mask) {
        bool hasAlphaGreaterThanZero = false;
        bool hasValidPremultipliedAlpha = true;

        for (unsigned long y=0; y<spec.height; ++y) {
          const uint32_t* dst = (uint32_t*)(img.data()+y*spec.bytes_per_row);
          for (unsigned long x=0; x<spec.width; ++x, ++dst) {
            const uint32_t c = *dst;
            const int r = ((c & spec.red_mask  ) >> spec.red_shift  );
            const int g = ((c & spec.green_mask) >> spec.green_shift);
            const int b = ((c & spec.blue_mask ) >> spec.blue_shift );
            const int a = ((c & spec.alpha_mask) >> spec.alpha_shift);

            if (a > 0)
              hasAlphaGreaterThanZero = true;
            if (r > a || g > a || b > a)
              hasValidPremultipliedAlpha = false;
          }
        }

        for (unsigned long y=0; y<spec.height; ++y) {
          uint32_t* dst = (uint32_t*)(img.data()+y*spec.bytes_per_row);
          for (unsigned long x=0; x<spec.width; ++x, ++dst) {
            const uint32_t c = *dst;
            int r = ((c & spec.red_mask  ) >> spec.red_shift  );
            int g = ((c & spec.green_mask) >> spec.green_shift);
            int b = ((c & spec.blue_mask ) >> spec.blue_shift );
            int a = ((c & spec.alpha_mask) >> spec.alpha_shift);

            if (hasAlphaGreaterThanZero &&
                hasValidPremultipliedAlpha) {
              if (a > 0) {
                // Make straight alpha
                r = r * 255 / a;
                g = g * 255 / a;
                b = b * 255 / a;
              }
            }
            else {
              // If all alpha values = 0 or just one alpha value is
              // not a valid alpha for premultiplied RGB values, we
              // make the image opaque.
              a = 255;

              // We cannot change the image spec
              // (e.g. spec.alpha_mask=0) to make the image opaque,
              // because the "spec" variable is local to this
              // function. The image spec used by the client is the
              // one returned by get_image_spec().
            }

            *dst =
              (r << spec.red_shift  ) |
              (g << spec.green_shift) |
              (b << spec.blue_shift ) |
              (a << spec.alpha_shift);
          }
        }
      }
      break;
    }

    case 8: {
      const int colors = (bi->bmiHeader.biClrUsed > 0 ? bi->bmiHeader.biClrUsed: 256);
      std::vector<uint32_t> palette(colors);
      for (int c=0; c<colors; ++c) {
        palette[c] =
          (bi->bmiColors[c].rgbRed   << spec.red_shift) |
          (bi->bmiColors[c].rgbGreen << spec.green_shift) |
          (bi->bmiColors[c].rgbBlue  << spec.blue_shift);
      }

      const uint8_t* src = (((uint8_t*)bi)+bi->bmiHeader.biSize+sizeof(RGBQUAD)*colors);
      const int padding = (4-(spec.width&3))&3;

      for (long y=spec.height-1; y>=0; --y, src+=padding) {
        char* dst = img.data()+y*spec.bytes_per_row;

        for (unsigned long x=0; x<spec.width; ++x, ++src, dst+=3) {
          int idx = *src;
          if (idx < 0)
            idx = 0;
          else if (idx >= colors)
            idx = colors-1;

          *((uint32_t*)dst) = palette[idx];
        }
      }
      break;
    }
  }

  std::swap(output_img, img);
  return true;
}

bool lock::impl::get_image_spec(image_spec& spec) const {
  if (!IsClipboardFormatAvailable(CF_DIB))
    return false;

  BITMAPINFO* bi = (BITMAPINFO*)GetClipboardData(CF_DIB);
  if (!bi)
    return false;

  int w = bi->bmiHeader.biWidth;
  int h = bi->bmiHeader.biHeight;

  spec.width = w;
  spec.height = (h >= 0 ? h: -h);
  // We convert indexed to 24bpp RGB images to match the OS X behavior
  spec.bits_per_pixel = bi->bmiHeader.biBitCount;
  if (spec.bits_per_pixel <= 8)
    spec.bits_per_pixel = 24;
  spec.bytes_per_row = w*((spec.bits_per_pixel+7)/8);

  switch (spec.bits_per_pixel) {

    case 32:
      if (bi->bmiHeader.biCompression == BI_BITFIELDS) {
        spec.red_mask   = *((uint32_t*)&bi->bmiColors[0]);
        spec.green_mask = *((uint32_t*)&bi->bmiColors[1]);
        spec.blue_mask  = *((uint32_t*)&bi->bmiColors[2]);
        spec.alpha_mask = 0xff000000;
      }
      else if (bi->bmiHeader.biCompression == BI_RGB) {
        spec.red_mask   = 0xff0000;
        spec.green_mask = 0xff00;
        spec.blue_mask  = 0xff;
        spec.alpha_mask = 0xff000000;
      }
      break;

    case 24: {
      // We need one extra byte to avoid a crash updating the last
      // pixel on last row using:
      //
      //   *((uint32_t*)ptr) = pixel24bpp;
      //
      ++spec.bytes_per_row;

      // Align each row to 32bpp
      int padding = (4-(spec.bytes_per_row&3))&3;
      spec.bytes_per_row += padding;

      if (bi->bmiHeader.biCompression == BI_BITFIELDS) {
        spec.red_mask   = *((uint32_t*)&bi->bmiColors[0]);
        spec.green_mask = *((uint32_t*)&bi->bmiColors[1]);
        spec.blue_mask  = *((uint32_t*)&bi->bmiColors[2]);
      }
      else if (bi->bmiHeader.biCompression == BI_RGB) {
        spec.red_mask   = 0xff0000;
        spec.green_mask = 0xff00;
        spec.blue_mask  = 0xff;
      }
      break;
    }

    case 16: {
      int padding = (4-(spec.bytes_per_row&3))&3;
      spec.bytes_per_row += padding;

      if (bi->bmiHeader.biCompression == BI_BITFIELDS) {
        spec.red_mask   = *((DWORD*)&bi->bmiColors[0]);
        spec.green_mask = *((DWORD*)&bi->bmiColors[1]);
        spec.blue_mask  = *((DWORD*)&bi->bmiColors[2]);
      }
      else if (bi->bmiHeader.biCompression == BI_RGB) {
        spec.red_mask   = 0x7c00;
        spec.green_mask = 0x03e0;
        spec.blue_mask  = 0x001f;
      }
      break;
    }
  }

  unsigned long* masks = &spec.red_mask;
  unsigned long* shifts = &spec.red_shift;
  for (unsigned long* shift=shifts, *mask=masks; shift<shifts+4; ++shift, ++mask) {
    if (*mask)
      *shift = get_shift_from_mask(*mask);
  }

  return true;
}

format register_format(const std::string& name) {
  int reqsize = 1+MultiByteToWideChar(CP_UTF8, 0,
                                      name.c_str(), name.size(), NULL, 0);
  std::vector<WCHAR> buf(reqsize);
  MultiByteToWideChar(CP_UTF8, 0, name.c_str(), name.size(),
                      &buf[0], reqsize);

  // From MSDN, registered clipboard formats are identified by values
  // in the range 0xC000 through 0xFFFF.
  return (format)RegisterClipboardFormatW(&buf[0]);
}

} // namespace clip
#pragma warning(default: 4267)
