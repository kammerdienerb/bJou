//
//  BackEnd.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "BackEnd.hpp"
#include "Compile.hpp"
#include "Global.hpp"

namespace bjou {
BackEnd::BackEnd(FrontEnd & _frontEnd) : frontEnd(_frontEnd) {}
} // namespace bjou
