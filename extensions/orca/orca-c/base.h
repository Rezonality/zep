#pragma once
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//#include <unistd.h>

// clang or gcc or other
#if defined(__clang__)
#define ORCA_OPT_MINSIZE __attribute__((minsize))
#elif defined(__GNUC__)
#define ORCA_OPT_MINSIZE __attribute__(("Os"))
#else
#define ORCA_OPT_MINSIZE
#endif

// (gcc / clang) or msvc or other
#if defined(__GNUC__) || defined(__clang__)
#define ORCA_FORCEINLINE __attribute__((always_inline)) inline
#define ORCA_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define ORCA_FORCEINLINE __forceinline
#define ORCA_NOINLINE __declspec(noinline)
#else
#define ORCA_FORCEINLINE inline
#define ORCA_NOINLINE
#endif

// (gcc / clang) or other
#if defined(__GNUC__) || defined(__clang__)
#define ORCA_ASSUME_ALIGNED(_ptr, _alignment)                                  \
  __builtin_assume_aligned(_ptr, _alignment)
#define ORCA_PURE __attribute__((pure))
#define ORCA_LIKELY(_x) __builtin_expect(_x, 1)
#define ORCA_UNLIKELY(_x) __builtin_expect(_x, 0)
#define ORCA_OK_IF_UNUSED __attribute__((unused))
#define ORCA_UNREACHABLE __builtin_unreachable()
#else
#define ORCA_ASSUME_ALIGNED(_ptr, _alignment) (_ptr)
#define ORCA_PURE
#define ORCA_LIKELY(_x) (_x)
#define ORCA_UNLIKELY(_x) (_x)
#define ORCA_OK_IF_UNUSED
#define ORCA_UNREACHABLE assert(false)
#endif

// array count, safer on gcc/clang
#if defined(__GNUC__) || defined(__clang__)
#define ORCA_ASSERT_IS_ARRAY(_array)                                           \
  (sizeof(char[1 - 2 * __builtin_types_compatible_p(                           \
                           __typeof(_array), __typeof(&(_array)[0]))]) -       \
   1)
#define ORCA_ARRAY_COUNTOF(_array)                                             \
  (sizeof(_array) / sizeof((_array)[0]) + ORCA_ASSERT_IS_ARRAY(_array))
#else
// pray
#define ORCA_ARRAY_COUNTOF(_array) (sizeof(_array) / sizeof(_array[0]))
#endif

#define ORCA_Y_MAX UINT16_MAX
#define ORCA_X_MAX UINT16_MAX

typedef uint8_t U8;
typedef int8_t I8;
typedef uint16_t U16;
typedef int16_t I16;
typedef uint32_t U32;
typedef int32_t I32;
typedef uint64_t U64;
typedef int64_t I64;
typedef size_t Usz;
typedef int64_t Isz;

typedef char Glyph;
typedef U8 Mark;

ORCA_FORCEINLINE static Usz orca_round_up_power2(Usz x) {
  assert(x <= SIZE_MAX / 2 + 1);
  x -= 1;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
#if SIZE_MAX > UINT32_MAX
  x |= x >> 32;
#endif
  return x + 1;
}

ORCA_OK_IF_UNUSED
static bool orca_is_valid_glyph(Glyph c) {
  if (c >= '0' && c <= '9')
    return true;
  if (c >= 'A' && c <= 'Z')
    return true;
  if (c >= 'a' && c <= 'z')
    return true;
  switch (c) {
  case '!':
  case '#':
  case '%':
  case '*':
  case '.':
  case ':':
  case ';':
  case '=':
  case '?':
    return true;
  }
  return false;
}

//#define restrict __restrict
