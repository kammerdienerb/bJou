//
//  BackEnd.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef BackEnd_h
#define BackEnd_h

#include "Compile.hpp"

namespace bjou {
struct FrontEnd;
struct Procedure;

struct BackEnd {
    BackEnd(FrontEnd & _frontEnd);

    FrontEnd & frontEnd;

    virtual void init() = 0;

    virtual milliseconds go() = 0;

    virtual void run(Procedure * proc) = 0;
};
} // namespace bjou

#endif /* BackEnd_h */
