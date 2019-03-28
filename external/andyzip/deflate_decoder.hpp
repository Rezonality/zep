////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2012-2016
//
// 


#ifndef ANDYZIP_DEFLATE_DECODER_HPP_
#define ANDYZIP_DEFLATE_DECODER_HPP_

#include <cstdint>
#include <cstring>

namespace andyzip {

  class deflate_decoder {
    enum { debug = 0 };

    struct huffman_table {
      uint8_t min_lit_length;
      uint8_t max_lit_length;
      uint8_t min_dist_length;
      uint8_t max_dist_length;

      //uint8_t lit_lengths[288];
      uint16_t lit_codes[288];
      uint16_t lit_limits[18];
      uint16_t lit_base[18];

      //uint8_t dist_lengths[32];
      uint16_t dist_codes[32];
      uint16_t dist_limits[18];
      uint16_t dist_base[18];
    };

    huffman_table fixed_;

    // on ARM we can do this faster with the "rev" instruction
    inline static uint16_t rev16(uint16_t value) {
      // small table version.
      //static const uint8_t r16[] = { 0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe, 0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf };
      //return r16[value&0x0f] * 0x1000 | r16[(value>>4)&0x0f] * 0x100 | r16[(value>>8)&0x0f] * 0x10 | r16[value>>12];

      // this table is probably too big to be efficient. L1 cache misses are very expensive.
      static const uint8_t r256[] = {
        0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 
        0x00+0x10, 0x80+0x10, 0x40+0x10, 0xc0+0x10, 0x20+0x10, 0xa0+0x10, 0x60+0x10, 0xe0+0x10, 
        0x00+8, 0x80+8, 0x40+8, 0xc0+8, 0x20+8, 0xa0+8, 0x60+8, 0xe0+8, 
        0x00+0x10+8, 0x80+0x10+8, 0x40+0x10+8, 0xc0+0x10+8, 0x20+0x10+8, 0xa0+0x10+8, 0x60+0x10+8, 0xe0+0x10+8, 
        0x00+ 4, 0x80+ 4, 0x40+ 4, 0xc0+ 4, 0x20+ 4, 0xa0+ 4, 0x60+ 4, 0xe0+ 4, 
        0x00+0x10+ 4, 0x80+0x10+ 4, 0x40+0x10+ 4, 0xc0+0x10+ 4, 0x20+0x10+ 4, 0xa0+0x10+ 4, 0x60+0x10+ 4, 0xe0+0x10+ 4, 
        0x00+8+ 4, 0x80+8+ 4, 0x40+8+ 4, 0xc0+8+ 4, 0x20+8+ 4, 0xa0+8+ 4, 0x60+8+ 4, 0xe0+8+ 4, 
        0x00+0x10+8+ 4, 0x80+0x10+8+ 4, 0x40+0x10+8+ 4, 0xc0+0x10+8+ 4, 0x20+0x10+8+ 4, 0xa0+0x10+8+ 4, 0x60+0x10+8+ 4, 0xe0+0x10+8+ 4, 
        0x00+ 2, 0x80+ 2, 0x40+ 2, 0xc0+ 2, 0x20+ 2, 0xa0+ 2, 0x60+ 2, 0xe0+ 2, 
        0x00+0x10+ 2, 0x80+0x10+ 2, 0x40+0x10+ 2, 0xc0+0x10+ 2, 0x20+0x10+ 2, 0xa0+0x10+ 2, 0x60+0x10+ 2, 0xe0+0x10+ 2, 
        0x00+8+ 2, 0x80+8+ 2, 0x40+8+ 2, 0xc0+8+ 2, 0x20+8+ 2, 0xa0+8+ 2, 0x60+8+ 2, 0xe0+8+ 2, 
        0x00+0x10+8+ 2, 0x80+0x10+8+ 2, 0x40+0x10+8+ 2, 0xc0+0x10+8+ 2, 0x20+0x10+8+ 2, 0xa0+0x10+8+ 2, 0x60+0x10+8+ 2, 0xe0+0x10+8+ 2, 
        0x00+ 4+ 2, 0x80+ 4+ 2, 0x40+ 4+ 2, 0xc0+ 4+ 2, 0x20+ 4+ 2, 0xa0+ 4+ 2, 0x60+ 4+ 2, 0xe0+ 4+ 2, 
        0x00+0x10+ 4+ 2, 0x80+0x10+ 4+ 2, 0x40+0x10+ 4+ 2, 0xc0+0x10+ 4+ 2, 0x20+0x10+ 4+ 2, 0xa0+0x10+ 4+ 2, 0x60+0x10+ 4+ 2, 0xe0+0x10+ 4+ 2, 
        0x00+8+ 4+ 2, 0x80+8+ 4+ 2, 0x40+8+ 4+ 2, 0xc0+8+ 4+ 2, 0x20+8+ 4+ 2, 0xa0+8+ 4+ 2, 0x60+8+ 4+ 2, 0xe0+8+ 4+ 2, 
        0x00+0x10+8+ 4+ 2, 0x80+0x10+8+ 4+ 2, 0x40+0x10+8+ 4+ 2, 0xc0+0x10+8+ 4+ 2, 0x20+0x10+8+ 4+ 2, 0xa0+0x10+8+ 4+ 2, 0x60+0x10+8+ 4+ 2, 0xe0+0x10+8+ 4+ 2, 
        0x00+ 1, 0x80+ 1, 0x40+ 1, 0xc0+ 1, 0x20+ 1, 0xa0+ 1, 0x60+ 1, 0xe0+ 1, 
        0x00+0x10+ 1, 0x80+0x10+ 1, 0x40+0x10+ 1, 0xc0+0x10+ 1, 0x20+0x10+ 1, 0xa0+0x10+ 1, 0x60+0x10+ 1, 0xe0+0x10+ 1, 
        0x00+8+ 1, 0x80+8+ 1, 0x40+8+ 1, 0xc0+8+ 1, 0x20+8+ 1, 0xa0+8+ 1, 0x60+8+ 1, 0xe0+8+ 1, 
        0x00+0x10+8+ 1, 0x80+0x10+8+ 1, 0x40+0x10+8+ 1, 0xc0+0x10+8+ 1, 0x20+0x10+8+ 1, 0xa0+0x10+8+ 1, 0x60+0x10+8+ 1, 0xe0+0x10+8+ 1, 
        0x00+ 4+ 1, 0x80+ 4+ 1, 0x40+ 4+ 1, 0xc0+ 4+ 1, 0x20+ 4+ 1, 0xa0+ 4+ 1, 0x60+ 4+ 1, 0xe0+ 4+ 1, 
        0x00+0x10+ 4+ 1, 0x80+0x10+ 4+ 1, 0x40+0x10+ 4+ 1, 0xc0+0x10+ 4+ 1, 0x20+0x10+ 4+ 1, 0xa0+0x10+ 4+ 1, 0x60+0x10+ 4+ 1, 0xe0+0x10+ 4+ 1, 
        0x00+8+ 4+ 1, 0x80+8+ 4+ 1, 0x40+8+ 4+ 1, 0xc0+8+ 4+ 1, 0x20+8+ 4+ 1, 0xa0+8+ 4+ 1, 0x60+8+ 4+ 1, 0xe0+8+ 4+ 1, 
        0x00+0x10+8+ 4+ 1, 0x80+0x10+8+ 4+ 1, 0x40+0x10+8+ 4+ 1, 0xc0+0x10+8+ 4+ 1, 0x20+0x10+8+ 4+ 1, 0xa0+0x10+8+ 4+ 1, 0x60+0x10+8+ 4+ 1, 0xe0+0x10+8+ 4+ 1, 
        0x00+ 2+ 1, 0x80+ 2+ 1, 0x40+ 2+ 1, 0xc0+ 2+ 1, 0x20+ 2+ 1, 0xa0+ 2+ 1, 0x60+ 2+ 1, 0xe0+ 2+ 1, 
        0x00+0x10+ 2+ 1, 0x80+0x10+ 2+ 1, 0x40+0x10+ 2+ 1, 0xc0+0x10+ 2+ 1, 0x20+0x10+ 2+ 1, 0xa0+0x10+ 2+ 1, 0x60+0x10+ 2+ 1, 0xe0+0x10+ 2+ 1, 
        0x00+8+ 2+ 1, 0x80+8+ 2+ 1, 0x40+8+ 2+ 1, 0xc0+8+ 2+ 1, 0x20+8+ 2+ 1, 0xa0+8+ 2+ 1, 0x60+8+ 2+ 1, 0xe0+8+ 2+ 1, 
        0x00+0x10+8+ 2+ 1, 0x80+0x10+8+ 2+ 1, 0x40+0x10+8+ 2+ 1, 0xc0+0x10+8+ 2+ 1, 0x20+0x10+8+ 2+ 1, 0xa0+0x10+8+ 2+ 1, 0x60+0x10+8+ 2+ 1, 0xe0+0x10+8+ 2+ 1, 
        0x00+ 4+ 2+ 1, 0x80+ 4+ 2+ 1, 0x40+ 4+ 2+ 1, 0xc0+ 4+ 2+ 1, 0x20+ 4+ 2+ 1, 0xa0+ 4+ 2+ 1, 0x60+ 4+ 2+ 1, 0xe0+ 4+ 2+ 1, 
        0x00+0x10+ 4+ 2+ 1, 0x80+0x10+ 4+ 2+ 1, 0x40+0x10+ 4+ 2+ 1, 0xc0+0x10+ 4+ 2+ 1, 0x20+0x10+ 4+ 2+ 1, 0xa0+0x10+ 4+ 2+ 1, 0x60+0x10+ 4+ 2+ 1, 0xe0+0x10+ 4+ 2+ 1, 
        0x00+8+ 4+ 2+ 1, 0x80+8+ 4+ 2+ 1, 0x40+8+ 4+ 2+ 1, 0xc0+8+ 4+ 2+ 1, 0x20+8+ 4+ 2+ 1, 0xa0+8+ 4+ 2+ 1, 0x60+8+ 4+ 2+ 1, 0xe0+8+ 4+ 2+ 1, 
        0x00+0x10+8+ 4+ 2+ 1, 0x80+0x10+8+ 4+ 2+ 1, 0x40+0x10+8+ 4+ 2+ 1, 0xc0+0x10+8+ 4+ 2+ 1, 0x20+0x10+8+ 4+ 2+ 1, 0xa0+0x10+8+ 4+ 2+ 1, 0x60+0x10+8+ 4+ 2+ 1, 0xe0+0x10+8+ 4+ 2+ 1, 
      };
      return r256[value&0xff] << 8 | r256[(value>>8)&0xff];

      //value = ( ( value >> 1 ) & 0x5555 ) | ( ( value & 0x5555 ) << 1 );
      //value = ( ( value >> 2 ) & 0x3333 ) | ( ( value & 0x3333 ) << 2 );
      //value = ( ( value >> 4 ) & 0x0f0f ) | ( ( value & 0x0f0f ) << 4 );
      //value = ( ( value >> 8 ) & 0x00ff ) | ( ( value & 0x00ff ) << 8 );
      //return value;
    }

