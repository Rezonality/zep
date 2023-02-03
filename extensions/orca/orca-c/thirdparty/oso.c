#include "oso.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if (defined(__GNUC__) || defined(__clang__)) && defined(__has_attribute)
#if __has_attribute(noinline) && __has_attribute(noclone)
#define OSO_NOINLINE __attribute__((noinline, noclone))
#elif __has_attribute(noinline)
#define OSO_NOINLINE __attribute__((noinline))
#endif
#elif defined(_MSC_VER)
#define OSO_NOINLINE __declspec(noinline)
#endif
#ifndef OSO_NOINLINE
#define OSO_NOINLINE
#endif

#define OSO_INTERNAL OSO_NOINLINE static
#define OSO_HDR(s) ((oso_header *)s - 1)
#define OSO_CAP_MAX (SIZE_MAX - (sizeof(oso_header) + 1))

typedef struct oso {
  size_t len, cap;
} oso_header;

OSO_INTERNAL oso *oso_impl_reallochdr(oso_header *hdr, size_t new_cap) {
  if (hdr) {
    oso_header *new_hdr = realloc(hdr, sizeof(oso_header) + new_cap + 1);
    if (!new_hdr) {
      free(hdr);
      return NULL;
    }
    new_hdr->cap = new_cap;
    return new_hdr + 1;
  }
  hdr = malloc(sizeof(oso_header) + new_cap + 1);
  if (!hdr)
    return NULL;
  hdr->len = 0;
  hdr->cap = new_cap;
  ((char *)(hdr + 1))[0] = '\0';
  return hdr + 1;
}
OSO_INTERNAL oso *oso_impl_catvprintf(oso *s, char const *fmt, va_list ap) {
  size_t old_len;
  int required;
  va_list cpy;
  va_copy(cpy, ap);
  required = vsnprintf(NULL, 0, fmt, cpy);
  va_end(cpy);
  osomakeroomfor(&s, (size_t)required);
  if (!s)
    return NULL;
  old_len = OSO_HDR(s)->len;
  vsnprintf((char *)s + old_len, (size_t)required + 1, fmt, ap);
  OSO_HDR(s)->len = old_len + (size_t)required;
  return s;
}

OSO_NOINLINE
void osoensurecap(oso **p, size_t new_cap) {
  oso *s = *p;
  if (new_cap > OSO_CAP_MAX) {
    if (s) {
      free(OSO_HDR(s));
      *p = NULL;
    }
    return;
  }
  oso_header *hdr = NULL;
  if (s) {
    hdr = OSO_HDR(s);
    if (hdr->cap >= new_cap)
      return;
  }
  *p = oso_impl_reallochdr(hdr, new_cap);
}

OSO_NOINLINE
void osomakeroomfor(oso **p, size_t add_len) {
  oso *s = *p;
  oso_header *hdr = NULL;
  size_t new_cap;
  if (s) {
    hdr = OSO_HDR(s);
    size_t len = hdr->len, cap = hdr->cap;
    if (len > OSO_CAP_MAX - add_len) { // overflow, goodnight
      free(hdr);
      *p = NULL;
      return;
    }
    new_cap = len + add_len;
    if (cap >= new_cap)
      return;
  } else {
    if (add_len > OSO_CAP_MAX)
      return;
    new_cap = add_len;
  }
  *p = oso_impl_reallochdr(hdr, new_cap);
}

void osoput(oso **p, char const *restrict cstr) {
  osoputlen(p, cstr, strlen(cstr));
}

OSO_NOINLINE
void osoputlen(oso **p, char const *restrict cstr, size_t len) {
  oso *s = *p;
  osoensurecap(&s, len);
  if (s) {
    OSO_HDR(s)->len = len;
    memcpy((char *)s, cstr, len);
    ((char *)s)[len] = '\0';
  }
  *p = s;
}
void osoputoso(oso **p, oso const *other) {
  if (!other)
    return;
  osoputlen(p, (char const *)other, OSO_HDR(other)->len);
}
void osoputvprintf(oso **p, char const *fmt, va_list ap) {
  oso *s = *p;
  if (s) {
    OSO_HDR(s)->len = 0;
    ((char *)s)[0] = '\0';
  }
  *p = oso_impl_catvprintf(s, fmt, ap);
}
void osoputprintf(oso **p, char const *fmt, ...) {
  oso *s = *p;
  if (s) {
    OSO_HDR(s)->len = 0;
    ((char *)s)[0] = '\0';
  }
  va_list ap;
  va_start(ap, fmt);
  *p = oso_impl_catvprintf(s, fmt, ap);
  va_end(ap);
}
void osocat(oso **p, char const *cstr) { osocatlen(p, cstr, strlen(cstr)); }
OSO_NOINLINE
void osocatlen(oso **p, char const *cstr, size_t len) {
  oso *s = *p;
  osomakeroomfor(&s, len);
  if (s) {
    oso_header *hdr = OSO_HDR(s);
    size_t curr_len = hdr->len;
    memcpy((char *)s + curr_len, cstr, len);
    ((char *)s)[curr_len + len] = '\0';
    hdr->len = curr_len + len;
  }
  *p = s;
}
void osocatoso(oso **p, oso const *other) {
  if (!other)
    return;
  osocatlen(p, (char const *)other, OSO_HDR(other)->len);
}
void osocatvprintf(oso **p, char const *fmt, va_list ap) {
  *p = oso_impl_catvprintf(*p, fmt, ap);
}
void osocatprintf(oso **p, char const *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  *p = oso_impl_catvprintf(*p, fmt, ap);
  va_end(ap);
}
void osoclear(oso **p) {
  oso *s = *p;
  if (!s)
    return;
  OSO_HDR(s)->len = 0;
  ((char *)s)[0] = '\0';
}
void osofree(oso *s) {
  if (s)
    free(OSO_HDR(s));
}
void osowipe(oso **p) {
  osofree(*p);
  *p = NULL;
}
void ososwap(oso **a, oso **b) {
  oso *tmp = *a;
  *a = *b;
  *b = tmp;
}
void osopokelen(oso *s, size_t len) { OSO_HDR(s)->len = len; }
size_t osolen(oso const *s) { return s ? OSO_HDR(s)->len : 0; }
size_t osocap(oso const *s) { return s ? OSO_HDR(s)->cap : 0; }
void osolencap(oso const *s, size_t *out_len, size_t *out_cap) {
  if (!s) {
    *out_len = 0;
    *out_cap = 0;
    return;
  }
  oso_header *hdr = OSO_HDR(s);
  *out_len = hdr->len;
  *out_cap = hdr->cap;
}
size_t osoavail(oso const *s) {
  if (!s)
    return 0;
  oso_header *h = OSO_HDR(s);
  return h->cap - h->len;
}

void osotrim(oso *restrict s, char const *restrict cut_set) {
  if (!s)
    return;
  char *str, *end, *start_pos, *end_pos;
  start_pos = str = (char *)s;
  end_pos = end = str + OSO_HDR(s)->len - 1;
  while (start_pos <= end && strchr(cut_set, *start_pos))
    start_pos++;
  while (end_pos > start_pos && strchr(cut_set, *end_pos))
    end_pos--;
  size_t len = (start_pos > end_pos) ? 0 : ((size_t)(end_pos - start_pos) + 1);
  OSO_HDR(s)->len = len;
  if (str != start_pos)
    memmove(str, start_pos, len);
  str[len] = '\0';
}

#undef OSO_HDR
#undef OSO_NOINLINE
#undef OSO_CAP_MAX
#undef OSO_INTERNAL
