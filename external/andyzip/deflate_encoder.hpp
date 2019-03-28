////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2012-2016
//
// 


#ifndef MINIZIP_DEFLATE_ENCODER_INCLUDED
#define MINIZIP_DEFLATE_ENCODER_INCLUDED

#include <algorithm>

namespace andyzip {
  template <class CharType=uint8_t, class AddrType=uint32_t, class Allocator=std::allocator<char>>
  class suffix_array {

  public:
    typedef AddrType addr_type;
    typedef CharType char_type;

    suffix_array(const char_type *src, const char_type *src_max) {
      size_t size = src_max - src;

      struct sorter_t {
        addr_type group;
        addr_type next_group;
        addr_type addr;
      };

      std::vector<sorter_t, Allocator> sorter(size + 1);
      addr_to_sa_.resize(size + 1);

      for (size_t i = 0; i != size+1; ++i) {
        sorter_t &s = sorter[i];
        s.group = i == size ? 0 : src[i];
        s.next_group = i == size ? 0 : 1;
        s.addr = (addr_type)i;
      }

      auto sort_by_group_and_next = [](const sorter_t &a, const sorter_t &b) {
        return a.group < b.group + (a.next_group < b.next_group);
      };

      auto debug_dump = [src, &sorter](const char *msg, size_t h) {
        printf("\n%s h=%d\n", msg, (int)h);
        for (size_t i = 0; i != sorter.size(); ++i) {
        //for (size_t i = 0; i != 4; ++i) {
          auto &s = sorter[i];
          printf("%4d %12d %4d %4d %s\n", (int)i, (int)s.group, (int)s.next_group, (int)s.addr, (char*)src + s.addr);
        }
      };

      // todo: limit work to parts of the array not yet complete.
      constexpr bool debug = false;
      for (size_t h = 1; ; h *= 2) {
        // sort by group
        std::sort(sorter.begin(), sorter.end(), sort_by_group_and_next);

        int finished = 1;
        for (size_t i = 0; i != size+1;) {
          auto si = sorter[i];
          size_t j = i+1;
          sorter[i].group = (addr_type)i;
          int done = 0;
          for (; j != size+1 && sorter[j].group == si.group && sorter[j].next_group == si.next_group; ++j) {
            sorter[j].group = (addr_type)i;
            done = 1;
          }
          finished &= !done;
          i = j;
        }

        if (debug) debug_dump("built group numbers", h);

        for (size_t i = 0; i != size+1; ++i) {
          addr_to_sa_[sorter[i].addr] = i;
        }

        if (finished) break;

        for (size_t i = 0; i != size+1; ++i) {
          sorter_t &s = sorter[i];
          s.next_group = s.addr < size - h ? sorter[addr_to_sa_[s.addr + h]].group : 0;
        }

        if (debug) debug_dump("set next_group", h);
      }

      addresses_.resize(size+1);
      for (size_t i = 0; i != size+1; ++i) {
        addresses_[i] = sorter[i].addr;
      }

      sorter.resize(0);
      sorter.shrink_to_fit();

      // Kasai, T.; Lee, G.; Arimura, H.; Arikawa, S.; Park, K. (2001). Linear-Time Longest-Common-Prefix Computation in Suffix Arrays and Its Applications.
      // Proceedings of the 12th Annual Symposium on Combinatorial Pattern Matching. Lecture Notes in Computer Science. 2089. pp. 181–192. doi:10.1007/3-540-48194-X_17. ISBN 978-3-540-42271-6.
      longest_common_prefix_.resize(size+1);
      addr_type h = 0;
      for (size_t i = 0; i != sorter.size(); ++i) {
        addr_type r = addr_to_sa_[i];
        if (r > 0) {
          addr_type j = addresses_[r-1];
          while (src[i+h] == src[j+h]) {
            ++h;
          }
          longest_common_prefix_[r] = h;
          h -= h > 0;
        }
      }
    }

    auto addr(size_t i) const { return addresses_[i]; }
    auto lcp(size_t i) const { return longest_common_prefix_[i]; }
    auto rank(size_t i) const { return addr_to_sa_[i]; }
  private:

    std::vector<addr_type, Allocator> addresses_;
    std::vector<addr_type, Allocator> longest_common_prefix_;
    std::vector<addr_type, Allocator> addr_to_sa_;
  };

  class old_suffix_array {
  public:
    typedef uint32_t addr_type;

    struct sorter_t {
      addr_type group;
      addr_type next_group;
      addr_type addr;
    };

