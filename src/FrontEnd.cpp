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
static void fix_typeinfo_v_table_size(FrontEnd & frontEnd) {
    if (!compilation->max_interface_procs)
        return;

    BJOU_DEBUG_ASSERT(frontEnd.typeTable.count("typeinfo"));
    StructType * s_t = (StructType *)frontEnd.typeTable["typeinfo"];
    Struct * s = s_t->_struct;

    // Fix type
    Type * _v_table_t =
        (Type *)s_t->memberTypes[s_t->memberIndices["_v_table"]];
    BJOU_DEBUG_ASSERT(_v_table_t && _v_table_t->isArray());
    ArrayType * v_table_t = (ArrayType *)_v_table_t;
    BJOU_DEBUG_ASSERT(v_table_t->expression);
    BJOU_DEBUG_ASSERT(v_table_t->expression->nodeKind ==
                      ASTNode::INTEGER_LITERAL);
    v_table_t->expression = (Expression *)v_table_t->expression->clone();
    v_table_t->expression->setContents(
        std::to_string(compilation->max_interface_procs));
    v_table_t->size = compilation->max_interface_procs;

    // Fix original declaration
    VariableDeclaration * v_table_member = nullptr;
    for (ASTNode * _mem : s->getMemberVarDecls()) {
        VariableDeclaration * mem = (VariableDeclaration *)_mem;
        if (mem->getName() == "_v_table") {
            v_table_member = mem;
            break;
        }
    }
    BJOU_DEBUG_ASSERT(v_table_member && v_table_member->getTypeDeclarator());
    Declarator * _decl = (Declarator *)v_table_member->getTypeDeclarator();
    BJOU_DEBUG_ASSERT(_decl->nodeKind == ASTNode::ARRAY_DECLARATOR);
    ArrayDeclarator * decl = (ArrayDeclarator *)_decl;
    BJOU_DEBUG_ASSERT(decl->expression);
    BJOU_DEBUG_ASSERT(decl->expression->nodeKind == ASTNode::INTEGER_LITERAL);
    ASTNode * old = decl->getExpression();
    decl->setExpression(old->clone());
    delete old;
    Expression * expr = (Expression *)decl->getExpression();
    expr->setContents(std::to_string(compilation->max_interface_procs));
    decl->size = compilation->max_interface_procs;
}

FrontEnd::FrontEnd() : n_nodes(0), n_primatives(0) {}

FrontEnd::~FrontEnd() {
    for (ASTNode * node : AST)
        delete node;
}

