////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2017
//
// 

#ifndef ANDYZIP_HUFFMAN_TABLE_HPP_
#define ANDYZIP_HUFFMAN_TABLE_HPP_

#include <cstdint>
#include <utility>

#if _MSC_VER > 0
  #define ALWAYS_INLINE __forceinline
#elif __has_attribute(always_inline)
  #define ALWAYS_INLINE __attribute__((always_inline))
#else
  #define ALWAYS_INLINE
#endif

namespace andyzip {
  static inline uint16_t rev16(uint16_t value) {
    // https://graphics.stanford.edu/~seander/bithacks.html#BitReverseTable
    static const unsigned char BitReverseTable256[256] = {
      #define R2(n)     n,     n + 2*64,     n + 1*64,     n + 3*64
      #define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
      #define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )
      R6(0), R6(2), R6(1), R6(3)
    };
    #undef R2
    #undef R4
    #undef R6
    return BitReverseTable256[value&0xff] << 8 | BitReverseTable256[(value>>8)&0xff];
  }

  template<int MaxCodes>
  class huffman_table {
    uint8_t min_length_;
    uint8_t max_length_;

    uint16_t symbols_[MaxCodes];
    uint16_t limits_[18];
    uint16_t base_[18];

    bool invalid_;

  public:
    static const int max_codes = MaxCodes;

    void init(const uint8_t *lengths, const uint16_t *symbols, unsigned num_lengths) {
      invalid_ = true;

      if (num_lengths == 1) {
        min_length_ = 0;
        max_length_ = 0;
        limits_[0] = 0xffff;
        base_[0] = 0;
        symbols_[0] = symbols[0];
        invalid_ = false;
        return;
      }
      if (num_lengths == 2) {
        min_length_ = 1;
        max_length_ = 1;
        limits_[0] = 0xffff;
        base_[0] = 0;
        symbols_[0] = symbols[0];
        symbols_[1] = symbols[1];
        invalid_ = false;
        return;
      }

      min_length_ = 16;
      max_length_ = 0;
      for (unsigned i = 0; i != num_lengths; ++i) {
        if (lengths[i]) {
          if (min_length_ > lengths[i]) min_length_ = lengths[i];
          if (max_length_ < lengths[i]) max_length_ = lengths[i];
        }
      }

      if ( min_length_ <= 0 || min_length_ > max_length_ || max_length_ > 16 ) {
        return;
      }

      unsigned code = 0;
      unsigned huffcode = 0;
      for (unsigned length = min_length_; length <= max_length_; ++length) {
        base_[length-min_length_] = huffcode - code;
        for (unsigned i = 0; i != num_lengths; ++i) {
          if (lengths[i] == length) {
            symbols_[code++] = symbols ? symbols[i] : i;
            huffcode++;
          }
        }
        limits_[length-min_length_] = (uint16_t)( ( huffcode << (16-length) ) - 1 );
        if (( huffcode << (16-length) ) - 1 > 0xffff) {
          return;
        }
        huffcode *= 2;
      }

      // prevent escape from bitstream decoding loop.
      limits_[max_length_+1-min_length_] = 0xffff;
      invalid_ = false;
    }

    bool invalid() const { return invalid_; }

    std::pair<unsigned, unsigned> ALWAYS_INLINE decode(unsigned peek16) {
      unsigned value = rev16(peek16);
      unsigned index = 0;
      while (value > limits_[index]) {
        index++;
      }
      unsigned length = min_length_ + index;
      unsigned offset = ( value >> ( 16 - length ) );
      unsigned code = symbols_[offset - base_[index]];
      return std::make_pair(length, code);
    }
  };
}

#endif
