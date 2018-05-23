//
//  Defaults.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 5/23/18.
//  Copyright © 2018 me. All rights reserved.
//

#ifndef Defaults_hpp
#define Defaults_hpp

#include "Compile.hpp"
#include "FrontEnd.hpp"
#include "BackEnd.hpp"
#include "LLVMBackEnd.hpp"
#include "CLI.hpp"

#define DEFAULT_FE FrontEnd
#define DEFAULT_BE LLVMBackEnd

namespace bjou {
void StartDefaultCompilation(ArgSet& args);
}

#endif
