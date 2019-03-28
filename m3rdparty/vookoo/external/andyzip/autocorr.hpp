
////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2012-2016
//
// Autocorrelation entropy test


#ifndef MINIZIP_ALGORITHM_INCLUDED
#define MINIZIP_ALGORITHM_INCLUDED

#include <cstdint>
#include <cstring>

namespace andyzip {
  template<class T>
  class range {
  public:
    range(T begin, T end) : begin_(begin), end_(end) {
    }
    T begin() { return begin_; }
    T end() { return end_; }
  private:
    T begin_;
    T end_;
  };


  // result[n] is the autocorrelation of sequence [begin,end)
  void autocorrelate(size_t *result, size_t num_results, const uint8_t *begin, const uint8_t *end) {
    for (size_t n : range((size_t)0, num_results)) {
      for (auto p = begin; p != end; p += n) {
        
      }
    }
    
  };

}

#endif