    void assign(const uint8_t *src, const uint8_t *src_max) {
      size_t size = src_max - src;

      sorter.resize(size + 1);
      for (size_t i = 0; i != size+1; ++i) {
        sorter_t &s = sorter[i];
        s.group = i == size ? 0 : src[i];
        s.next_group = i == size ? 0 : 1;
        s.addr = (addr_type)i;
      }

      auto sort_by_address = [](const sorter_t &a, const sorter_t &b) { return a.addr < b.addr; };

      auto sort_by_group = [](const sorter_t &a, const sorter_t &b) {
        return a.group < b.group;
      };

      auto sort_by_group_and_next = [](const sorter_t &a, const sorter_t &b) {
        return a.group < b.group + (a.next_group < b.next_group);
      };

      auto debug_dump = [this, src](const char *msg, size_t h) {
        printf("\n%s h=%d\n", msg, (int)h);
        for (size_t i = 0; i != sorter.size(); ++i) {
        //for (size_t i = 0; i != 4; ++i) {
          auto &s = sorter[i];
          size_t acp = 0;
          if (i != 0) {
            auto p = src + s.addr;
            auto q = src + sorter[i-1].addr;
            while (*p && *p == *q) { ++p; ++q; ++acp; }
          }
          printf("%4d %12d %4d %4d %4d %s\n", (int)i, (int)s.group, (int)s.next_group, (int)s.addr, (int)acp, (char*)src + s.addr);
        }
      };

      bool debug = true;
      for (size_t h = 1; ; h *= 2) {
        // sort by group    
        std::sort(sorter.begin(), sorter.end(), sort_by_group_and_next);
    
        int finished = 1;
        for (size_t i = 0; i != size+1;) {
          auto si = sorter[i];
          size_t j = i+1;
          sorter[i].group = (addr_type)i;
          int done = 0;
          for (; j != size+1 && sorter[j].group == si.group && sorter[j].next_group == si.next_group; ++j) {
            sorter[j].group = (addr_type)i;
            done = 1;
          }
          finished &= !done;
          i = j;
        }

        if (debug) debug_dump("built group numbers", h);

        if (finished) break;
    
        // invert the sort, restoring the original order of the sequence
        std::sort(sorter.begin(), sorter.end(), sort_by_address);

        // avoid random access to the string by using a sort.                
        for (size_t i = 0; i != size+1; ++i) {
          sorter_t &s = sorter[i];
          s.next_group = i < size - h ? sorter[i+h].group : 0;
        }
    
        //if (debug) debug_dump("set next_group", h);
      }


      // Kasai, T.; Lee, G.; Arimura, H.; Arikawa, S.; Park, K. (2001). Linear-Time Longest-Common-Prefix Computation in Suffix Arrays and Its Applications.
      // Proceedings of the 12th Annual Symposium on Combinatorial Pattern Matching. Lecture Notes in Computer Science. 2089. pp. 181–192. doi:10.1007/3-540-48194-X_17. ISBN 978-3-540-42271-6.
      auto pos = [this](size_t i) -> addr_type & { return sorter[i].addr; };
      auto rank = [this](size_t i) -> addr_type & { return sorter[i].group; };
      auto height = [this](size_t i) -> addr_type & { return sorter[i].next_group; };

      for (size_t i = 0; i != sorter.size(); ++i) {
        rank(pos(i)) = (addr_type)i;
      }

      addr_type h = 0;
      for (size_t i = 0; i != sorter.size(); ++i) {
        addr_type r = rank(i);
        if (r > 0) {
          addr_type j = sorter[r-1].addr;
          while (src[i+h] == src[j+h]) {
            ++h;
          }
          height(r) = h;
          h -= h > 0;
        }
      }

      //std::sort(sorter.begin(), sorter.end(), sort_by_address);
      debug_dump("final", 1);
    }

    sorter_t &operator[](size_t i) { return sorter[i]; }
    size_t size() const { return sorter.size(); }
  private:

    std::vector<sorter_t> sorter;
  };

  class deflate_encoder {
  public:
    deflate_encoder() {
    }

    bool encode(uint8_t *dest, uint8_t *dest_max, const uint8_t *src, const uint8_t *src_max) const {
      size_t block_size = 0x10000;
      FILE *log = fopen("1.txt", "wb");

      while (src != src_max) {
        size_t size = std::min((size_t)(src_max - src), block_size);
        suffix_array<uint8_t, uint32_t> sa(src, src + size);
        for (size_t i = 0; i != size; ++i) {
          size_t addr = sa.addr(i);
          char buf[12];
          size_t k = 0;
          for (size_t j = std::max(1u, addr)-1; k != 10 && j != size; ++j) {
            buf[k++] = src[j] < ' ' || src[j] >= 0x7f  ? '.' : src[j];
          }
          buf[k] = 0;
          fprintf(log, "%8d <%s>\n", i, buf);
        }
        break;
        src += size;
      }
      return false;
    }
  private:
  };
}

#endif
