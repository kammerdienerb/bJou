//
//  Module.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "Module.hpp"
#include "CLI.hpp"
#include "FrontEnd.hpp"
#include "Misc.hpp"
#include "Parser.hpp"

#include "hash_table.h"

#ifdef BJOU_DEBUG_BUILD
#define SAVE_BJOU_DEBUG_BUILD
#endif
#undef BJOU_DEBUG_BUILD
#include "tclap/CmdLine.h"
#ifdef SAVE_BJOU_DEBUG_BUILD
#define BJOU_DEBUG_BUILD
#endif

#include <algorithm>
#include <deque>
#include <fstream>
#include <functional>
#include <future>
#include <utility>

namespace bjou {

Module::Module()
    : identifier(), activated(false), activatedAsCT(false), filled(false), n_lines(0),
      multi(nullptr) {}

void Module::fill(std::vector<ASTNode *> & _nodes,
                  std::vector<ASTNode *> & _structs) {
    if (filled)
        return;

    multi = new MultiNode;
    multi->isModuleContainer = true;

    nodes     = std::move(_nodes);
    structs   = std::move(_structs);
}

void Module::activate(Import * source, bool ct) {
    if (activated) {
        // printf("!!! R %d %s\n", ct, source->module.c_str());

        if (activatedAsCT && !ct) {
            // We need to tell the CT that encapsulates the
            // activated module import to ignore the multinode.
            MacroUse * encaps_ct = nullptr;
            ASTNode * p = multi->getParent();
            while (p) {
                if (p->nodeKind == ASTNode::MACRO_USE) {
                    MacroUse * use = (MacroUse *)p;
                    if (use->getMacroName() == "ct") {
                        encaps_ct = use;
                        break;
                    }
                }
                p = p->getParent();
            }

            BJOU_DEBUG_ASSERT(encaps_ct);
            encaps_ct->leaveMeAloneArgs.insert(multi);
            multi->setFlag(ASTNode::CT, false);
            activatedAsCT = false;
            
            std::vector<ASTNode*> sub_imports;
            multi->unwrap(sub_imports);
            for (ASTNode * node : sub_imports) {
                    // printf("HERE\n");
                if (node->nodeKind == ASTNode::IMPORT) {
                    Import * i = (Import*)node;
                    i->activate(false);
                    i->theModule->multi->setFlag(ASTNode::CT, false);
                }
            }
        }
    } else {
        // printf("!!!   %d %s\n", ct, source->module.c_str());

        activated = true;
        activatedAsCT = ct;

        BJOU_DEBUG_ASSERT(multi && multi->isModuleContainer);

        (*source->replace)(source->parent, source, multi);

        multi->take(nodes);
        multi->nodes.insert(multi->nodes.begin(), source);

        Scope * module_scope = new Scope("", compilation->frontEnd.globalScope, /* is_module_scope =*/ true, identifier);
        module_scope->description =
            "scope opened by module '" + identifier + "'";
        compilation->frontEnd.globalScope->scopes.push_back(module_scope);
        (*compilation->frontEnd.globalScope->module_scopes)[identifier] = module_scope;

        for (ASTNode * s : structs)
            ((Struct *)s)->preDeclare(identifier, module_scope);

        for (ASTNode * s : structs)
            ((Struct *)s)->addSymbols(identifier, module_scope);

        for (ASTNode * n : multi->nodes) {
            if (n->nodeKind != ASTNode::STRUCT) {
                n->addSymbols(identifier, module_scope);
            }
        }

        compilation->frontEnd.n_lines += n_lines;
    }
}

static void pushImportsFromAST(std::vector<ASTNode *> & AST,
                               std::deque<Import *> & imports) {
    for (ASTNode * node : AST) {
        std::vector<ASTNode *> terminals;
        node->unwrap(terminals);
        for (ASTNode * term : terminals) {
            if (term->nodeKind == ASTNode::IMPORT) {
                Import * import = (Import *)term;
                BJOU_DEBUG_ASSERT(import->getFlag(Import::FROM_PATH));
                imports.push_back(import);
            }
        }
    }
}

static void importModules(std::deque<Import *> imports, FrontEnd & frontEnd) {
    std::set<std::string> & modulesImported =
        compilation->frontEnd.modulesImported;
    std::set<std::string> filesSeen;
    hash_table_t<std::string, milliseconds, STRING_HASHER> times;
    hash_table_t<std::string, milliseconds, STRING_HASHER> peektimes;

    unsigned int nthreaded = std::thread::hardware_concurrency() - 1;

    if (nthreaded > 0 && !compilation->args.noparallel_arg) {
        std::vector<ImportParser *> parserContainer;
        std::unordered_map<std::string, std::future<milliseconds>, STRING_HASHER> futureTimes;
        parserContainer.reserve(nthreaded);

        while (!imports.empty()) {
            while (!imports.empty() && (imports.size() < nthreaded ||
                                        parserContainer.size() < nthreaded)) {
                Import * import = imports.front();
                imports.pop_front();
                std::string fname = de_quote(import->getModule());

                std::ifstream in;
                for (std::string & path : compilation->module_search_paths) {
                    in.open(path + fname);
                    if (in) {
                        fname = path + fname;
                        break;
                    }
                }

                if (in) {
                    import->full_path = fname;
                    if (filesSeen.find(fname) == filesSeen.end()) {
                        filesSeen.insert(fname);

                        ImportParser * parser = new ImportParser(in, fname);
                        parser->source = import;
                        in.close();
                        // go ahead and start
                        peektimes[fname] = (*parser)();

                        if (parser->mod_decl) {
                            Module * module = nullptr;
                            if (modulesImported.find(
                                    parser->mod_decl->getIdentifier()) ==
                                modulesImported.end()) {
                                modulesImported.insert(
                                    parser->mod_decl->getIdentifier());

                                module = new Module;
                                compilation->frontEnd.modulesByID
                                    [parser->mod_decl->getIdentifier()] =
                                    module;
                                compilation->frontEnd.modulesByPath[fname] =
                                    module;

                                parserContainer.push_back(parser);
                                futureTimes[fname] = std::async(
                                    std::launch::async,
                                    std::ref(*parserContainer.back()));
                            } else {
                                module =
                                    compilation->frontEnd.modulesByID
                                        [parser->mod_decl->getIdentifier()];

                                parser->Dispose();
                                delete parser;
                            }

                            BJOU_DEBUG_ASSERT(module);

                            module->identifier = parser->mod_decl->getIdentifier();

                            import->theModule = module;
                        } else {
                            import->notModuleError = true;
                        }
                    } else {
                        import->theModule =
                            compilation->frontEnd.modulesByPath[fname];
                    }
                } else {
                    import->fileError = true;
                }
            }

            for (auto & ft : futureTimes)
                times[ft.first] = peektimes[ft.first] + ft.second.get();

            // collect nodes
            for (ImportParser * p : parserContainer) {
                pushImportsFromAST(p->nodes, imports);

                Module * module = p->source->theModule;

                BJOU_DEBUG_ASSERT(module);

                module->fill(p->nodes, p->structs);

                module->n_lines = p->n_lines;

                delete p;
            }

            futureTimes.clear();
            parserContainer.clear();
        }
    } else { // single threaded
        while (!imports.empty()) {
            Import * import = imports.front();
            imports.pop_front();
            std::string fname = de_quote(import->getModule());

            if (filesSeen.find(fname) == filesSeen.end()) {
                std::ifstream in;
                for (std::string & path : compilation->module_search_paths) {
                    in.open(path + fname, std::ios::binary);
                    if (in) {
                        fname = path + fname;
                        break;
                    }
                }

                if (in) {
                    import->full_path = fname;
                    filesSeen.insert(fname);

                    ImportParser parser(in, fname);
                    parser.source = import;
                    in.close();
                    // go ahead and start
                    peektimes[fname] = parser();

                    if (parser.mod_decl) {
                        Module * module = nullptr;
                        if (modulesImported.find(
                                parser.mod_decl->getIdentifier()) ==
                            modulesImported.end()) {
                            modulesImported.insert(
                                parser.mod_decl->getIdentifier());
                            times[fname] =
                                peektimes[fname] + parser(); // continue

                            module = new Module;

                            compilation->frontEnd
                                .modulesByID[parser.mod_decl->getIdentifier()] =
                                module;
                            compilation->frontEnd.modulesByPath[fname] = module;

                            pushImportsFromAST(parser.nodes, imports);

                            module->fill(parser.nodes, parser.structs);

                            module->n_lines = parser.n_lines;
                        } else {
                            module = compilation->frontEnd.modulesByID
                                         [parser.mod_decl->getIdentifier()];

                            parser.Dispose();
                        }

                        BJOU_DEBUG_ASSERT(module);

                        module->identifier = parser.mod_decl->getIdentifier();

                        import->theModule = module;
                    } else {
                        import->notModuleError = true;
                    }
                } else {
                    import->fileError = true;
                }
            } else {
                import->theModule = compilation->frontEnd.modulesByPath[fname];
            }
        }
    }

    if (compilation->args.time_arg)
        for (auto & t : times)
            prettyPrintTimeMin(t.second, "    imported " + t.first);
}

void importModulesFromAST(FrontEnd & frontEnd) {
    std::deque<Import *> imports;
    pushImportsFromAST(frontEnd.AST, imports);
    importModules(imports, frontEnd);
}

void importModuleFromFile(FrontEnd & frontEnd, const char * _fname) {
    Import * import = new Import;
    import->setModule(_fname);
    import->setFlag(Import::FROM_PATH, true);

    import->replace = rpget<replacementPolicy_Global_Node>();

    std::vector<ASTNode*> & AST = compilation->frontEnd.AST;
    AST.push_back(import);

    // std::deque<Import *> imports{import};

    // importModules(imports, frontEnd);
}

} // namespace bjou
