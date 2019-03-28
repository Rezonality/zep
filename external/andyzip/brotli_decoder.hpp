////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2012-2016
//
//
// Brotli decoder.
//
// Note this is far from optimal but is relatively simple compared with the reference to understand.
// 
// Given time we could make this far faster.
//

#ifndef _ANDYZIP_BROTLI_DECODER_HPP_
#define _ANDYZIP_BROTLI_DECODER_HPP_

#include <andyzip/huffman_table.hpp>

#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <algorithm>

#include <andyzip/brotli_data.hpp>

namespace andyzip {
  struct brotli_decoder_state {
    enum class error_code {
      ok = 0,
      need_more_input = 1,
      syntax_error = 2,
      end = 3,
      huffman_length_error = 4,
      context_map_error = 5,
    };

    enum {
      max_types = 256,
      dump_bits = 0,
    };

    FILE *log_file = nullptr;
    const char *src = nullptr;
    std::uint32_t bitptr = 0;
    std::uint32_t bitptr_max = 0;
    char *dest = 0;
    char *dest_max = 0;
    error_code error;
    int max_backward_distance = 0;
    int num_types[3];
    int last_block_type[3];
    int block_type[3];
    int block_len[3];
    uint8_t context_mode[max_types];
    uint8_t literal_context_map[256]; // todo: what is the max size?
    uint8_t distance_context_map[256];
    uint64_t bytes_written = 0;
    std::vector<uint8_t> ring_buffer;
    andyzip::huffman_table<256+2> block_type_tables[3];
    andyzip::huffman_table<26> block_count_tables[3];

    int read(int bits) {
      int value = peek(bits);
      if (dump_bits) {
        fprintf(log_file, "[BrotliReadBits]  %d %d %d val: %6x\n", (bitptr_max - bitptr - bits)/8, 24+(bitptr&7), bits, value);
      }
      bitptr += bits;
      return value;
    }

    int peek(int bits) {
      //if (bitptr + bits > bitptr_max) { error = error_code::need_more_input; return 0; }
      auto i = bitptr >> 3, j = bitptr & 7;
      int value = (int)( (unsigned&)src[i] >> j ) & ( (1u << bits) - 1 );
      return value;
    }

    void drop(int bits) {
      bitptr += bits;
    }
  };

  class brotli_decoder {
    enum {
      debug = 0,
      window_gap = 16,
      literal_context_bits = 6,
      distance_context_bits = 2,
      block_len_symbols = 26,

      num_distance_short_codes = 16,
    };
    typedef brotli_decoder_state::error_code error_code;

    enum {
      idx_L, idx_I, idx_D
    };

    static unsigned read_window_size(brotli_decoder_state &s) {
      auto w0 = s.read(1);
      if (s.error != error_code::ok) return 0; 
      if (w0 == 0) {
        return 16;
      }

      auto w13 = s.read(3);
      if (s.error != error_code::ok) return 0; 
      if (w13 != 0) {
        return w13 + 17;
      }

      auto w47 = s.read(4);
      if (s.error != error_code::ok) return 0; 

      return w47 ? w47 + 10 - 2 : 17;
    }

    // read a value from 1 to 256
    int read_256(brotli_decoder_state &s) {
      int nlt0 = s.read(1);
      if (s.error != error_code::ok) return 0; 

      if (!nlt0) return 1;

      int nlt14 = s.read(3);
      if (s.error != error_code::ok) return 0; 

      if (!nlt14) return 2;

      int nlt5x = s.read(nlt14);
      if (s.error != error_code::ok) return 0; 

      return (1 << nlt14) + nlt5x + 1;
    }

    // From reference inplementation.
    static int log2_floor(int x) {
      int result = 0;
      while (x) {
        x >>= 1;
        ++result;
      }
      return result;
    }