    static bool build_huffman(uint8_t *lengths, unsigned num_lengths, uint8_t &min_length, uint8_t &max_length, uint16_t *codes, uint16_t *limits, uint16_t *base) {
      min_length = 16;
      max_length = 0;
      for (unsigned i = 0; i != num_lengths; ++i) {
        if (debug) printf("%d,", lengths[i]);
        if (lengths[i]) {
          if (min_length > lengths[i]) min_length = lengths[i];
          if (max_length < lengths[i]) max_length = lengths[i];
        }
      }
      if (debug) printf("\n");

      if (debug) printf("min_length=%d\n", min_length);
      if (debug) printf("max_length=%d\n", max_length);

      if ( min_length <= 0 || min_length > max_length || max_length > 16 ) {
        return false;
      }

      unsigned code = 0;
      unsigned huffcode = 0;
      for (unsigned length = min_length; length <= max_length; ++length) {
        base[length-min_length] = huffcode - code;
        for (unsigned i = 0; i != num_lengths; ++i) {
          if (lengths[i] == length) {
            codes[code++] = i;
            //dump_bits(huffcode << (16-length), 16, "huffcode");
            huffcode++;
          }
        }
        limits[length-min_length] = (uint16_t)( ( huffcode << (16-length) ) - 1 );
        if (debug) printf("length %d: lim=%04x base=%04x\n", length, ( huffcode << (16-length) ) - 1, base[length-min_length]);
        if (( huffcode << (16-length) ) - 1 > 0xffff) {
          if (debug) printf("invalid huffman table\n");
          return false;
        }
        huffcode *= 2;
      }

      // prevent escape from bitstream decoding loop.
      limits[max_length+1-min_length] = 0xffff;
      return true;
    }

