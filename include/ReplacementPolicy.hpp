//
//  ReplacementPolicy.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 8/11/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef ReplacementPolicy_hpp
#define ReplacementPolicy_hpp

#include <vector>

namespace bjou {

struct ASTNode;

void init_replacementPolicies();

struct replacementPolicy {
    replacementPolicy(bool _block = false) : block(_block) {}
    bool block = false;
    std::vector<int> allowed_nodeKinds;
    bool canReplace(ASTNode * node) const;
    //                             parent      old       new
    virtual ASTNode * operator()(ASTNode *, ASTNode *, ASTNode *) = 0;
    virtual ~replacementPolicy();
};

template <typename T> replacementPolicy * rpget();

#define RP_FUNCTOR_DECL(name)                                                  \
    struct replacementPolicy_##name : replacementPolicy {                      \
        ASTNode * operator()(ASTNode * parent, ASTNode * old_node,             \
                             ASTNode * new_node);                              \
    };                                                                         \
    template <> replacementPolicy * rpget<replacementPolicy_##name>();         \
    extern replacementPolicy_##name functor_replacementPolicy_##name

#define RP_FUNCTOR_DECL_BL(name)                                               \
    struct replacementPolicy_##name : replacementPolicy {                      \
        replacementPolicy_##name() : replacementPolicy(true) {}                \
        ASTNode * operator()(ASTNode * parent, ASTNode * old_node,             \
                             ASTNode * new_node);                              \
    };                                                                         \
    template <> replacementPolicy * rpget<replacementPolicy_##name>();         \
    extern replacementPolicy_##name functor_replacementPolicy_##name

RP_FUNCTOR_DECL(empty);
RP_FUNCTOR_DECL_BL(Global_Node);
RP_FUNCTOR_DECL_BL(MultiNode_Node);
RP_FUNCTOR_DECL(ExpressionL);
RP_FUNCTOR_DECL(ExpressionR);
RP_FUNCTOR_DECL(InitializerList_ObjDeclarator);
RP_FUNCTOR_DECL(InitializerList_Expression);
RP_FUNCTOR_DECL(SliceExpression_Src);
RP_FUNCTOR_DECL(SliceExpression_Start);
RP_FUNCTOR_DECL(SliceExpression_Length);
RP_FUNCTOR_DECL(DynamicArrayExpression_TypeDeclarator);
RP_FUNCTOR_DECL(LenExpression_Expr);
RP_FUNCTOR_DECL(TupleLiteral_subExpression);
RP_FUNCTOR_DECL_BL(ExprBlock_Statement);
RP_FUNCTOR_DECL(NamedArg_Expression);
RP_FUNCTOR_DECL(Declarator_Identifier);
RP_FUNCTOR_DECL(Declarator_TemplateInst);
RP_FUNCTOR_DECL(ArrayDeclarator_ArrayOf);
RP_FUNCTOR_DECL(ArrayDeclarator_Expression);
RP_FUNCTOR_DECL(SliceDeclarator_SliceOf);
RP_FUNCTOR_DECL(DynamicArrayDeclarator_ArrayOf);
RP_FUNCTOR_DECL(PointerDeclarator_PointerOf);
RP_FUNCTOR_DECL(RefDeclarator_RefOf);
RP_FUNCTOR_DECL(MaybeDeclarator_MaybeOf);
RP_FUNCTOR_DECL(SumDeclarator_subDeclarator);
RP_FUNCTOR_DECL(TupleDeclarator_subDeclarator);
RP_FUNCTOR_DECL(ProcedureDeclarator_ParamDeclarators);
RP_FUNCTOR_DECL(ProcedureDeclarator_RetDeclarator);
RP_FUNCTOR_DECL(Constant_TypeDeclarator);
RP_FUNCTOR_DECL(Constant_Initialization);
RP_FUNCTOR_DECL(VariableDeclaration_TypeDeclarator);
RP_FUNCTOR_DECL(VariableDeclaration_Initialization);
RP_FUNCTOR_DECL(Alias_Declarator);
RP_FUNCTOR_DECL(Struct_Extends);
RP_FUNCTOR_DECL_BL(Struct_MemberVarDecl);
RP_FUNCTOR_DECL_BL(Struct_ConstantDecl);
RP_FUNCTOR_DECL_BL(Struct_MemberProc);
RP_FUNCTOR_DECL_BL(Struct_MemberTemplateProc);
RP_FUNCTOR_DECL_BL(ArgList_Expressions);
RP_FUNCTOR_DECL(Procedure_ParamVarDeclaration);
RP_FUNCTOR_DECL(Procedure_RetDeclarator);
RP_FUNCTOR_DECL(Procedure_ProcDeclarator);
RP_FUNCTOR_DECL_BL(Procedure_Statement);
RP_FUNCTOR_DECL(Print_Args);
RP_FUNCTOR_DECL(Return_Expression);
RP_FUNCTOR_DECL(ExprBlockYield_Expression);
RP_FUNCTOR_DECL(If_Conditional);
RP_FUNCTOR_DECL_BL(If_Statement);
RP_FUNCTOR_DECL(If_Else);
RP_FUNCTOR_DECL_BL(Else_Statement);
RP_FUNCTOR_DECL_BL(For_Initialization);
RP_FUNCTOR_DECL(For_Conditional);
RP_FUNCTOR_DECL_BL(For_Afterthought);
RP_FUNCTOR_DECL_BL(For_Statement);
RP_FUNCTOR_DECL(Foreach_Expression);
RP_FUNCTOR_DECL_BL(Foreach_Statement);
RP_FUNCTOR_DECL(While_Conditional);
RP_FUNCTOR_DECL_BL(While_Statement);
RP_FUNCTOR_DECL(DoWhile_Conditional);
RP_FUNCTOR_DECL_BL(DoWhile_Statement);
RP_FUNCTOR_DECL(Match_Expression);
RP_FUNCTOR_DECL_BL(Match_With);
RP_FUNCTOR_DECL(With_Expression);
RP_FUNCTOR_DECL_BL(With_Statement);
RP_FUNCTOR_DECL(TemplateDefineList_Element);
RP_FUNCTOR_DECL(TemplateDefineTypeDescriptor_Bound);
RP_FUNCTOR_DECL(TemplateDefineExpression_VarDecl);
RP_FUNCTOR_DECL(TemplateInstantiation_Element);
RP_FUNCTOR_DECL(TemplateAlias_Template);
RP_FUNCTOR_DECL(TemplateAlias_TemplateDef);
RP_FUNCTOR_DECL(TemplateStruct_Template);
RP_FUNCTOR_DECL(TemplateStruct_TemplateDef);
RP_FUNCTOR_DECL(TemplateProc_Template);
RP_FUNCTOR_DECL(TemplateProc_TemplateDef);
RP_FUNCTOR_DECL_BL(MacroUse_Arg);
} // namespace bjou

#endif /* ReplacementPolicy_hpp */
