//
//  Template.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Template_hpp
#define Template_hpp

#include "ASTNode.hpp"

template <typename A, typename B>
void p(A a, B b) {}

template <typename A, typename B>
void p(B b, A a) {}

namespace bjou {
    Struct * makeTemplateStruct(ASTNode * _ttype, ASTNode * _inst);
    Procedure * makeTemplateProc(ASTNode * _tproc, ASTNode * _passed_args, ASTNode * _inst, Context * context, bool fail = true);
    bool checkTemplateProcInstantiation(ASTNode * _tproc, ASTNode * _passed_args, ASTNode * _inst, Context * context, bool fail = true, TemplateInstantiation * new_inst = nullptr);
}

#endif /* Template_hpp */