milliseconds FrontEnd::go() {
    auto start = Clock::now();

    compilationAddPrimativeTypes();
    n_primatives = typeTable.size();
    typeTable.insert(primativeTypeTable.begin(), primativeTypeTable.end());

    bool time_arg = compilation->args.time_arg.getValue();

    auto p_time = ParseStage();
    if (time_arg) {
        size_t nfiles = compilation->args.files.getValue().size();
        prettyPrintTimeMin(p_time, "Parsed " + std::to_string(nfiles) +
                                       (nfiles == 1 ? " file" : " files"));
    }

    auto i_time = ImportStage();
    if (time_arg)
        prettyPrintTimeMin(i_time, "Import modules");

    auto s_time = SymbolsStage();
    if (time_arg)
        prettyPrintTimeMin(s_time, "Populate symbol tables");
    if (compilation->args.symbols_arg.getValue())
        printSymbolTables();

    auto t_time = TypesStage();
    if (time_arg)
        prettyPrintTimeMin(t_time, "Type completion");

    auto a_time = AnalysisStage();
    if (time_arg)
        prettyPrintTimeMin(a_time, "Semantic analysis");

    if (!compilation->args.nopreload_arg.isSet())
        fix_typeinfo_v_table_size(*this);

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

milliseconds FrontEnd::ParseStage() {
    auto start = Clock::now();

    unsigned int nthreaded = std::thread::hardware_concurrency() - 1;
    const std::vector<std::string> & fnames =
        compilation->args.files.getValue();
    size_t nfiles = fnames.size();

    std::unordered_map<std::string, milliseconds> times;

    if (nfiles > 1 && nthreaded > 0 &&
        !compilation->args.noparallel_arg.getValue()) {
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
                parserContainer.emplace_back(in, fname, false);
                in.close();
                futureTimes[fname] = std::async(
                    std::launch::async, std::ref(parserContainer.back()));
            }

            for (auto & ft : futureTimes)
                times[ft.first] = ft.second.get();

            // collect nodes
            for (AsyncParser & p : parserContainer) {
                AST.insert(AST.end(), p.nodes.begin(), p.nodes.end());
				structs.insert(structs.end(), p.structs.begin(), p.structs.end());
				ifaceDefs.insert(ifaceDefs.end(), p.ifaceDefs.begin(), p.ifaceDefs.end());
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
            parserContainer.emplace_back(in, fname, false);
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
			ifaceDefs.insert(ifaceDefs.end(), p.ifaceDefs.begin(), p.ifaceDefs.end());
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

    if (compilation->args.time_arg.getValue())
        for (auto & t : times)
            prettyPrintTimeMin(t.second, "    parsed " + t.first);

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

milliseconds FrontEnd::ImportStage() {
    auto start = Clock::now();

    if (!compilation->args.nopreload_arg.getValue())
        importModuleFromFile(*this, "__preload.bjou");

    importModulesFromAST(*this);

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

milliseconds FrontEnd::SymbolsStage() {
    auto start = Clock::now();

    globalScope = new Scope("global", nullptr);
   
	// @incomplete
	// does this work with namespaces? doubt it
	for (ASTNode * i : ifaceDefs)
		((InterfaceDef*)i)->preDeclare(globalScope);
	for (ASTNode * s : structs)
		((Struct*)s)->preDeclare(globalScope);

	for (ASTNode * i : ifaceDefs)
		((InterfaceDef*)i)->addSymbols(globalScope);
	for (ASTNode * s : structs)
		((Struct*)s)->addSymbols(globalScope);
    
	for (ASTNode * node : AST) {
		if (node->nodeKind != ASTNode::STRUCT && node->nodeKind != ASTNode::INTERFACE_DEF)
        	node->addSymbols(globalScope);
	}

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

milliseconds FrontEnd::TypesStage() {
    auto start = Clock::now();

    std::vector<const Type *> nonPrimatives;
    nonPrimatives.reserve(typeTable.size() - n_primatives);

    for (auto & t : typeTable) {
        BJOU_DEBUG_ASSERT(t.second->isValid());
        if (t.second->isStruct() || t.second->isAlias()) // @incomplete
            nonPrimatives.push_back(t.second);
    }

    nonPrimatives = typesSortedByDepencencies(nonPrimatives);
    for (const Type * t : nonPrimatives) {
        if (t->isStruct())
            ((StructType *)t)->complete();
        else if (t->isAlias())
            ((AliasType *)t)->complete();
    }

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

milliseconds FrontEnd::AnalysisStage() {
    auto start = Clock::now();

    for (ASTNode * node : AST) {
        node->parent = nullptr;
        node->replace = rpget<replacementPolicy_Global_Node>();
        node->analyze();
    }

    for (ASTNode * node : deferredAST) {
        // node->parent = nullptr;
        // node->replace = replacementPolicy_Global_Node;
        node->analyze();
        AST.push_back(node);
    }

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

/*
milliseconds FrontEnd::ModuleStage() {
    auto start = Clock::now();

    exportModule(AST);

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}*/

unsigned int FrontEnd::getInterfaceSortKey(std::string key) {
    static unsigned int n = 0;

    if (interface_sort_keys.count(key))
        return interface_sort_keys[key];

    interface_sort_keys[key] = n;

    return n++;
}

std::string FrontEnd::getBuiltinVoidTypeName() const { return "void"; }

std::string FrontEnd::makeUID(std::string hintID) {
    static int counter = 0;
    return "__bjou_" + hintID + std::to_string(counter++);
}
} // namespace bjou
