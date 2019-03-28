
////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2012-2016
//
// Autocorrelation entropy test


#ifndef MINIZIP_ALGORITHM_INCLUDED
#define MINIZIP_ALGORITHM_INCLUDED

#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>

namespace andyzip {
  // Get a measure of how correlated the data is at various stride levels.
  void abs_correlation(std::vector<int> &result, const uint8_t *begin, const uint8_t *end, const std::initializer_list<int> &values) {
    result.clear();
    for (auto n : values) {
      int value = 0;
      for (auto p = begin; p < end - n; ++p) {
        value += std::abs((int8_t)(p[0] - p[n]));
      }
      result.push_back(value);
    }
  };

}

#endif