    /// debug function for dumping bit fields
    static void dump_bits(unsigned value, unsigned bits, const char *name) {
      char tmp[64];
      if (bits < sizeof(tmp)-1) {
        for (unsigned i = 0; i != bits; ++i) tmp[i] = ( ( value >> (bits-1-i) ) & 1 ) + '0';
        tmp[bits] = 0;
        printf("[%s] %s\n", tmp, name);
      }
    }

    /// peek a fixed number of little-endian bits from the bitstream
    /// note: this will have to be fixed on PPC and other big-endian devices
    static unsigned peek(const uint8_t *src, unsigned bitptr, unsigned bits, const char *name) {
      unsigned i = bitptr >> 3, j = bitptr & 7;
      unsigned value = ( (unsigned&)src[i] >> j ) & ( (1u << bits) - 1 );
      if (debug && name) dump_bits(value, bits, name);
      return value;
    }

    unsigned decode_uncompressed(uint8_t *&dest, uint8_t *dest_max, const uint8_t *src, const uint8_t *src_max, unsigned bitptr) const {
      bitptr = ( bitptr + 7 ) & ~7;
      unsigned bytes_to_copy = peek(src, bitptr, 16, "bytes_to_copy");
      unsigned clength = peek(src, bitptr + 16, 16, "store length check");
      bitptr += 32;

      if (bytes_to_copy != (clength^0xffff)) return ~0;
      if (dest + bytes_to_copy > dest_max) return ~0;
      if ((src + bitptr/8) + bytes_to_copy > src_max) return ~0;

      memcpy(dest, src + bitptr/8, bytes_to_copy);
      dest += bytes_to_copy;
      bitptr += bytes_to_copy * 8;
      return bitptr;
    }