    // From 7.3.  Encoding of the Context Map
    static void inverse_move_to_front(uint8_t values[], int num_values) {
      uint8_t mtf[256];
      for (int i = 0; i < 256; ++i) {
        mtf[i] = (uint8_t)i;
      }
      for (int i = 0; i < num_values; ++i) {
        uint8_t index = values[i];
        uint8_t value = mtf[index];
        values[i] = value;
        for (; index; --index) {
          mtf[index] = mtf[index - 1];
        }
        mtf[0] = value;
      }
    }

    template <class Table>
    static void read_huffman_code(brotli_decoder_state &s, Table &table, int alphabet_size) {
      int code_type = s.read(2);
      if (s.error != error_code::ok) return;
      alphabet_size &= 1023;
      if (debug) fprintf(s.log_file, "[ReadHuffmanCode] s->sub_loop_counter = %d\n", code_type);
      if (code_type == 1) {
        // 3.4.  Simple Prefix Codes
        int num_symbols = s.read(2) + 1;
        int alphabet_bits = log2_floor(alphabet_size - 1);
        uint16_t symbols[Table::max_codes];
        for (int i = 0; i != num_symbols; ++i) {
          symbols[i] = (uint16_t)s.read(alphabet_bits);
          if (debug) fprintf(s.log_file, "[ReadSimpleHuffmanSymbols] s->symbols_lists_array[i] = %d\n", symbols[i]);
        }
        if (debug) fprintf(s.log_file, "[ReadHuffmanCode] s->symbol = %d\n", num_symbols-1);
        static const uint8_t simple_lengths[][4] = {
          {0},
          {1, 1},
          {1, 2, 2},
          {2, 2, 2, 2},
          {1, 2, 3, 3},
        };
        int tree_select = num_symbols == 4 ? s.read(1) : 0;
        table.init(simple_lengths[num_symbols - 1 + tree_select], symbols, num_symbols);
      } else {
        // 3.5.  Complex Prefix Codes
        andyzip::huffman_table<18> complex_table;
        {
          uint8_t lengths[18] = {0};
          int space = 0;
          int num_codes = 0;
          for (int i = code_type; i != 18; ++i) {
            int bits = s.peek(4);
            if (s.error != error_code::ok) return;

            static const uint8_t kCodeLengthCodeOrder[18] = {
              1, 2, 3, 4, 0, 5, 17, 6, 16, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            };

            // Static prefix code for the complex code length code lengths.
            static const uint8_t kCodeLengthPrefixLength[16] = {
              2, 2, 2, 3, 2, 2, 2, 4, 2, 2, 2, 3, 2, 2, 2, 4,
            };

            static const uint8_t kCodeLengthPrefixValue[16] = {
              0, 4, 3, 2, 0, 4, 3, 1, 0, 4, 3, 2, 0, 4, 3, 5,
            };

            s.drop(kCodeLengthPrefixLength[bits]);
            uint8_t length = kCodeLengthPrefixValue[bits];
            lengths[kCodeLengthCodeOrder[i]] = length;
            if (debug) fprintf(s.log_file, "[ReadCodeLengthCodeLengths] s->code_length_code_lengths[%d] = %d\n", kCodeLengthCodeOrder[i], lengths[kCodeLengthCodeOrder[i]]);
            if (length) {
              ++num_codes;
              space += 32 >> length;
              if (space >= 32) break;
            }
          }
          if (num_codes != 1 && space != 32) {
            if (debug) fprintf(s.log_file, "bad\n");
            s.error = error_code::huffman_length_error;
            return;
          }

          complex_table.init(lengths, nullptr, 18);
          if (complex_table.invalid()) {
            if (debug) fprintf(s.log_file, "bad2\n");
            s.error = error_code::huffman_length_error;
            return;
          }
        }

        {
          int space = 0;
          uint8_t lengths[Table::max_codes];
          uint8_t nzcl = 8;
          int prev_code_len = 8;
          int repeat = 0;
          int repeat_code_len = 0;
          int i = 0;
          for(; i < alphabet_size && space < 32768;) {
            auto code = complex_table.decode(s.peek(16));
            s.drop(code.first);
            int code_len = code.second;
            if (code_len < 16) {
              if (code_len) {
                if (debug) fprintf(s.log_file, "[ReadHuffmanCode] code_length[%d] = %d\n", i, code_len);
              }
              if (i + 1 > alphabet_size) {
                s.error = error_code::huffman_length_error;
                if (debug) fprintf(s.log_file, "bad3\n");
                return;
              }
              lengths[i++] = (uint8_t)code_len;
              if (code_len) {
                space += 32768 >> code_len;
                prev_code_len = code_len;
              }
              repeat = 0;
            } else {
              int extra_bits = code_len == 16 ? 2 : 3;
              int new_len = code_len == 16 ? prev_code_len : 0;
              int repeat_delta = s.peek(extra_bits);
              s.drop(extra_bits);

              if (repeat_code_len != new_len) {
                repeat = 0;
                repeat_code_len = new_len;
              }

              int old_repeat = repeat;
              if (repeat > 0) {
                repeat -= 2;
                repeat <<= extra_bits;
              }

              repeat += repeat_delta + 3;
              repeat_delta = repeat - old_repeat;

              if (debug) fprintf(s.log_file, "[ReadHuffmanCode] code_length[%d..%d] = %d\n", i, i + repeat_delta - 1, new_len);
              if (i + repeat_delta > alphabet_size) {
                if (debug) fprintf(s.log_file, "bad4 %d %d %d\n", i, repeat_delta, alphabet_size);
                s.error = error_code::huffman_length_error;
                return;
              }
              for (int j = 0; j != repeat_delta; ++j) {
                lengths[i++] = new_len;
              }
              if (new_len) {
                space += (32768 >> new_len) * repeat_delta;
              }
            }
          }
          if (space > 32768) {
            if (debug) fprintf(s.log_file, "bad5\n");
            s.error = error_code::huffman_length_error;
            return;
          }
          memset(lengths + i, 0, alphabet_size - i);
          table.init(lengths, nullptr, alphabet_size);
          if (table.invalid()) {
            if (debug) fprintf(s.log_file, "bad6\n");
            s.error = error_code::huffman_length_error;
            return;
          }
        }
      }
    }

