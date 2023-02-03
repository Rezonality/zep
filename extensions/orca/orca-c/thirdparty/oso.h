#pragma once
// Heap-allocated string handling.
// Inspired by antirez's sds and gingerBill's gb_string.h.
//
// "I need null-terminated strings to interact with libc and/or POSIX, and my
// software isn't serious enough to warrant using arena or page allocation
// strategies. Manually fussing with null-terminated strings with libc sucks,
// even though we're allocating everything individually on the heap! Why can't
// we at least get a nicer interface for the trade-off we've already made?"
//
//                                EXAMPLE
//                               ---------
//   oso *mystring = NULL;
//   osoput(&mystring, "Hello World");
//   printf((char *)mystring);
//   osoput(&mystring, "How about some pancakes?");
//   printf((char *)mystring);
//   osocat(&mstring, " Sure!");
//   printf((char *)mystring);
//   osofree(mystring);
//
//   > Hello World!
//   > How about some pancakes?
//   > How about some pancakes? Sure!
//
//                                 RULES
//                                -------
// 1. `oso *` can always be cast to `char *`, but it's your job to check if
//    it's null before passing it to on something that doesn't tolerate null
//    pointers.
//
// 2. The functions defined in this header tolerate null pointers like this:
//
//           `oso *` -> OK to be null.
//    `char const *` -> Must not be null.
//          `oso **` -> Must not be null, but the `oso *` pointed to
//                      can be null. The pointed-to `oso *` may be
//                      modified during the call.
//
// 3. `oso *` and `char const *` as arguments to the functions here must not
//    overlap in memory. During the call, the buffer pointed to by a `oso *`
//    might need to be reallocated in memory to make room for the `char const
//    *` contents, and if the `char const *` contents end up being moved by
//    that operation, the pointer will no longer be pointing at valid memory.
//    (This also applies to functions which accept two `oso *` as inputs.)
//
// Parameters with the type `oso *` are safe to pass as a null pointer. But
// `oso **` parameters shouldn't be null. The `oso *` pointed to by the `oso
// **` can be null, though. In other words, you need to do `&mystring` to pass
// a `oso **` argument.
//
// During the function call, if the `oso *` pointed to by the `oso **` needs to
// become non-null, or if the existing buffer needs to be moved to grow larger,
// the `oso *` will be set to a new value.
//
//   oso *mystring = NULL;
//   osolen(mystring); // Gives 0
//   osocat(&mystring, "waffles");
//   osolen(mystring); // Gives 7
//   osofree(mystring);
//
//                                 NAMES
//                                -------
//   osoput______ -> Copy a string into an oso string.
//   osocat______ -> Append a string onto the end of an oso string.
//   ______len    -> Do it with an explicit length argument, so the C-string
//                   doesn't have to be '\0'-terminated.
//   ______oso    -> Do it with a second oso string.
//   ______printf -> Do it by using printf.
//
//                             ALLOC FAILURE
//                            ---------------
// If an allocation fails (including failing to reallocate) the `oso *` will be
// set to null. If you decide to handle memory allocation failures, you'll need
// to check for that.
//
// In the oso code, if a reallocation of an existing buffer fails (`realloc()`
// returns null) then the old, still-valid buffer is immediately freed.
// Therefore, in an out-of-memory situation, it's possible that you will *lose*
// your strings without getting a chance to do something with the old buffers.
//
// Variations of the oso functions may be added in the future, with a _c suffix
// or something, which preserve the old buffer and return an error code in the
// event of a reallocation failure. I'm not sure how important this is, since
// most existing libc-based software doesn't handle out-of-memory conditions
// for small strings without imploding.
//
// (sds, for example, will lose your string *and* leak the old buffer if a
// reallocation fails.)

#include <stdarg.h>
#include <stddef.h>

#if (defined(__GNUC__) || defined(__clang__)) && defined(__has_attribute)
#if __has_attribute(format)
#define OSO_PRINTF(...) __attribute__((format(printf, __VA_ARGS__)))
#endif
#if __has_attribute(nonnull)
#define OSO_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#endif
#endif
#ifndef OSO_PRINTF
#define OSO_PRINTF(...)
#endif
#ifndef OSO_NONNULL
#define OSO_NONNULL(...)
#endif

