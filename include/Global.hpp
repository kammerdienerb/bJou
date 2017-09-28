//
//  Global.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/9/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Global_h
#define Global_h


#include <mutex>

namespace bjou {
    struct Compilation;
    extern Compilation* compilation;
}

using bjou::compilation;

extern std::mutex cli_mtx;

#endif /* Global_h */