    static int read_block_length(brotli_decoder_state &s, int index) {
      unsigned code = s.peek(16);
      auto length_code = s.block_count_tables[index].decode(code);
      s.drop(length_code.first);
      int extra_bits = brotli_data::kBlockLengthPrefixCode[length_code.second].nbits;
      int value = s.read(extra_bits);
      return brotli_data::kBlockLengthPrefixCode[length_code.second].offset + value;
    }

    static void read_block_switch_command(brotli_decoder_state &s, int index) {
      int num_types = s.num_types[index];
      if (num_types == 1) return;

      //  read block type using HTREE_BTYPE_D and set BTYPE_D
      // 6.  Encoding of Block-Switch Commands
      unsigned bits = s.peek(16);
      auto length_code = s.block_type_tables[index].decode(bits);
      s.drop(length_code.first);
      int code = length_code.second;
      int cur = s.block_type[index];
      int last = s.last_block_type[index];

      int block_type = code == 0 ? last : code == 1 ? cur + 1 : code - 2;
      if (block_type > num_types) {
        block_type -= num_types;
      }

      //   save previous block type
      s.last_block_type[index] = cur;
      s.block_type[index] = block_type;

      //if (debug) fprintf(s.log_file, "block_type[%d] = {%d, %d}\n", index, cur, block_type);

      //  read block count using HTREE_BLEN_D and set BLEN_D
      s.block_len[index] = read_block_length(s, index);
    }

