//
//  Compile.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/8/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "FrontEnd.hpp"
#include "CLI.hpp"
#include "Misc.hpp"
#include "Module.hpp"
#include "Parser.hpp"
#include "Type.hpp"

#include <fstream>
#include <functional>
#include <future>

#ifdef BJOU_DEBUG_BUILD
#define SAVE_BJOU_DEBUG_BUILD
#endif
#undef BJOU_DEBUG_BUILD
#include "tclap/CmdLine.h"
#ifdef SAVE_BJOU_DEBUG_BUILD
#define BJOU_DEBUG_BUILD
#endif

namespace bjou {

FrontEnd::FrontEnd()
    : printf_decl(nullptr), malloc_decl(nullptr),
      free_decl(nullptr), memset_decl(nullptr), memcpy_decl(nullptr),
      n_nodes(0), n_primatives(0) {

    ctruntime = milliseconds::zero();

    kind2string[ASTNode::NodeKind::NONE] = "NONE";
    kind2string[ASTNode::NodeKind::PROC_SET] = "PROC_SET";
    kind2string[ASTNode::NodeKind::EXPRESSION] = "EXPRESSION";
    kind2string[ASTNode::NodeKind::BINARY_EXPRESSION] = "BINARY_EXPRESSION";
    kind2string[ASTNode::NodeKind::ADD_EXPRESSION] = "ADD_EXPRESSION";
    kind2string[ASTNode::NodeKind::SUB_EXPRESSION] = "SUB_EXPRESSION";
    kind2string[ASTNode::NodeKind::MULT_EXPRESSION] = "MULT_EXPRESSION";
    kind2string[ASTNode::NodeKind::DIV_EXPRESSION] = "DIV_EXPRESSION";
    kind2string[ASTNode::NodeKind::MOD_EXPRESSION] = "MOD_EXPRESSION";
    kind2string[ASTNode::NodeKind::ASSIGNMENT_EXPRESSION] =
        "ASSIGNMENT_EXPRESSION";
    kind2string[ASTNode::NodeKind::ADD_ASSIGN_EXPRESSION] =
        "ADD_ASSIGN_EXPRESSION";
    kind2string[ASTNode::NodeKind::SUB_ASSIGN_EXPRESSION] =
        "SUB_ASSIGN_EXPRESSION";
    kind2string[ASTNode::NodeKind::MULT_ASSIGN_EXPRESSION] =
        "MULT_ASSIGN_EXPRESSION";
    kind2string[ASTNode::NodeKind::DIV_ASSIGN_EXPRESSION] =
        "DIV_ASSIGN_EXPRESSION";
    kind2string[ASTNode::NodeKind::MOD_ASSIGN_EXPRESSION] =
        "MOD_ASSIGN_EXPRESSION";
    kind2string[ASTNode::NodeKind::MAYBE_ASSIGN_EXPRESSION] =
        "MAYBE_ASSIGN_EXPRESSION";
    kind2string[ASTNode::NodeKind::LSS_EXPRESSION] = "LSS_EXPRESSION";
    kind2string[ASTNode::NodeKind::LEQ_EXPRESSION] = "LEQ_EXPRESSION";
    kind2string[ASTNode::NodeKind::GTR_EXPRESSION] = "GTR_EXPRESSION";
    kind2string[ASTNode::NodeKind::GEQ_EXPRESSION] = "GEQ_EXPRESSION";
    kind2string[ASTNode::NodeKind::EQU_EXPRESSION] = "EQU_EXPRESSION";
    kind2string[ASTNode::NodeKind::NEQ_EXPRESSION] = "NEQ_EXPRESSION";
    kind2string[ASTNode::NodeKind::LOG_AND_EXPRESSION] = "LOG_AND_EXPRESSION";
    kind2string[ASTNode::NodeKind::LOG_OR_EXPRESSION] = "LOG_OR_EXPRESSION";
    kind2string[ASTNode::NodeKind::SUBSCRIPT_EXPRESSION] =
        "SUBSCRIPT_EXPRESSION";
    kind2string[ASTNode::NodeKind::CALL_EXPRESSION] = "CALL_EXPRESSION";
    kind2string[ASTNode::NodeKind::ACCESS_EXPRESSION] = "ACCESS_EXPRESSION";
    kind2string[ASTNode::NodeKind::UNARY_PRE_EXPRESSION] =
        "UNARY_PRE_EXPRESSION";
    kind2string[ASTNode::NodeKind::NOT_EXPRESSION] = "NOT_EXPRESSION";
    kind2string[ASTNode::NodeKind::DEREF_EXPRESSION] = "DEREF_EXPRESSION";
    kind2string[ASTNode::NodeKind::ADDRESS_EXPRESSION] = "ADDRESS_EXPRESSION";
    kind2string[ASTNode::NodeKind::REF_EXPRESSION] = "REF_EXPRESSION";
    kind2string[ASTNode::NodeKind::NEW_EXPRESSION] = "NEW_EXPRESSION";
    kind2string[ASTNode::NodeKind::DELETE_EXPRESSION] = "DELETE_EXPRESSION";
    kind2string[ASTNode::NodeKind::SIZEOF_EXPRESSION] = "SIZEOF_EXPRESSION";
    kind2string[ASTNode::NodeKind::UNARY_POST_EXPRESSION] =
        "UNARY_POST_EXPRESSION";
    kind2string[ASTNode::NodeKind::AS_EXPRESSION] = "AS_EXPRESSION";
    kind2string[ASTNode::NodeKind::IDENTIFIER] = "IDENTIFIER";
    kind2string[ASTNode::NodeKind::INITIALZER_LIST] = "INITIALZER_LIST";
    kind2string[ASTNode::NodeKind::BOOLEAN_LITERAL] = "BOOLEAN_LITERAL";
    kind2string[ASTNode::NodeKind::INTEGER_LITERAL] = "INTEGER_LITERAL";
    kind2string[ASTNode::NodeKind::FLOAT_LITERAL] = "FLOAT_LITERAL";
    kind2string[ASTNode::NodeKind::STRING_LITERAL] = "STRING_LITERAL";
    kind2string[ASTNode::NodeKind::CHAR_LITERAL] = "CHAR_LITERAL";
    kind2string[ASTNode::NodeKind::PROC_LITERAL] = "PROC_LITERAL";
    kind2string[ASTNode::NodeKind::EXTERN_LITERAL] = "EXTERN_LITERAL";
    kind2string[ASTNode::NodeKind::SOME_LITERAL] = "SOME_LITERAL";
    kind2string[ASTNode::NodeKind::NOTHING_LITERAL] = "NOTHING_LITERAL";
    kind2string[ASTNode::NodeKind::TUPLE_LITERAL] = "TUPLE_LITERAL";
    kind2string[ASTNode::NodeKind::_END_EXPRESSIONS] = "_END_EXPRESSIONS";
    kind2string[ASTNode::NodeKind::DECLARATOR] = "DECLARATOR";
    kind2string[ASTNode::NodeKind::ARRAY_DECLARATOR] = "ARRAY_DECLARATOR";
    kind2string[ASTNode::NodeKind::DYNAMIC_ARRAY_DECLARATOR] =
        "DYNAMIC_ARRAY_DECLARATOR";
    kind2string[ASTNode::NodeKind::POINTER_DECLARATOR] = "POINTER_DECLARATOR";
    kind2string[ASTNode::NodeKind::MAYBE_DECLARATOR] = "MAYBE_DECLARATOR";
    kind2string[ASTNode::NodeKind::TUPLE_DECLARATOR] = "TUPLE_DECLARATOR";
    kind2string[ASTNode::NodeKind::PROCEDURE_DECLARATOR] =
        "PROCEDURE_DECLARATOR";
    kind2string[ASTNode::NodeKind::REF_DECLARATOR] = "REF_DECLARATOR";
    kind2string[ASTNode::NodeKind::PLACEHOLDER_DECLARATOR] =
        "PLACEHOLDER_DECLARATOR";
    kind2string[ASTNode::NodeKind::_END_DECLARATORS] = "_END_DECLARATORS";
    kind2string[ASTNode::NodeKind::CONSTANT] = "CONSTANT";
    kind2string[ASTNode::NodeKind::VARIABLE_DECLARATION] =
        "VARIABLE_DECLARATION";
    kind2string[ASTNode::NodeKind::ALIAS] = "ALIAS";
    kind2string[ASTNode::NodeKind::STRUCT] = "STRUCT";
    kind2string[ASTNode::NodeKind::ENUM] = "ENUM";
    kind2string[ASTNode::NodeKind::ARG_LIST] = "ARG_LIST";
    kind2string[ASTNode::NodeKind::THIS] = "THIS";
    kind2string[ASTNode::NodeKind::PROCEDURE] = "PROCEDURE";
    kind2string[ASTNode::NodeKind::NAMESPACE] = "NAMESPACE";
    kind2string[ASTNode::NodeKind::IMPORT] = "IMPORT";
    kind2string[ASTNode::NodeKind::INCLUDE] = "INLCUDE";
    kind2string[ASTNode::NodeKind::USING] = "USING";
    kind2string[ASTNode::NodeKind::PRINT] = "PRINT";
    kind2string[ASTNode::NodeKind::RETURN] = "RETURN";
    kind2string[ASTNode::NodeKind::BREAK] = "BREAK";
    kind2string[ASTNode::NodeKind::CONTINUE] = "CONTINUE";
    kind2string[ASTNode::NodeKind::IF] = "IF";
    kind2string[ASTNode::NodeKind::ELSE] = "ELSE";
    kind2string[ASTNode::NodeKind::FOR] = "FOR";
    kind2string[ASTNode::NodeKind::WHILE] = "WHILE";
    kind2string[ASTNode::NodeKind::DO_WHILE] = "DO_WHILE";
    kind2string[ASTNode::NodeKind::MATCH] = "MATCH";
    kind2string[ASTNode::NodeKind::WITH] = "WITH";
    kind2string[ASTNode::NodeKind::TEMPLATE_DEFINE_LIST] =
        "TEMPLATE_DEFINE_LIST";
    kind2string[ASTNode::NodeKind::TEMPLATE_DEFINE_ELEMENT] =
        "TEMPLATE_DEFINE_ELEMENT";
    kind2string[ASTNode::NodeKind::TEMPLATE_DEFINE_TYPE_DESCRIPTOR] =
        "TEMPLATE_DEFINE_TYPE_DESCRIPTOR";
    kind2string[ASTNode::NodeKind::TEMPLATE_DEFINE_VARIADIC_TYPE_ARGS] =
        "TEMPLATE_DEFINE_VARIADIC_TYPE_ARGS";
    kind2string[ASTNode::NodeKind::TEMPLATE_DEFINE_EXPRESSION] =
        "TEMPLATE_DEFINE_EXPRESSION";
    kind2string[ASTNode::NodeKind::TEMPLATE_INSTANTIATION] =
        "TEMPLATE_INSTANTIATION";
    kind2string[ASTNode::NodeKind::TEMPLATE_ALIAS] = "TEMPLATE_ALIAS";
    kind2string[ASTNode::NodeKind::TEMPLATE_STRUCT] = "TEMPLATE_STRUCT";
    kind2string[ASTNode::NodeKind::TEMPLATE_PROC] = "TEMPLATE_PROC";
    kind2string[ASTNode::NodeKind::SL_COMMENT] = "SL_COMMENT";
    kind2string[ASTNode::NodeKind::MODULE_DECL] = "MODULE_DECL";
    kind2string[ASTNode::NodeKind::IGNORE] = "IGNORE";
    kind2string[ASTNode::NodeKind::MACRO_USE] = "MACRO_USE";
    kind2string[ASTNode::NodeKind::MULTINODE] = "MULTINODE";
}

FrontEnd::~FrontEnd() {
    for (ASTNode * node : AST)
        delete node;
}

milliseconds FrontEnd::go() {
    auto start = Clock::now();

    init_replacementPolicies();

    compilationAddPrimativeTypes();
    // not needed any more due to the way Type objects are created on demand now
    n_primatives = typeTable.size();
    typeTable.insert(primativeTypeTable.begin(), primativeTypeTable.end());

    bool time_arg = compilation->args.time_arg;

    auto p_time = ParseStage();
    if (time_arg) {
        size_t nfiles = compilation->args.files.size();
        prettyPrintTimeMin(p_time, "Parsed " + std::to_string(nfiles) +
                                       (nfiles == 1 ? " file" : " files"));
    }

    auto i_time = ImportStage();
    if (time_arg)
        prettyPrintTimeMin(i_time, "Import modules");

    auto s_time = SymbolsStage();
    if (time_arg)
        prettyPrintTimeMin(s_time, "Populate symbol tables");

    auto t_time = TypesStage();
    if (time_arg)
        prettyPrintTimeMin(t_time, "Type completion");

    auto a_time = AnalysisStage();
    if (time_arg)
        prettyPrintTimeMin(a_time - ctruntime, "Semantic analysis");

    if (compilation->args.symbols_arg)
        printSymbolTables();

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

milliseconds FrontEnd::ParseStage() {
    auto start = Clock::now();

    unsigned int nthreaded = std::thread::hardware_concurrency() - 1;

    std::vector<std::string> fnames;
    for (auto & f : compilation->args.files)
        if (has_suffix(f, ".bjou"))
            fnames.push_back(f);

    size_t nfiles = fnames.size();

    std::unordered_map<std::string, milliseconds> times;

    if (nfiles > 1 && nthreaded > 0 && !compilation->args.noparallel_arg) {
        // do threads
        std::vector<AsyncParser> parserContainer;
        parserContainer.reserve(nthreaded);
        std::unordered_map<std::string, std::future<milliseconds>> futureTimes;

        // parse files in batches of nthreaded
        unsigned int i;
        for (i = 0; i < nfiles / nthreaded; i += 1) {
            for (unsigned int j = 0; j < nthreaded; j += 1) {
                const std::string & fname = fnames[(i * nthreaded) + j];
                if (fname[0] == '-')
                    error(Context(), "Unrecognized option '" + fname + "'.");

                std::ifstream in(fname, std::ios::binary);
                if (!in)
                    error(Context(), "Unable to read file '" + fname + "'.");
                parserContainer.emplace_back(in, fname);
                in.close();
                futureTimes[fname] = std::async(
                    std::launch::async, std::ref(parserContainer.back()));
            }

            for (auto & ft : futureTimes)
                times[ft.first] = ft.second.get();

            // collect nodes
            for (AsyncParser & p : parserContainer) {
                AST.insert(AST.end(), p.nodes.begin(), p.nodes.end());
                structs.insert(structs.end(), p.structs.begin(),
                               p.structs.end());

                n_lines += p.n_lines;
            }

            futureTimes.clear();
            parserContainer.clear();
        }

        // parse remaining files
        for (unsigned long i = nfiles - (nfiles % nthreaded); i < nfiles;
             i += 1) {
            const std::string & fname = fnames[i];
            if (fname[0] == '-')
                error(Context(), "Unrecognized option '" + fname + "'.");

            std::ifstream in(fname, std::ios::binary);
            if (!in)
                error(Context(), "Unable to read file '" + fname + "'.");
            parserContainer.emplace_back(in, fname);
            in.close();
            futureTimes[fname] = std::async(std::launch::async,
                                            std::ref(parserContainer.back()));
        }

        // wait for and store the remaining times
        for (auto & ft : futureTimes)
            times[ft.first] = ft.second.get();

        // collect nodes
        for (AsyncParser & p : parserContainer) {
            AST.insert(AST.end(), p.nodes.begin(), p.nodes.end());
            structs.insert(structs.end(), p.structs.begin(), p.structs.end());

            n_lines += p.n_lines;
        }

    } else {
        for (const std::string & fname : fnames) {
            if (fname[0] == '-')
                error(Context(), "Unrecognized option '" + fname + "'.");

            auto f_start = Clock::now();

            std::ifstream in(fname, std::ios::binary);
            if (!in)
                error(Context(), "Unable to read file '" + fname + "'.");

            Parser parser(in, fname);
            in.close();

            times[fname] = duration_cast<milliseconds>(Clock::now() - f_start);
        }
    }

    if (compilation->args.time_arg)
        for (auto & t : times)
            prettyPrintTimeMin(t.second, "    parsed " + t.first);

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

milliseconds FrontEnd::ImportStage() {
    auto start = Clock::now();

    if (!compilation->args.nopreload_arg)
        importModuleFromFile(*this, "__preload.bjou");

    importModulesFromAST(*this);

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

milliseconds FrontEnd::SymbolsStage() {
    auto start = Clock::now();

    globalScope = new Scope("global", nullptr);

    /* @bad
     * We're going to force usings for some preload stuff.
     * We should instead use an 'include' kind of thing
     * and have __preload.bjou have using statements for
     * the modules it imports.. another day.
     */
    globalScope->usings.push_back("__ref");
    globalScope->usings.push_back("__errcode");
    globalScope->usings.push_back("null");
    globalScope->usings.push_back("__array");
    globalScope->usings.push_back("__slice");
    globalScope->usings.push_back("__dynamic_array");
    globalScope->usings.push_back("__string");
    globalScope->usings.push_back("__env");
    globalScope->usings.push_back("__panic");
    globalScope->usings.push_back("__c_support");

    std::string empty_mod_string = "";
    for (ASTNode * s : structs)
        ((Struct *)s)->preDeclare(empty_mod_string, globalScope);

    for (ASTNode * s : structs)
        ((Struct *)s)->addSymbols(empty_mod_string, globalScope);

    for (ASTNode * node : AST) {
        if (node->nodeKind != ASTNode::STRUCT)
            node->addSymbols(empty_mod_string, globalScope);
    }

    for (Identifier * ident : idents_out_of_order) {
        auto m_sym = ident->getScope()->getSymbol(ident->getScope(), ident, &ident->getContext(), true, false, false, false);
        if (m_sym) {
            Symbol * sym = nullptr;
            m_sym.assignTo(sym);
            BJOU_DEBUG_ASSERT(sym);
            if ((sym->isVar() || sym->isConstant())
            && (sym->node()->getScope()->parent && !sym->node()->getScope()->is_module_scope)) {
                errorl(ident->getContext(), "Premature use of identifier '" + sym->unmangled + "'.", false);
                errorl(sym->node()->getContext(), "'" + sym->unmangled + "' defined here.");
            }
        }
    }

    std::vector<Identifier*> empty;
    idents_out_of_order.swap(empty);

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

milliseconds FrontEnd::TypesStage() {
    auto start = Clock::now();

    std::vector<const Type *> nonPrimatives;
    nonPrimatives.reserve(typeTable.size() - n_primatives);

    for (auto & t : typeTable) {
        if (t.second->isStruct()) // || t.second->isAlias()) // @incomplete
            nonPrimatives.push_back(t.second);
    }

    /* nonPrimatives = typesSortedByDepencencies(nonPrimatives); */
    for (const Type * t : nonPrimatives) {
        ((StructType*)t)->checkForCycles();

        if (t->isStruct())
            ((StructType *)t)->complete();
        /*
        else if (t->isAlias())
            ((AliasType *)t)->complete();
        */
    }

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

milliseconds FrontEnd::AnalysisStage() {
    auto start = Clock::now();

    for (ASTNode * node : AST) {
        node->parent = nullptr;
        node->replace = rpget<replacementPolicy_Global_Node>();
    }

    // filter so that, for example, \static_if{ \static_if {...}...}
    // only runs on the outermost macro use
    auto filter_parent_child = [](std::set<ASTNode *> & nodes) {
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto node = nodes.begin(); node != nodes.end(); node++) {
                ASTNode * n = *node;
                ASTNode * parent = n->parent;
                while (parent) {
                    auto search = std::find(nodes.begin(), nodes.end(), parent);
                    if (search != nodes.end()) {
                        nodes.erase(node);
                        changed = true;
                        break;
                    }
                    parent = parent->parent;
                }
                if (changed)
                    break;
            }
        }
    };

    stop_tracking_macros = true;

    filter_parent_child(macros_need_fast_tracked_analysis);
    for (ASTNode * node : macros_need_fast_tracked_analysis)
        node->analyze();

    filter_parent_child(non_run_non_fast_tracked_macros);
    for (ASTNode * node : non_run_non_fast_tracked_macros)
        node->analyze();

    for (ASTNode * node : AST)
        node->analyze();

    while (deferredAST.size()) {
        std::vector<ASTNode*> hold_deferred = std::move(deferredAST);
        for (ASTNode * node : hold_deferred) {
            node->parent = nullptr;
            node->replace = rpget<replacementPolicy_Global_Node>();
            node->analyze();
            AST.push_back(node);
        }
    }

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

// milliseconds FrontEnd::DesugarStage() {
//     auto start = Clock::now();
//
//     for (ASTNode * node : AST) {
//         node->desugar();
//     }
//
//     auto end = Clock::now();
//     return duration_cast<milliseconds>(end - start);
// }

/*
milliseconds FrontEnd::ModuleStage() {
    auto start = Clock::now();

    exportModule(AST);

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}*/

std::string FrontEnd::getBuiltinVoidTypeName() const { return "void"; }

std::string FrontEnd::makeUID(std::string hintID) {
    static int counter = 0;
    return "__bjou_" + hintID + std::to_string(counter++);
}
} // namespace bjou
