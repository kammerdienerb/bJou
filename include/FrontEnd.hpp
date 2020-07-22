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
#include <vector>

namespace bjou {
struct Include;

struct FrontEnd {
    FrontEnd();
    ~FrontEnd();

    std::vector<ASTNode *> AST;
    std::vector<ASTNode *> deferredAST;
    std::vector<Identifier*> idents_out_of_order;
    std::set<ASTNode *> macros_need_fast_tracked_analysis;
    std::set<ASTNode *> non_run_non_fast_tracked_macros;
    bool stop_tracking_macros = false;
    std::vector<ASTNode *> structs, namespaces;
    hash_table_t<std::string, const Type *, STRING_HASHER> typeTable;
    hash_table_t<std::string, const Type *, STRING_HASHER> primativeTypeTable;
    Scope * globalScope;

    Procedure * printf_decl;
    Procedure * malloc_decl;
    Procedure * free_decl;
    Procedure * memset_decl;
    Procedure * memcpy_decl;

    Procedure * __bjou_rt_init_def;

    bool abc = true;

    std::stack<ASTNode *> procStack;
    std::vector<ASTNode *> constantStack;
    std::stack<const Type *> lValStack;
    std::set<std::string> modulesImported;
    std::map<std::string, Module *> modulesByID;
    std::map<std::string, Module *> modulesByPath;
    std::vector<Include*> include_stack;
    std::vector<std::string> include_path_stack;
    std::map<ASTNode::NodeKind, std::string> kind2string;
    MacroManager macroManager;

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