    static void read_context_map(brotli_decoder_state &s, uint8_t *context_map, int context_map_size, int num_trees) {
      if (debug) fprintf(s.log_file, "[DecodeContextMap] context_map_size = %d\n", context_map_size);
      if (debug) fprintf(s.log_file, "[DecodeContextMap] *num_htrees = %d\n", num_trees);

      // if NTREESL >= 2
      if (num_trees >= 2) {
        //  read literal context map, CMAPL[]
        int bits = s.peek(5);
        if (s.error != error_code::ok) return; 
        int rlemax = (bits & 1) ? (bits >> 1) + 1 : 0;
        s.drop((bits & 1) ? 5 : 1);
        if (debug) fprintf(s.log_file, "[DecodeContextMap] s->max_run_length_prefix = %d\n", rlemax);
        andyzip::huffman_table<256> table;
        read_huffman_code(s, table, num_trees + rlemax);
        if (s.error != error_code::ok) return;
        for (int i = 0; i != context_map_size;) {
          unsigned bits = s.peek(16);
          auto length_value = table.decode(bits);
          s.drop(length_value.first);
          int code = length_value.second;
          if (debug) fprintf(s.log_file, "[DecodeContextMap] code = %d\n", code);
          if (code == 0) {
            context_map[i++] = (uint8_t)code;
          } else if (code > rlemax) {
            context_map[i++] = (uint8_t)code - rlemax;
          } else {
            int repeat = s.read(code) + (1 << code);
            if (debug) fprintf(s.log_file, "[DecodeContextMap] reps = %d\n", repeat);
            if (i + repeat > context_map_size) {
              s.error = error_code::context_map_error;
              return;
            }
            for (int j = 0; j != repeat; ++j) {
              context_map[i++] = 0;
            }
          }
        }

        int imtf = s.read(1);
        if (s.error != error_code::ok) return; 
        if (imtf) {
          inverse_move_to_front(context_map, context_map_size);
        }
      } else {
        //  fill CMAPL[] with zeros
        std::fill(context_map, context_map + context_map_size, 0);
      }
    }

    int transform_dictionary_word(char *buffer, const uint8_t *src, int transform_idx, int copy_len, int ringbuffer_mask) {
      char *dest = buffer;
      auto &t = brotli_data::table[transform_idx];
      for (const char *psrc = t.prefix; *psrc; ++psrc) {
        *dest++ = *psrc;
      }

      if (t.id >= brotli_data::OmitLast1) {
        copy_len -= t.id - brotli_data::OmitLast1 + 1;
      } else if (t.id >= brotli_data::OmitFirst1) {
        copy_len -= t.id - brotli_data::OmitFirst1 + 1;
        src += t.id - brotli_data::OmitFirst1 + 1;
      }

      // fermentation (aka. case conversion)
      if (t.id == brotli_data::FermentFirst || t.id == brotli_data::FermentAll) {
        uint8_t fermented[24];
        if (copy_len <= 24) {
          memcpy(fermented, src, copy_len);
        }
  
        for (int i = 0; i < copy_len;) {
          uint8_t chr = src[i];
          if (chr < 192) {
            if (chr >= 97 && chr <= 122) {
              fermented[i] ^= 32;
            }
            ++i;
          } else if (chr < 224) {
            if (i + 1 < copy_len) {
              fermented[i+1] ^= 32;
            }
            i += 2;
          } else {
            if (i + 2 < copy_len) {
              fermented[i+2] ^= 32;
            }
            i += 3;
          }
          if (t.id == brotli_data::FermentFirst) break;
        }
        src = fermented;
      }

      for (int i = 0; i != copy_len; ++i) {
        *dest++ = *src++;
      }

      for (const char *dsrc = t.suffix; *dsrc; ++dsrc) {
        *dest++ = *dsrc;
      }

      return (int)(dest - buffer);
    }
  public:
    brotli_decoder() {
    }

