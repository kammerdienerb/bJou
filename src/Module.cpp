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
    : activated(false), activatedAsCT(false), filled(false), n_lines(0),
      multi(nullptr) {}

void Module::fill(std::vector<ASTNode *> & _nodes,
                  std::vector<ASTNode *> & _structs,
                  std::vector<ASTNode *> & _ifaceDefs) {
    if (filled)
        return;

    multi = new MultiNode;
    multi->isModuleContainer = true;

    nodes = std::move(_nodes);
    structs = std::move(_structs);
    ifaceDefs = std::move(_ifaceDefs);
}

void Module::activate(Import * source, bool ct) {
    if (activated) {
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
        }

        return;
    }

    activated = true;
    activatedAsCT = ct;

    BJOU_DEBUG_ASSERT(multi && multi->isModuleContainer);

    (*source->replace)(source->parent, source, multi);

    multi->take(nodes);

    for (ASTNode * i : ifaceDefs)
        ((InterfaceDef *)i)->preDeclare(source->getScope());
    for (ASTNode * s : structs)
        ((Struct *)s)->preDeclare(source->getScope());

    for (ASTNode * i : ifaceDefs)
        ((InterfaceDef *)i)->addSymbols(source->getScope());
    for (ASTNode * s : structs)
        ((Struct *)s)->addSymbols(source->getScope());

    for (ASTNode * node : multi->nodes) {
        if (node->nodeKind != ASTNode::STRUCT &&
            node->nodeKind != ASTNode::INTERFACE_DEF)
            node->addSymbols(source->getScope());
    }

    compilation->frontEnd.n_lines += n_lines;
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
    std::unordered_map<std::string, milliseconds> times;
    std::unordered_map<std::string, milliseconds> peektimes;

    unsigned int nthreaded = std::thread::hardware_concurrency() - 1;

    if (nthreaded > 0 && !compilation->args.noparallel_arg) {
        std::vector<ImportParser *> parserContainer;
        std::unordered_map<std::string, std::future<milliseconds>> futureTimes;
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

                module->fill(p->nodes, p->structs, p->ifaceDefs);

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

                            module->fill(parser.nodes, parser.structs,
                                         parser.ifaceDefs);

                            module->n_lines = parser.n_lines;
                        } else {
                            module = compilation->frontEnd.modulesByID
                                         [parser.mod_decl->getIdentifier()];

                            parser.Dispose();
                        }

                        BJOU_DEBUG_ASSERT(module);

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

    compilation->frontEnd.AST.push_back(import);

    // std::deque<Import *> imports{import};

    // importModules(imports, frontEnd);
}

/*
AsyncImporter::AsyncImporter() : ifs(nullptr), ar(nullptr) {  }

AsyncImporter::AsyncImporter(std::string& _header, std::ifstream * _ifs,
boost::archive::text_iarchive * _ar) : header(_header), ifs(_ifs), ar(_ar) {  }

milliseconds AsyncImporter::operator () () {
    auto start = Clock::now();

    deserializeTypeTable(typeTable, *ar);
    deserializeAST(AST, *ar);

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}
 */

