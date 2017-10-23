//
//  bMaybe.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/9/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef bMaybe_h
#define bMaybe_h

#include <cstddef>
#include <type_traits>

namespace bjou {
template <typename T> class Maybe {
    bool _valid;
    T _value;

  public:
    Maybe() : _valid(false) {}
    Maybe(const T & __value) : _valid(true), _value(__value) {}

    operator bool() const { return _valid; }

    bool assignTo(T & _t) const {
        if (_valid) {
            _t = _value;
            return true;
        } else
            return false;
    }

    template <typename U> bool assignTo(U & _u) const {
        static_assert(
            std::is_base_of<typename std::remove_pointer<T>::type,
                            typename std::remove_pointer<U>::type>::value,
            "Type being assigned to is not related to type contained in "
            "bjou::Maybe.");

        if (_valid) {
            _u = (U)_value;
            return true;
        } else
            return false;
    }
};
} // namespace bjou

#endif /* bMaybe_h */