    // decode some input bits and return -1 if we need more input.
    brotli_decoder_state::error_code decode(brotli_decoder_state &s) {
      typedef brotli_decoder_state::error_code error_code;
      // https://tools.ietf.org/html/rfc7932
      s.error = error_code::ok;

      // read window size
      unsigned lg_window_size = read_window_size(s);
      if (s.error != error_code::ok) return s.error;
      s.max_backward_distance = (1 << lg_window_size) - window_gap;
      if (debug) fprintf(s.log_file, "[BrotliDecoderDecompressStream] s->window_bits = %d\n", lg_window_size);
      if (debug) fprintf(s.log_file, "[BrotliDecoderDecompressStream] s->pos = %d\n", 0);

      s.ring_buffer.resize(1 << lg_window_size);
      int ringbuffer_mask = (1 << lg_window_size) - 1;

      //  do
      {
          // read ISLAST bit
          int is_last = s.read(1);
          if (s.error != error_code::ok) return s.error;

          // if ISLAST
          if (is_last) {
            //  read ISLASTEMPTY bit
            int is_last_empty = s.read(1);
            if (s.error != error_code::ok) return s.error;

            //  if ISLASTEMPTY break from loop
            if (is_last_empty) { s.error = error_code::end; return s.error; }
          }

          // read MNIBBLES
          int nibbles_code = s.read(2);
          if (s.error != error_code::ok) return s.error;

          // if MNIBBLES is zero
          int mlen = 0;
          if (nibbles_code == 3) {
            //  verify reserved bit is zero
            //  read MSKIPLEN
            //  skip any bits up to the next byte boundary
            //  skip MSKIPLEN bytes
            //  continue to the next meta-block
            s.error = error_code::syntax_error;
            return s.error;
          } else {
            //  read MLEN
            for (int i = 0; i != nibbles_code + 4; ++i) {
              int val = s.read(4);
              if (s.error != error_code::ok) return s.error;
              mlen |= val << (i*4);
            }
            ++mlen;
          }

          // if not ISLAST
          if (!is_last) {
            //  read ISUNCOMPRESSED bit
            int is_uncompressed = s.read(1);
            //  if ISUNCOMPRESSED
            if (is_uncompressed) {
              // skip any bits up to the next byte boundary
              // copy MLEN bytes of compressed data as literals
              // continue to the next meta-block
              s.error = error_code::syntax_error;
              return s.error;
            }
          }

          if (debug) fprintf(s.log_file, "[BrotliDecoderDecompressStream] s->is_last_metablock = %d\n", is_last);
          if (debug) fprintf(s.log_file, "[BrotliDecoderDecompressStream] s->meta_block_remaining_len = %d\n", mlen);
          if (debug) fprintf(s.log_file, "[BrotliDecoderDecompressStream] s->is_metadata = %d\n", 0);
          if (debug) fprintf(s.log_file, "[BrotliDecoderDecompressStream] s->is_uncompressed = %d\n", 0);

          enum {
            idx_L, idx_I, idx_D
          };

          // loop for each three block categories (i = L, I, D)
          for (int i = 0; i != 3; ++i) {
            //  read NBLTYPESi
            int nbltypesi = read_256(s);
            if (s.error != error_code::ok) return s.error;
            if (debug) fprintf(s.log_file, "[BrotliDecoderDecompressStream] s->num_block_types[s->loop_counter] = %d\n", nbltypesi);

            s.num_types[i] = nbltypesi;

            //  if NBLTYPESi >= 2
            if (nbltypesi >= 2) {
              // read prefix code for block types, HTREE_BTYPE_i
              read_huffman_code(s, s.block_type_tables[i], nbltypesi + 2);
              if (s.error != error_code::ok) return s.error;
              // read prefix code for block counts, HTREE_BLEN_i
              read_huffman_code(s, s.block_count_tables[i], block_len_symbols);
              if (s.error != error_code::ok) return s.error;
              // read block count, BLEN_i
              s.block_len[i] = read_block_length(s, i);
              // set block type, BTYPE_i to 0
              s.block_type[i] = 0;
              // initialize second-to-last and last block types to 0 and 1
              s.last_block_type[i] = 1;
              if (debug) fprintf(s.log_file, "[BrotliDecoderDecompressStream] s->block_length[s->loop_counter] = %d\n", s.block_len[i]);
            } else {
              // set block type, BTYPE_i to 0
              s.block_type[i] = 0;
              // set block count, BLEN_i to 16777216
              s.block_len[i] = 16777216;
            }
          }

          // read NPOSTFIX and NDIRECT
          int pbits = s.read(6);
          if (s.error != error_code::ok) return s.error;
          int NPOSTFIX = pbits & 3;
          int NDIRECT = (pbits >> 2) << NPOSTFIX;
          if (s.error != error_code::ok) return s.error;
          if (debug) fprintf(s.log_file, "[BrotliDecoderDecompressStream] s->num_direct_distance_codes = %d\n", NDIRECT + 16);
          if (debug) fprintf(s.log_file, "[BrotliDecoderDecompressStream] s->distance_postfix_bits = %d\n", NPOSTFIX);

          // read array of literal context modes, CMODE[]
          for (int i = 0; i != s.num_types[idx_L]; ++i) {
            int ctxt = s.read(2) * 2;
            if (s.error != error_code::ok) return s.error;
            s.context_mode[i & (brotli_decoder_state::max_types-1)] = ctxt;
            if (debug) fprintf(s.log_file, "[ReadContextModes] s->context_modes[%d] = %d\n", i, s.context_mode[i]);
          }

          // read NTREESL
          int num_literal_htrees = read_256(s);
          if (s.error != error_code::ok) return s.error;
          read_context_map(s, s.literal_context_map, s.num_types[idx_L] << literal_context_bits, num_literal_htrees);
          if (s.error != error_code::ok) return s.error;

          // read NTREESD
          int num_distance_htrees = read_256(s);
          if (s.error != error_code::ok) return s.error;
          read_context_map(s, s.distance_context_map, s.num_types[idx_D] << distance_context_bits, num_distance_htrees);
          if (s.error != error_code::ok) return s.error;

          // read array of literal prefix codes, HTREEL[]
          std::vector<andyzip::huffman_table<256>> literal_tables(num_literal_htrees);
          for (int i = 0; i != literal_tables.size(); ++i) {
            read_huffman_code(s, literal_tables[i], 256);
            if (s.error != error_code::ok) return s.error;
          }

          // read array of insert-and-copy length prefix codes, HTREEI[]
          std::vector<andyzip::huffman_table<704>> iandc_tables(s.num_types[idx_I]);
          for (int i = 0; i != iandc_tables.size(); ++i) {
            read_huffman_code(s, iandc_tables[i], 704);
            if (s.error != error_code::ok) return s.error;
          }

          // read array of distance prefix codes, HTREED[]
          static const int max_distance_alphabet_size = num_distance_short_codes + (15 << 3) + (48 << 3);
          std::vector<andyzip::huffman_table<256>> distance_tables(num_distance_htrees);
          int distance_alphabet_size = 16 + NDIRECT + (48 << NPOSTFIX);
          for (int i = 0; i != distance_tables.size(); ++i) {
            read_huffman_code(s, distance_tables[i], distance_alphabet_size);
            if (s.error != error_code::ok) return s.error;
          }

          // do
          int last_distances[4] = { 16, 15, 11, 4 };
          int last_distance_idx = 0;

          for (int pos = 0; pos < mlen; ) {
            //  if BLEN_I is zero
            if (s.block_len[idx_I] == 0) {
              read_block_switch_command(s, idx_I);
            }
            //  decrement BLEN_I
            s.block_len[idx_I]--;

            //  read insert-and-copy length symbol using HTREEI[BTYPE_I]
            int peek16 = s.peek(16);
            auto iandc = iandc_tables[s.block_type[idx_I]].decode(peek16);
            s.drop(iandc.first);

            //  compute insert length, ILEN, and copy length, CLEN
            brotli_data::CmdLutElement cmd = brotli_data::kCmdLut[iandc.second];
            int insert_len =
              cmd.insert_len_offset + 
              (cmd.insert_len_extra_bits ? s.read(cmd.insert_len_extra_bits) : 0)
            ;
            int copy_len =
              cmd.copy_len_offset + 
              (cmd.copy_len_extra_bits ? s.read(cmd.copy_len_extra_bits) : 0)
            ;
            //if (debug) fprintf(s.log_file, "{%d %d %d %d} %d\n", last_distances[0], last_distances[1], last_distances[2], last_distances[3], last_distance_idx);
            if (debug) fprintf(s.log_file, "[ProcessCommandsInternal] pos = %d insert = %d copy = %d\n", pos, insert_len, copy_len);

            //  loop for ILEN
            int p2 = pos < 2 ? 0 : s.ring_buffer[(pos - 2) & ringbuffer_mask];
            int p1 = pos < 1 ? 0 : s.ring_buffer[(pos - 1) & ringbuffer_mask];
            for (int i = 0; i != insert_len && pos < mlen; ++i) {
              // if BLEN_L is zero
              if (s.block_len[idx_L] == 0) {
                read_block_switch_command(s, idx_L);
              }
              // decrement BLEN_L
              s.block_len[idx_L]--;

              // look up context mode CMODE[BTYPE_L]
              uint8_t cmode = s.context_mode[s.block_type[idx_L]];

              // compute context ID, CIDL from last two uncompressed bytes
              // 7.1.  Context Modes and Context ID Lookup for Literals
              // For LSB6:    Context ID = p1 & 0x3f
              // For MSB6:    Context ID = p1 >> 2
              // For UTF8:    Context ID = Lut0[p1] | Lut1[p2]
              // For Signed:  Context ID = (Lut2[p1] << 3) | Lut2[p2]
              int context_id =
                cmode < 2 ? (p1 >> cmode*2) & 63 :
                cmode == 2 ? brotli_data::Lut0[p1] | brotli_data::Lut1[p2] : (brotli_data::Lut2[p1] << 3) | brotli_data::Lut2[p2]
              ;
              if (debug) fprintf(s.log_file, "[ProcessCommandsInternal] context = %d\n", context_id);

              // read literal using HTREEL[CMAPL[64*BTYPE_L + CIDL]]
              int peek16 = s.peek(16);
              if (debug) fprintf(s.log_file, "%04x\n", peek16);
              int table = s.literal_context_map[64 * s.block_type[idx_L] + context_id];
              if (debug) fprintf(s.log_file, "[ProcessCommandsInternal] s->context_map_slice[context] = %d\n", table);
              auto lit = literal_tables[table].decode(peek16);
              s.drop(lit.first);

              // write literal to uncompressed stream
              uint8_t value = (uint8_t)lit.second;
              if (debug) fprintf(s.log_file, "[ProcessCommandsInternal] s->ringbuffer[%d] = %d\n", pos, value);
              s.ring_buffer[pos & ringbuffer_mask] = value;
              p2 = p1;
              p1 = value;
              pos++;
            }

            // if number of uncompressed bytes produced in the loop for
            if (pos >= mlen) {
              // this meta-block is MLEN, then break from loop (in this
              // case the copy length is ignored and can have any value)
              break;
            }

            // 4.  Encoding of Distances

            // if distance code is implicit zero from insert-and-copy code
            int distance = 0;
            bool is_dictionary_ref = false;
            if (cmd.distance_code == 0) {
              // set backward distance to the last distance
              distance = last_distances[(last_distance_idx-1) & 3];
            } else {
              // if BLEN_D is zero
              if (s.block_len[idx_D] == 0) {
                read_block_switch_command(s, idx_D);
              }
              // decrement BLEN_D
              s.block_len[idx_D]--;

              // compute context ID, CIDD from CLEN
              // read distance code using HTREED[CMAPD[4*BTYPE_D + CIDD]]
              int peek16 = s.peek(16);
              int table = s.distance_context_map[4 * s.block_type[idx_D] + cmd.context];
              auto dist = distance_tables[table].decode(peek16);
              int dcode = dist.second;
              s.drop(dist.first);

              if (dcode < 16) {
                // compute distance by distance short code substitution
                uint8_t subst = brotli_data::distance_table[dcode];
                int base = last_distances[(last_distance_idx - (subst >> 4)) & 3];
                distance = base + (subst & 0x0f) - 4;
              } else if (dcode - NDIRECT - 16 < 0) {
                distance = dcode - 15;
              } else {
                int ndistbits = 1 + ((dcode - NDIRECT - 16) >> (NPOSTFIX + 1));
                int dextra = s.read(ndistbits);
                int POSTFIX_MASK = (1 << NPOSTFIX) - 1;
                int hcode = (dcode - NDIRECT - 16) >> NPOSTFIX;
                int lcode = (dcode - NDIRECT - 16) & POSTFIX_MASK;
                int offset = ((2 + (hcode & 1)) << ndistbits) - 4;
                distance = ((offset + dextra) << NPOSTFIX) + lcode + NDIRECT + 1;
              }

              is_dictionary_ref = distance > pos;

              // if distance code is not zero,
              if (dcode != 0 && !is_dictionary_ref) {
                //  and distance is not a static dictionary reference,
                //  push distance to the ring buffer of last distances
                last_distances[last_distance_idx++ & 3] = distance;
              }
            }
            if (insert_len) if (debug) fprintf(s.log_file, "[ProcessCommandsInternal] s->meta_block_remaining_len = %d\n", mlen - pos);
            if (debug) fprintf(s.log_file, "[ProcessCommandsInternal] pos = %d distance = %d\n", pos, distance);
  
            //  if distance is less than the max allowed distance plus one
            if (!is_dictionary_ref) {
              // move backwards distance bytes in the uncompressed data,
              // and copy CLEN bytes from this position to
              // the uncompressed stream
              for (int i = 0; i != copy_len; ++i) {
                s.ring_buffer[pos & ringbuffer_mask] = s.ring_buffer[(pos-distance) & ringbuffer_mask];
                ++pos;
              }
            } else {
              if (copy_len < 4 || copy_len > 24) {
                s.error = error_code::syntax_error;
                return s.error;
              }
              // look up the static dictionary word, transform the word as
              // directed, and copy the result to the uncompressed stream
              int offset = brotli_data::kBrotliDictionaryOffsetsByLength[copy_len];
              int word_id = distance - pos - 1;
              uint8_t shift = brotli_data::kBrotliDictionarySizeBitsByLength[copy_len];
              int word_idx = word_id & ((1 << shift)-1);
              int transform_idx = word_id >> shift;
              const uint8_t *src = brotli_data::kBrotliDictionary + offset + word_idx * copy_len;
              char buffer[128];
              int len = transform_dictionary_word(buffer, src, transform_idx, copy_len, ringbuffer_mask);
              if (debug) fprintf(s.log_file, "[ProcessCommandsInternal] dictionary word: [%.*s]\n", len, buffer);
              for (int i = 0; i != len; ++i) {
                s.ring_buffer[pos & ringbuffer_mask] = buffer[i];
                ++pos;
              }
            }
            if (debug) fprintf(s.log_file, "[ProcessCommandsInternal] s->meta_block_remaining_len = %d\n", mlen - pos);
          } // while number of uncompressed bytes for this meta-block < MLEN
          s.bytes_written += mlen;
      } //  while not ISLAST

      return s.error;
    }

  };

}

#endif