/*
void importModules(std::vector<ASTNode*>& AST) {
    std::vector<Import*> imports;
    for (ASTNode * node : AST)
        if (node->nodeKind == ASTNode::IMPORT)
            imports.push_back((Import*)node);

    unsigned int nthreaded = std::thread::hardware_concurrency() - 1;
    size_t nimports = imports.size();

    std::unordered_map<std::string, milliseconds> times;

    if (nimports > 1 && nthreaded > 0 &&
!compilation->args.noparallel_arg.getValue()) { unsigned int i = 0; while (i <
nimports) {
            // do threads
            std::vector<AsyncImporter> importers;
            importers.reserve(nthreaded);
            std::unordered_map<std::string, std::future<milliseconds> >
futureTimes;

            while (importers.size() < nthreaded || (i >= (nimports - (nimports %
nthreaded)) && i < nimports)) { Import * import = imports[i];

                BJOU_DEBUG_ASSERT(import->getFlag(Import::FROM_PATH));
                BJOU_DEBUG_ASSERT(import->module.size() > 2);

                // de-quote
                std::string fname = import->module.substr(1,
import->module.size() - 2);

                std::ifstream * ifs = new std::ifstream(fname);
                BJOU_DEBUG_ASSERT(ifs && "Bad file!");
                boost::archive::text_iarchive * ar = new
boost::archive::text_iarchive(*ifs);

                std::string header = read_header(*ar);
                auto& modulesImported = compilation->frontEnd.modulesImported;

                if (modulesImported.find(header) == modulesImported.end()) {
                    std::set<std::string> importedIDs;

                    deserializeImportedIDs(importedIDs, *ar);
                    joinImportedIDsWithCompilation(importedIDs);

                    modulesImported.insert(header);

                    importers.emplace_back(header, ifs, ar);

                    futureTimes[header] = std::async(std::launch::async,
std::ref(importers.back()));

                    for (auto& ft : futureTimes)
                        times[ft.first] = ft.second.get();

                    for (AsyncImporter& importer : importers) {
                        joinTypeTableWithCompilation(importer.typeTable);
                        AST.reserve(AST.size() + importer.AST.size());
                        AST.insert(AST.end(), importer.AST.begin(),
importer.AST.end());

                        delete importer.ar;
                        delete importer.ifs;
                    }
                } else {
                    delete ar;
                    delete ifs;
                }
                i += 1;
            }
        }
    } else {
        for (Import * import : imports) {
            BJOU_DEBUG_ASSERT(import->getFlag(Import::FROM_PATH));
            BJOU_DEBUG_ASSERT(import->module.size() > 2);

            // de-quote
            std::string fname = import->module.substr(1, import->module.size() -
2);

            auto start = Clock::now();

            importModuleFromFile(AST, fname.c_str());

            auto end = Clock::now();
            times[fname] = duration_cast<milliseconds>(end - start);
        }
    }

    if (compilation->args.time_arg.getValue())
        for (auto& t : times)
            prettyPrintTimeMin(t.second, "    " + t.first);
}

void importModuleFromFile(std::vector<ASTNode*>& AST, const char * fname) {
    std::vector<ASTNode*> importedNodes;
    std::string header;

    std::ifstream ifs(fname);
    {
        BJOU_DEBUG_ASSERT(ifs && "Bad file!");
        boost::archive::text_iarchive ar(ifs);

        header = read_header(ar);
        auto& modulesImported = compilation->frontEnd.modulesImported;

        if (modulesImported.find(header) == modulesImported.end()) {
            std::vector<ASTNode*> importedNodes;
            std::unordered_map<std::string, Type*> importedTypeTable;
            std::set<std::string> importedIDs;

            modulesImported.insert(header);

            deserializeImportedIDs(importedIDs, ar);
            deserializeTypeTable(importedTypeTable, ar);
            deserializeAST(importedNodes, ar);
            joinImportedIDsWithCompilation(importedIDs);
            joinTypeTableWithCompilation(importedTypeTable);
            AST.reserve(AST.size() + importedNodes.size());
            AST.insert(AST.end(), importedNodes.begin(), importedNodes.end());
        }
    }
}

void exportModule(std::vector<ASTNode*>& AST) {
    exportModuleToFile(AST, (compilation->outputpath +
compilation->outputbasefilename).c_str());
}

void exportModuleToFile(std::vector<ASTNode*>& AST, const char * fname) {
    std::ofstream ofs(fname);
    {
        boost::archive::text_oarchive ar(ofs);

        write_header(compilation->module_identifier, ar);
        serializeImportedIDs(compilation->frontEnd.modulesImported, ar);
        serializeTypeTable(compilation->frontEnd.typeTable, ar);
        serializeAST(AST, ar);
    }
}

void joinTypeTableWithCompilation(std::unordered_map<std::string, Type*>&
typeTable) {
    // @leak?

    compilation->frontEnd.typeTable.insert(typeTable.begin(), typeTable.end());
}

void joinImportedIDsWithCompilation(std::set<std::string>& modulesImported) {
    compilation->frontEnd.modulesImported.insert(modulesImported.begin(),
modulesImported.end());
}
 */
} // namespace bjou
