//
//  FrontEnd.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/8/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef FrontEnd_hpp
#define FrontEnd_hpp

#include "Compile.hpp"
#include "Macro.hpp"
#include "Scope.hpp"
#include "Symbol.hpp"
#include "Type.hpp"

#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

namespace bjou {
struct FrontEnd {
    FrontEnd();
    ~FrontEnd();

    std::vector<ASTNode *> AST;
    std::vector<ASTNode *> deferredAST;
    std::unordered_map<std::string, Type *> typeTable;
    std::unordered_map<std::string, Type *> primativeTypeTable;
    Scope * globalScope;
    std::stack<ASTNode *> procStack;
    std::stack<const Type *> lValStack;
    std::set<std::string> modulesImported;
    std::unordered_map<std::string, MacroDispatchInfo> macros;

    std::unordered_map<std::string, unsigned int> interface_sort_keys;
    unsigned int getInterfaceSortKey(std::string);

    int n_nodes;
    size_t n_primatives;

    milliseconds go();

    milliseconds ParseStage();
    milliseconds ImportStage();
    milliseconds SymbolsStage();
    milliseconds TypesStage();
    milliseconds AnalysisStage();
    // milliseconds ModuleStage();

    std::string getBuiltinVoidTypeName() const;

    std::string makeUID(std::string hintID);
};
} // namespace bjou

#endif /* FrontEnd_hpp */
