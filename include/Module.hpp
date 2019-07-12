//
//  Module.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Module_hpp
#define Module_hpp

#include "ASTNode.hpp"

#include <map>
#include <vector>

namespace bjou {

struct FrontEnd;

struct Module {
    std::string identifier;
    bool activated, activatedAsCT, filled;
    unsigned int n_lines;

    std::vector<ASTNode *> nodes, structs, ifaceDefs;

    MultiNode * multi;

    Module();

    void fill(std::vector<ASTNode *> & _nodes,
              std::vector<ASTNode *> & _structs);
    void activate(Import * source, bool ct);
};

void importModulesFromAST(FrontEnd & frontEnd);
void importModuleFromFile(FrontEnd & frontEnd, const char * _fname);
} // namespace bjou

#endif /* Module_hpp */
