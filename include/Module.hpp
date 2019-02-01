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
/*
struct AsyncImporter {
    std::string header;
    std::ifstream * ifs;
    boost::archive::text_iarchive * ar;

    std::vector<ASTNode*> AST;
    std::unordered_map<std::string, Type*> typeTable;

    AsyncImporter();
    AsyncImporter(std::string& _header, std::ifstream * _ifs,
boost::archive::text_iarchive * _ar);

    milliseconds operator () ();
};
 */

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
void activateModule(Import * import);
// void exportModule(std::vector<ASTNode*>& AST);
// void exportModuleToFile(std::vector<ASTNode*>& AST, const char * fname);
// void joinTypeTableWithCompilation(std::unordered_map<std::string, Type*>&
// typeTable); void joinImportedIDsWithCompilation(std::set<std::string>&
// modulesImported);
} // namespace bjou

#endif /* Module_hpp */