    static unsigned decode_lz77(uint8_t *&dest, uint8_t *dest_max, const uint8_t *src, const uint8_t *src_max, unsigned bitptr, const huffman_table *table_) {
      for(;;) {
        if (src + bitptr/8 > src_max) return ~0;
        unsigned peek16 = peek(src, bitptr, 16, NULL);
        unsigned value = rev16(peek16);
        unsigned index = 0;
        while (value > table_->lit_limits[index]) {
          index++;
        }
        unsigned length = table_->min_lit_length + index;
        unsigned offset = ( value >> ( 16 - length ) );
        unsigned code = table_->lit_codes[offset - table_->lit_base[index]];
        bitptr += length;
        if (debug) dump_bits(peek16, length, "code");

        if (code < 256) {
          if (dest+1 > dest_max) return ~0;
          *dest++ = code;
          if (debug) printf("%02x\n", code);
        } else if (code == 256) {
          return bitptr;
        } else {
          unsigned block_length;
          unsigned distance;
          {
            if (debug) printf("[%d]\n", code);
            static const uint8_t extra[] = {
              0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
            };
            static const uint8_t base[] = {
              3-3, 4-3, 5-3, 6-3, 7-3, 8-3, 9-3, 10-3,
              11-3, 13-3, 15-3, 17-3, 19-3, 23-3, 27-3, 31-3,
              35-3, 43-3, 51-3, 59-3, 67-3, 83-3, 99-3, 115-3,
              131-3, 163-3, 195-3, 227-3, 258-3,
            };
            if (code-257 > sizeof(extra)) return ~0;
            unsigned extra_length = extra[ code-257 ];
            block_length = base[ code-257 ] + 3 + peek(src, bitptr, extra_length, "extra");
            bitptr += extra_length;
          }
          {
            //if (src + (bitptr + table_->max_dist_length)/8 > src_max ) return ~0;
            unsigned peek16 = peek(src, bitptr, 16, NULL);
            unsigned value = rev16(peek16);
            unsigned index = 0;
            while (value > table_->dist_limits[index]) {
              index++;
            }
            unsigned length = table_->min_dist_length + index;
            unsigned offset = ( value >> ( 16 - length ) );
            unsigned code = table_->dist_codes[offset - table_->dist_base[index]];
            bitptr += length;

            if (debug) printf("{%d}\n", code);
            static const uint8_t extra[] = {
              0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 0, 0, 0,
            };
            static const uint16_t base[] = {
              1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
            };
            if (code > sizeof(extra)) return ~0;
            unsigned extra_length = extra[ code ];
            distance = base[ code ] + peek(src, bitptr, extra_length, "extra");
            bitptr += extra_length;
          }

          if (debug) printf("length=%d distance=%d\n", block_length, distance);

          if (dest+block_length > dest_max) return ~0;

          for(unsigned i = 0; i != block_length; ++i) {
            dest[0] = dest[-(int)distance];
            dest++;
          }
        }
      }
    }

    unsigned decode_fixed(uint8_t *&dest, uint8_t *dest_max, const uint8_t *src, const uint8_t *src_max, unsigned bitptr) const {
      return decode_lz77(dest, dest_max, src, src_max, bitptr, &fixed_);
    }

