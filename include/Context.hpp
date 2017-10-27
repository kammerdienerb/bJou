//
//  Context.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/4/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Context_h
#define Context_h

#include <string>

namespace bjou {

struct Loc {
    int line;
    int character;
};

struct Context {
    std::string filename;
    Loc begin;
    Loc end;

    Context();
    Context(std::string _filename, Loc _begin);
    void start(Context * context);
    void finish(Context * context, Context * wtspccontext = nullptr);
    Context lastchar();
};

Context diff(Context * c1, Context * c2 = nullptr);
} // namespace bjou

#define COMPILER_SRC_CONTEXT() Context(__FILE__, {__LINE__, 0})

#endif /* Context_h */
