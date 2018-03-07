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
#include "Serialization.hpp"

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

void importModulesFromAST(FrontEnd & frontEnd);
void importModuleFromFile(FrontEnd & frontEnd, const char * _fname);
// void exportModule(std::vector<ASTNode*>& AST);
// void exportModuleToFile(std::vector<ASTNode*>& AST, const char * fname);
// void joinTypeTableWithCompilation(std::unordered_map<std::string, Type*>&
// typeTable); void joinImportedIDsWithCompilation(std::set<std::string>&
// modulesImported);
} // namespace bjou

#endif /* Module_hpp */
