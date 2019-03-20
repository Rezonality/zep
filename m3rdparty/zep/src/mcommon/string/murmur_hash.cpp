#include "mcommon.h"
#include <cassert>

#include "murmur_hash.h"

// CM: I can't remember where this came from; please let me know if you do!
// I know it is open source, but not sure who wrote it.
uint32_t murmur_hash(const void * key, int len, uint32_t seed)
{
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.
    const unsigned int m = 0x5bd1e995;
    const int r = 24;

    // Initialize the hash to a 'random' value
    unsigned int h = seed ^ len;

    // Mix 4 bytes at a time into the hash
    const unsigned char * data = (const unsigned char *)key;

    while (len >= 4)
    {
#ifdef PLATFORM_BIG_ENDIAN
        unsigned int k = (data[0]) + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
#else
        unsigned int k = *(unsigned int *)data;
#endif

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    // Handle the last few bytes of the input array

    switch (len)
    {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0];
        h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

/// Inverts a (h ^= h >> s) operation with 8 <= s <= 16
unsigned int invert_shift_xor(unsigned int hs, unsigned int s)
{
    assert(s >= 8 && s <= 16);
    unsigned hs0 = hs >> 24;
    unsigned hs1 = (hs >> 16) & 0xff;
    unsigned hs2 = (hs >> 8) & 0xff;
    unsigned hs3 = hs & 0xff;

    unsigned h0 = hs0;
    unsigned h1 = hs1 ^ (h0 >> (s - 8));
    unsigned h2 = (hs2 ^ (h0 << (16 - s)) ^ (h1 >> (s - 8))) & 0xff;
    unsigned h3 = (hs3 ^ (h1 << (16 - s)) ^ (h2 >> (s - 8))) & 0xff;
    return (h0 << 24) + (h1 << 16) + (h2 << 8) + h3;
}

unsigned int murmur_hash_inverse(unsigned int h, unsigned int seed)
{
    const unsigned int m = 0x5bd1e995;
    const unsigned int minv = 0xe59b19bd;	// Multiplicative inverse of m under % 2^32
    const int r = 24;

    h = invert_shift_xor(h, 15);
    h *= minv;
    h = invert_shift_xor(h, 13);

    unsigned int hforward = seed ^ 4;
    hforward *= m;
    unsigned int k = hforward ^ h;
    k *= minv;
    k ^= k >> r;
    k *= minv;

#ifdef PLATFORM_BIG_ENDIAN
    char *data = (char *)&k;
    k = (data[0]) + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
#endif

    return k;
}

uint64_t murmur_hash_64(const void * key, uint32_t len, uint64_t seed)
{
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const uint32_t r = 47;

    uint64_t h = seed ^ (len * m);

    const uint64_t * data = (const uint64_t *)key;
    const uint64_t * end = data + (len / 8);

    while (data != end)
    {
#ifdef PLATFORM_BIG_ENDIAN
        uint64 k = *data++;
        char *p = (char *)&k;
        char c;
        c = p[0]; p[0] = p[7]; p[7] = c;
        c = p[1]; p[1] = p[6]; p[6] = c;
        c = p[2]; p[2] = p[5]; p[5] = c;
        c = p[3]; p[3] = p[4]; p[4] = c;
#else
        uint64_t k = *data++;
#endif

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const unsigned char * data2 = (const unsigned char*)data;

    switch (len & 7)
    {
    case 7: h ^= uint64_t(data2[6]) << 48;
    case 6: h ^= uint64_t(data2[5]) << 40;
    case 5: h ^= uint64_t(data2[4]) << 32;
    case 4: h ^= uint64_t(data2[3]) << 24;
    case 3: h ^= uint64_t(data2[2]) << 16;
    case 2: h ^= uint64_t(data2[1]) << 8;
    case 1: h ^= uint64_t(data2[0]);
        h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}
