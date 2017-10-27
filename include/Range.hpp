//
//  bRange.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/4/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef bRange_h
#define bRange_h

#include <cstddef>

namespace bjou {
template <typename T> class Range {
    T * begin_;
    T * end_;

  public:
    Range(T * first, T * last) : begin_{first}, end_{last} {}
    Range(T * first, std::ptrdiff_t size) : Range{first, first + size} {}

    T * begin() const noexcept { return begin_; }
    T * end() const noexcept { return end_ + 1; }

    T & operator[](const int index) { return begin_[index]; }
};
} // namespace bjou

#endif /* bRange_h */
