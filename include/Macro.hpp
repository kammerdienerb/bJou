//
//  Macro.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/8/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Macro_hpp
#define Macro_hpp

#include "ASTNode.hpp"

#include <map>

namespace bjou {
typedef ASTNode * (*MacroDispatch_fn)(MacroUse *);

struct Macro {
    Macro();
    Macro(std::string _name, MacroDispatch_fn _dispatch,
          std::vector<std::vector<ASTNode::NodeKind>> _arg_kinds,
          bool _isVararg = false, std::vector<int> _args_no_add_symbols = {});

    std::string name;
    MacroDispatch_fn dispatch;
    std::vector<std::vector<ASTNode::NodeKind>> arg_kinds;
    std::vector<int> args_no_add_symbols;
    bool isVararg;
};

struct MacroManager {
    MacroManager();

    std::map<std::string, Macro> macros;

    ASTNode * invoke(MacroUse * use);
    bool shouldAddSymbols(MacroUse * use, int arg_index);
};
} // namespace bjou

#endif /* Macro_hpp */
