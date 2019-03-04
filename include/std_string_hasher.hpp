//
//  std_string_hasher.hpp
//  bJou
//
//  Created by Brandon Kammerdiener on 3/2/19.
//

#ifndef std_string_hasher_hpp
#define std_string_hasher_hpp 

#include "bJouDemangle.h"

#include <string>

namespace bjou {

struct std_string_hasher {
    size_t operator() (std::string const& key) const {
        return bJouDemangle_hash(key.c_str());
    }
};

}

#endif