typedef struct oso oso;

#define osoc(s) ((char const *)s)

void osoput(oso **p, char const *cstr) OSO_NONNULL();
// ^- Copies the '\0'-terminated string into the `oso *` string located at
//    `*p`. If `*p` is null or there isn't enough capacity to hold `cstr`, it
//    will be reallocated. The pointer value at `*p` will be updated if
//    necessary. `*p` and `cstr` must not point to overlapping memory.
void osoputlen(oso **p, char const *cstr, size_t len) OSO_NONNULL();
// ^- Same as above, but uses an additional parameter that specifies the length
//    of `cstr, and `cstr` does not have to be '\0'-terminated.
//    `*p` and `cstr` must not point to overlapping memory.
void osoputoso(oso **p, oso const *other) OSO_NONNULL(1);
// ^- Same as above, but using another `oso`. `*p` and `other` must not point
//    to overlapping memory.
void osoputvprintf(oso **p, char const *fmt, va_list ap) OSO_NONNULL(1, 2)
    OSO_PRINTF(2, 0);
void osoputprintf(oso **p, char const *fmt, ...) OSO_NONNULL(1, 2)
    OSO_PRINTF(2, 3);
// ^- Same as above, but do it by using printf.

void osocat(oso **p, char const *cstr) OSO_NONNULL();
void osocatlen(oso **p, char const *cstr, size_t len) OSO_NONNULL();
void osocatoso(oso **p, oso const *other) OSO_NONNULL(1);
void osocatvprintf(oso **p, char const *fmt, va_list ap) OSO_NONNULL(1, 2)
    OSO_PRINTF(2, 0);
void osocatprintf(oso **p, char const *fmt, ...) OSO_NONNULL(1, 2)
    OSO_PRINTF(2, 3);
// ^- Append string to oso string. Same rules as `osoput` family.

void osoensurecap(oso **p, size_t cap) OSO_NONNULL();
// ^- Ensure that s has at least `cap` memory allocated for it. This does not
//    care about the strlen of the characters or the prefixed length count --
//    only the backing memory allocation.
void osomakeroomfor(oso **p, size_t len) OSO_NONNULL();
// ^- Ensure that s has enough allocated space after the '\0'-terminnation
//    character to hold an additional add_len characters. It does not adjust
//    the `length` number value, only the capacity, if necessary.
//
//    Both `osoensurecap()` and `osomakeroomfor()` can be used to avoid making
//    multiple smaller reallactions as the string grows in the future. You can
//    also use them if you're going to modify the string buffer manually in
//    your own code, and need to create some space in the buffer.

void osoclear(oso **p) OSO_NONNULL();
// ^- Set len to 0, write '\0' at pos 0. Leaves allocated memory in place.
void osofree(oso *s);
// ^- You know. And calling with null is allowed.
void osowipe(oso **p) OSO_NONNULL();
// ^- It's like `osofree()`, except you give it a ptr-to-ptr, and it also sets
//    `*p` to null for you when it's done freeing the memory.
void ososwap(oso **a, oso **b) OSO_NONNULL();
// ^- Swaps the two pointers. Yeah, that's all it does. Why? Because it's
//    common when dealing memory management for individually allocated strings
//    and changing between old and new string values.
void osopokelen(oso *s, size_t len) OSO_NONNULL();
// ^- Manually updates length field. Doesn't do anything else for you.

size_t osolen(oso const *s);
// ^- Bytes in use by the string (not including the '\0' character.)
size_t osocap(oso const *s);
// ^- Bytes allocated on heap (not including the '\0' terminator.)
void osolencap(oso const *s, size_t *out_len, size_t *out_cap)
    OSO_NONNULL(2, 3);
// ^- Get both the len and the cap in one call.
size_t osoavail(oso const *s);
// ^- osocap(s) - osolen(s)

void osotrim(oso *restrict s, char const *restrict cut_set) OSO_NONNULL(2);
// ^- Remove the characters in `cut_set` from the beginning and ending of `s`.

#undef OSO_PRINTF
#undef OSO_NONNULL
