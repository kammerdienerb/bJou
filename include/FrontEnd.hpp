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
#include "Context.hpp"
#include "Macro.hpp"
#include "Module.hpp"
#include "Scope.hpp"
#include "Symbol.hpp"
#include "Type.hpp"

#include <map>
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
    std::set<ASTNode *> macros_need_fast_tracked_analysis;
    std::set<ASTNode *> non_run_non_fast_tracked_macros;
    bool stop_tracking_macros = false;
    std::vector<ASTNode *> structs, ifaceDefs, namespaces;
    std::unordered_map<std::string, const Type *> typeTable;
    std::unordered_map<std::string, const Type *> primativeTypeTable;
    Scope * globalScope;

    Struct * typeinfo_struct;
    Procedure * printf_decl;
    Procedure * malloc_decl;
    Procedure * free_decl;
    Procedure * memset_decl;
    Procedure * memcpy_decl;

    Procedure * __bjou_rt_init_def;

    bool abc = true;

    std::stack<ASTNode *> procStack;
    std::stack<const Type *> lValStack;
    std::set<std::string> modulesImported;
    std::map<std::string, Module *> modulesByID;
    std::map<std::string, Module *> modulesByPath;
    std::map<ASTNode::NodeKind, std::string> kind2string;
    MacroManager macroManager;

    std::unordered_map<std::string, unsigned int> interface_sort_keys;
    unsigned int getInterfaceSortKey(std::string);
    void fix_typeinfo_v_table_size();

    int n_nodes;
    size_t n_primatives;

    unsigned int n_lines = 0;

    milliseconds ctruntime;

    milliseconds go();

    milliseconds ParseStage();
    milliseconds ImportStage();
    milliseconds SymbolsStage();
    milliseconds TypesStage();
    milliseconds AnalysisStage();
    // milliseconds DesugarStage();
    // milliseconds ModuleStage();

    std::string getBuiltinVoidTypeName() const;

    std::string makeUID(std::string hintID);
};
} // namespace bjou

#endif /* FrontEnd_hpp */