    unsigned decode_variable(uint8_t *&dest, uint8_t *dest_max, const uint8_t *src, const uint8_t *src_max, unsigned bitptr) const {
      unsigned num_lit_codes = peek(src, bitptr, 5, "num_lit_codes") + 257;
      unsigned num_dist_codes = peek(src, bitptr+5, 5, "num_dist_codes") + 1;
      unsigned num_length_codes = peek(src, bitptr+10, 4, "num_length_codes") + 4;
      
      bitptr += 14;

      uint8_t lengths[288 + 32];
      memset(lengths, 0, 20);
      if (src + bitptr/8 + num_length_codes > src_max ) return ~0;
      for (unsigned i = 0; i != num_length_codes; ++i) {
        static const uint8_t order[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        lengths[order[i]] = peek(src, bitptr, 3, "length code lenghs");
        bitptr += 3;
      }
      
      uint16_t codes[19];
      uint16_t limits[18];
      uint16_t base[18];
      uint8_t min_length;
      uint8_t max_length;
      if (!build_huffman(lengths, 19, min_length, max_length, codes, limits, base)) return ~0;
      
      unsigned todo = num_lit_codes + num_dist_codes;
      for(unsigned done = 0; done < todo;) {
        if (src + bitptr/8 + max_length > src_max ) return ~0;
        unsigned peek16 = peek(src, bitptr, 16, NULL);
        unsigned value = rev16(peek16);
        unsigned index = 0;
        while (value > limits[index]) {
          index++;
        }
        unsigned length = min_length + index;
        unsigned offset = ( value >> ( 16 - length ) );
        unsigned code = codes[offset - base[index]];
        bitptr += length;
        if (debug) dump_bits(peek16, length, "length");
        //fprintf(source_.debug(), "code=%03x\n", code);
        unsigned copy = 1;
        if (code < 16) {
        } else if(code == 16) {
          if (src + (bitptr+2)/8 > src_max ) return ~0;
          copy = peek(src, bitptr, 2, NULL) + 3;
          bitptr += 2;
          if (done == 0) return ~0;
          code = lengths[ done-1 ];
        } else if(code == 17) {
          if (src + (bitptr+3)/8 > src_max ) return ~0;
          copy = peek(src, bitptr, 3, NULL) + 3;
          bitptr += 3;
          code = 0;
        } else if(code == 18) {
          if (src + (bitptr+7)/8 > src_max ) return ~0;
          copy = peek(src, bitptr, 7, NULL) + 11;
          bitptr += 7;
          code = 0;
        } else {
          return ~0;
        }
        if (done + copy > todo) return ~0;
        do {
          lengths[done++] = code;
        } while( --copy );
      }

      if (debug) printf("lengths done\n");

      huffman_table var;
      if(
        !build_huffman(lengths, num_lit_codes, var.min_lit_length, var.max_lit_length, var.lit_codes, var.lit_limits, var.lit_base) ||
        !build_huffman(lengths+num_lit_codes, num_dist_codes, var.min_dist_length, var.max_dist_length, var.dist_codes, var.dist_limits, var.dist_base)
      ) {
        return ~0;
      }
      return decode_lz77(dest, dest_max, src, src_max, bitptr, &var);
    }
  public:
    deflate_decoder() {
      uint8_t lit_lengths[288];
      uint8_t dist_lengths[32];
      memset(lit_lengths +   0, 8, 144 - 0);
      memset(lit_lengths + 144, 9, 256-144);
      memset(lit_lengths + 256, 7, 280-256);
      memset(lit_lengths + 280, 8, 288-280);
      memset(dist_lengths, 5, 32);
      build_huffman(lit_lengths, 288, fixed_.min_lit_length, fixed_.max_lit_length, fixed_.lit_codes, fixed_.lit_limits, fixed_.lit_base);
      build_huffman(dist_lengths, 32, fixed_.min_dist_length, fixed_.max_dist_length, fixed_.dist_codes, fixed_.dist_limits, fixed_.dist_base);
    }

    bool decode(uint8_t *dest, uint8_t *dest_max, const uint8_t *src, const uint8_t *src_max) const {
      unsigned bitptr = 0;
      unsigned is_last_block;

      // for each "deflate" block:
      do {
        // prevent (bitptr / 8) from overflowing (>512Mib)
        src += bitptr / 8;
        bitptr %= 8;

        // three bits determine kind and exit condition
        is_last_block = peek(src, bitptr, 1, "deflate last") != 0;
        unsigned kind = peek(src, bitptr + 1, 2, "deflate kind");

        bitptr += 3;
        switch (kind) {
        case 0: bitptr = decode_uncompressed(dest, dest_max, src, src_max, bitptr); break;
        case 1: bitptr = decode_fixed(dest, dest_max, src, src_max, bitptr); break;
        case 2: bitptr = decode_variable(dest, dest_max, src, src_max, bitptr); break;
        default: return false;
        }
      } while( !is_last_block && bitptr != ~0);
      if (debug) printf("%p %p\n", dest, dest_max);
      if (debug) printf("%p %p\n", src + bitptr / 8, src_max);
      return is_last_block && bitptr != ~0 && dest == dest_max;
    }
  };

}

#endif
