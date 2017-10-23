//
//  ReplacementPolicy.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 8/11/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "ReplacementPolicy.hpp"
#include "ASTNode.hpp"
#include "FrontEnd.hpp"

namespace bjou {
replacementPolicy::~replacementPolicy() {}

#define RP_FUNCTOR_IMPL(name, ...)                                             \
    ASTNode * replacementPolicy_##name::operator()(                            \
        ASTNode * parent, ASTNode * old_node, ASTNode * new_node){__VA_ARGS__} \
                                                                               \
    replacementPolicy_##name functor_replacementPolicy_##name;                 \
                                                                               \
    template <> replacementPolicy * rpget<replacementPolicy_##name>() {        \
        return &functor_replacementPolicy_##name;                              \
    }

RP_FUNCTOR_IMPL(empty, return nullptr;);
RP_FUNCTOR_IMPL(
    Global_Node,
    auto found_node = std::find(compilation->frontEnd.AST.begin(),
                                compilation->frontEnd.AST.end(), old_node);
    if (found_node == compilation->frontEnd.AST.end()) internalError(
        "node to replace not found in replacementPolicy_Global_Node()");

    *found_node = new_node; new_node->parent = nullptr;
    new_node->replace = rpget<replacementPolicy_Global_Node>();

    return new_node;);
RP_FUNCTOR_IMPL(ExpressionL,
                BJOU_DEBUG_ASSERT(((Expression *)parent)->getLeft() ==
                                  old_node);
                ((Expression *)parent)->setLeft(new_node); return new_node;);
RP_FUNCTOR_IMPL(ExpressionR,
                BJOU_DEBUG_ASSERT(((Expression *)parent)->getRight() ==
                                  old_node);
                ((Expression *)parent)->setRight(new_node); return new_node;);
RP_FUNCTOR_IMPL(
    InitializerList_ObjDeclarator,
    BJOU_DEBUG_ASSERT(((InitializerList *)parent)->getObjDeclarator() ==
                      old_node);
    ((InitializerList *)parent)->setObjDeclarator(new_node); return new_node;);
RP_FUNCTOR_IMPL(
    InitializerList_Expression,
    InitializerList * parent_init_list = (InitializerList *)parent;
    auto found_node = std::find(parent_init_list->getExpressions().begin(),
                                parent_init_list->getExpressions().end(),
                                old_node);
    if (found_node == parent_init_list->getExpressions().end())
        internalError("node to replace not found in "
                      "replacementPolicy_InitializerList_Expression()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_InitializerList_Expression>();

    return new_node;);
RP_FUNCTOR_IMPL(
    TupleLiteral_subExpression,
    TupleLiteral * parent_tuple_lit = (TupleLiteral *)parent;
    auto found_node = std::find(parent_tuple_lit->getSubExpressions().begin(),
                                parent_tuple_lit->getSubExpressions().end(),
                                old_node);
    if (found_node == parent_tuple_lit->getSubExpressions().end())
        internalError("node to replace not found in "
                      "replacementPolicy_TupleLiteral_subExpression()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_TupleLiteral_subExpression>();

    return new_node;);
RP_FUNCTOR_IMPL(Declarator_Identifier,
                BJOU_DEBUG_ASSERT(((Declarator *)parent)->getIdentifier() ==
                                  old_node);
                ((Declarator *)parent)->setIdentifier(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(Declarator_TemplateInst,
                BJOU_DEBUG_ASSERT(((Declarator *)parent)->getTemplateInst() ==
                                  old_node);
                ((Declarator *)parent)->setTemplateInst(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(ArrayDeclarator_ArrayOf,
                BJOU_DEBUG_ASSERT(((ArrayDeclarator *)parent)->getArrayOf() ==
                                  old_node);
                ((ArrayDeclarator *)parent)->setArrayOf(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(
    ArrayDeclarator_Expression,
    BJOU_DEBUG_ASSERT(((ArrayDeclarator *)parent)->getExpression() == old_node);
    ((ArrayDeclarator *)parent)->setExpression(new_node); return new_node;);
RP_FUNCTOR_IMPL(
    DynamicArrayDeclarator_ArrayOf,
    BJOU_DEBUG_ASSERT(((DynamicArrayDeclarator *)parent)->getArrayOf() ==
                      old_node);
    ((DynamicArrayDeclarator *)parent)->setArrayOf(new_node); return new_node;);
RP_FUNCTOR_IMPL(PointerDeclarator_PointerOf,
                BJOU_DEBUG_ASSERT(
                    ((PointerDeclarator *)parent)->getPointerOf() == old_node);
                ((PointerDeclarator *)parent)->setPointerOf(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(MaybeDeclarator_MaybeOf,
                BJOU_DEBUG_ASSERT(((MaybeDeclarator *)parent)->getMaybeOf() ==
                                  old_node);
                ((MaybeDeclarator *)parent)->setMaybeOf(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(
    TupleDeclarator_subDeclarator,
    TupleDeclarator * parent_tuple_decl = (TupleDeclarator *)parent;
    auto found_node = std::find(parent_tuple_decl->getSubDeclarators().begin(),
                                parent_tuple_decl->getSubDeclarators().end(),
                                old_node);
    if (found_node == parent_tuple_decl->getSubDeclarators().end())
        internalError("node to replace not found in "
                      "replacementPolicy_TupleDeclarator_subDeclarator()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace =
        rpget<replacementPolicy_TupleDeclarator_subDeclarator>();

    return new_node;);
RP_FUNCTOR_IMPL(
    ProcedureDeclarator_ParamDeclarators,
    ProcedureDeclarator * parent_proc_decl = (ProcedureDeclarator *)parent;
    auto found_node = std::find(parent_proc_decl->getParamDeclarators().begin(),
                                parent_proc_decl->getParamDeclarators().end(),
                                old_node);
    if (found_node == parent_proc_decl->getParamDeclarators().end())
        internalError("node to replace not found in "
                      "replacementPolicy_ProcedureDeclarator_ParamDeclarators("
                      ")");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace =
        rpget<replacementPolicy_ProcedureDeclarator_ParamDeclarators>();

    return new_node;);
RP_FUNCTOR_IMPL(ProcedureDeclarator_RetDeclarator,
                BJOU_DEBUG_ASSERT(((ProcedureDeclarator *)parent)
                                      ->getRetDeclarator() == old_node);
                ((ProcedureDeclarator *)parent)->setRetDeclarator(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(Constant_TypeDeclarator,
                BJOU_DEBUG_ASSERT(((Constant *)parent)->getTypeDeclarator() ==
                                  old_node);
                ((Constant *)parent)->setTypeDeclarator(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(Constant_Initialization,
                BJOU_DEBUG_ASSERT(((Constant *)parent)->getInitialization() ==
                                  old_node);
                ((Constant *)parent)->setInitialization(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(VariableDeclaration_TypeDeclarator,
                BJOU_DEBUG_ASSERT(((VariableDeclaration *)parent)
                                      ->getTypeDeclarator() == old_node);
                ((VariableDeclaration *)parent)->setTypeDeclarator(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(VariableDeclaration_Initialization,
                BJOU_DEBUG_ASSERT(((VariableDeclaration *)parent)
                                      ->getInitialization() == old_node);
                ((VariableDeclaration *)parent)->setInitialization(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(Alias_Declarator,
                BJOU_DEBUG_ASSERT(((Alias *)parent)->getDeclarator() ==
                                  old_node);
                ((Alias *)parent)->setDeclarator(new_node); return new_node;);
RP_FUNCTOR_IMPL(Struct_Extends,
                BJOU_DEBUG_ASSERT(((Struct *)parent)->getExtends() == old_node);
                ((Struct *)parent)->setExtends(new_node); return new_node;);
RP_FUNCTOR_IMPL(
    Struct_MemberVarDecl, Struct * parent_struct = (Struct *)parent;
    auto found_node = std::find(parent_struct->getMemberVarDecls().begin(),
                                parent_struct->getMemberVarDecls().end(),
                                old_node);
    if (found_node == parent_struct->getMemberVarDecls().end())
        internalError("node to replace not found in "
                      "replacementPolicy_Struct_MemberVarDecl()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_Struct_MemberVarDecl>();

    return new_node;);
RP_FUNCTOR_IMPL(
    Struct_ConstantDecl, Struct * parent_struct = (Struct *)parent;
    auto found_node = std::find(parent_struct->getConstantDecls().begin(),
                                parent_struct->getConstantDecls().end(),
                                old_node);
    if (found_node == parent_struct->getConstantDecls().end()) internalError(
        "node to replace not found in replacementPolicy_Struct_ConstantDecl()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_Struct_ConstantDecl>();

    return new_node;);
RP_FUNCTOR_IMPL(
    Struct_MemberProc, Struct * parent_struct = (Struct *)parent;
    auto found_node = std::find(parent_struct->getMemberProcs().begin(),
                                parent_struct->getMemberProcs().end(),
                                old_node);
    if (found_node == parent_struct->getMemberProcs().end()) internalError(
        "node to replace not found in replacementPolicy_Struct_MemberProc()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_Struct_MemberProc>();

    return new_node;);
RP_FUNCTOR_IMPL(
    Struct_MemberTemplateProc, Struct * parent_struct = (Struct *)parent;
    auto found_node = std::find(parent_struct->getMemberTemplateProcs().begin(),
                                parent_struct->getMemberTemplateProcs().end(),
                                old_node);
    if (found_node == parent_struct->getMemberTemplateProcs().end())
        internalError("node to replace not found in "
                      "replacementPolicy_Struct_MemberTemplateProc()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_Struct_MemberTemplateProc>();

    return new_node;);
RP_FUNCTOR_IMPL(
    Struct_InterfaceImpl, Struct * parent_struct = (Struct *)parent;
    auto found_node = std::find(parent_struct->getInterfaceImpls().begin(),
                                parent_struct->getInterfaceImpls().end(),
                                old_node);
    if (found_node == parent_struct->getInterfaceImpls().end())
        internalError("node to replace not found in "
                      "replacementPolicy_Struct_InterfaceImpl()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_Struct_InterfaceImpl>();

    return new_node;);
RP_FUNCTOR_IMPL(
    InterfaceDef_Proc, InterfaceDef * parent_def = (InterfaceDef *)parent;
    ASTNode ** found_node = nullptr; for (auto & it
                                          : parent_def->getProcs()) {
        auto node = std::find(it.second.begin(), it.second.end(), old_node);
        if (node != it.second.end()) {
            found_node = node.base();
            break;
        }
    } if (!found_node) internalError("node to replace not found in "
                                     "replacementPolicy_InterfaceDef_Proc()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_InterfaceDef_Proc>();

    return new_node;);
RP_FUNCTOR_IMPL(InterfaceImplementation_Identifier,
                BJOU_DEBUG_ASSERT(((InterfaceImplementation *)parent)
                                      ->getIdentifier() == old_node);
                ((InterfaceImplementation *)parent)->setIdentifier(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(
    InterfaceImplementation_Proc,
    InterfaceImplementation * parent_impl = (InterfaceImplementation *)parent;
    ASTNode ** found_node = nullptr; for (auto & it
                                          : parent_impl->getProcs()) {
        auto node = std::find(it.second.begin(), it.second.end(), old_node);
        if (node != it.second.end()) {
            found_node = node.base();
            break;
        }
    } if (!found_node)
        internalError("node to replace not found in "
                      "replacementPolicy_InterfaceImplementation_Proc()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_InterfaceImplementation_Proc>();

    return new_node;);
RP_FUNCTOR_IMPL(
    ArgList_Expressions, ArgList * parent_args = (ArgList *)parent;
    auto found_node = std::find(parent_args->getExpressions().begin(),
                                parent_args->getExpressions().end(), old_node);
    if (found_node == parent_args->getExpressions().end()) internalError(
        "node to replace not found in replacementPolicy_ArgList_Expressions()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_ArgList_Expressions>();

    return new_node;);
RP_FUNCTOR_IMPL(
    Procedure_ParamVarDeclaration,
    Procedure * parent_proc = (Procedure *)parent;
    auto found_node = std::find(parent_proc->getParamVarDeclarations().begin(),
                                parent_proc->getParamVarDeclarations().end(),
                                old_node);
    if (found_node == parent_proc->getParamVarDeclarations().end())
        internalError("node to replace not found in "
                      "replacementPolicy_Procedure_ParamVarDeclaration()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace =
        rpget<replacementPolicy_Procedure_ParamVarDeclaration>();

    return new_node;);
RP_FUNCTOR_IMPL(Procedure_RetDeclarator,
                BJOU_DEBUG_ASSERT(((Procedure *)parent)->getRetDeclarator() ==
                                  old_node);
                ((Procedure *)parent)->setRetDeclarator(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(Procedure_ProcDeclarator,
                BJOU_DEBUG_ASSERT(((Procedure *)parent)->getProcDeclarator() ==
                                  old_node);
                ((Procedure *)parent)->setProcDeclarator(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(
    Procedure_Statement, Procedure * parent_proc = (Procedure *)parent;
    auto found_node = std::find(parent_proc->getStatements().begin(),
                                parent_proc->getStatements().end(), old_node);
    if (found_node == parent_proc->getStatements().end()) internalError(
        "node to replace not found in replacementPolicy_Procedure_Statement()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_Procedure_Statement>();

    return new_node;);
RP_FUNCTOR_IMPL(
    Namespace_Node, Namespace * parent_nspace = (Namespace *)parent;
    auto found_node = std::find(parent_nspace->getNodes().begin(),
                                parent_nspace->getNodes().end(), old_node);
    if (found_node == parent_nspace->getNodes().end()) internalError(
        "node to replace not found in replacementPolicy_Namespace_Node()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_Namespace_Node>();

    return new_node;);
RP_FUNCTOR_IMPL(Print_Args,
                BJOU_DEBUG_ASSERT(((Print *)parent)->getArgs() == old_node);
                ((Print *)parent)->setArgs(new_node); return new_node;);
RP_FUNCTOR_IMPL(Return_Expression,
                BJOU_DEBUG_ASSERT(((Return *)parent)->getExpression() ==
                                  old_node);
                ((Return *)parent)->setExpression(new_node); return new_node;);
RP_FUNCTOR_IMPL(If_Conditional,
                BJOU_DEBUG_ASSERT(((If *)parent)->getConditional() == old_node);
                ((If *)parent)->setConditional(new_node); return new_node;);
RP_FUNCTOR_IMPL(
    If_Statement, If * parent_if = (If *)parent;
    auto found_node = std::find(parent_if->getStatements().begin(),
                                parent_if->getStatements().end(), old_node);
    if (found_node == parent_if->getStatements().end()) internalError(
        "node to replace not found in replacementPolicy_If_Statement()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_If_Statement>();

    return new_node;);
RP_FUNCTOR_IMPL(If_Else,
                BJOU_DEBUG_ASSERT(((If *)parent)->getElse() == old_node);
                ((If *)parent)->setElse(new_node); return new_node;);
RP_FUNCTOR_IMPL(
    Else_Statement, Else * parent_else = (Else *)parent;
    auto found_node = std::find(parent_else->getStatements().begin(),
                                parent_else->getStatements().end(), old_node);
    if (found_node == parent_else->getStatements().end()) internalError(
        "node to replace not found in replacementPolicy_Else_Statement()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_Else_Statement>();

    return new_node;);
RP_FUNCTOR_IMPL(
    For_Initialization, For * parent_for = (For *)parent;
    auto found_node = std::find(parent_for->getInitializations().begin(),
                                parent_for->getInitializations().end(),
                                old_node);
    if (found_node == parent_for->getInitializations().end()) internalError(
        "node to replace not found in replacementPolicy_For_Initialization()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_For_Initialization>();

    return new_node;);
RP_FUNCTOR_IMPL(For_Conditional,
                BJOU_DEBUG_ASSERT(((For *)parent)->getConditional() ==
                                  old_node);
                ((For *)parent)->setConditional(new_node); return new_node;);
RP_FUNCTOR_IMPL(
    For_Afterthought, For * parent_for = (For *)parent;
    auto found_node = std::find(parent_for->getAfterthoughts().begin(),
                                parent_for->getAfterthoughts().end(), old_node);
    if (found_node == parent_for->getAfterthoughts().end()) internalError(
        "node to replace not found in replacementPolicy_For_Afterthought()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_For_Afterthought>();

    return new_node;);
RP_FUNCTOR_IMPL(
    For_Statement, For * parent_for = (For *)parent;
    auto found_node = std::find(parent_for->getStatements().begin(),
                                parent_for->getStatements().end(), old_node);
    if (found_node == parent_for->getStatements().end()) internalError(
        "node to replace not found in replacementPolicy_For_Statement()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_For_Statement>();

    return new_node;);
RP_FUNCTOR_IMPL(While_Conditional,
                BJOU_DEBUG_ASSERT(((While *)parent)->getConditional() ==
                                  old_node);
                ((While *)parent)->setConditional(new_node); return new_node;);
RP_FUNCTOR_IMPL(
    While_Statement, While * parent_while = (While *)parent;
    auto found_node = std::find(parent_while->getStatements().begin(),
                                parent_while->getStatements().end(), old_node);
    if (found_node == parent_while->getStatements().end()) internalError(
        "node to replace not found in replacementPolicy_While_Statement()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_While_Statement>();

    return new_node;);
RP_FUNCTOR_IMPL(DoWhile_Conditional,
                BJOU_DEBUG_ASSERT(((DoWhile *)parent)->getConditional() ==
                                  old_node);
                ((DoWhile *)parent)->setConditional(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(
    DoWhile_Statement, DoWhile * parent_do_while = (DoWhile *)parent;
    auto found_node = std::find(parent_do_while->getStatements().begin(),
                                parent_do_while->getStatements().end(),
                                old_node);
    if (found_node == parent_do_while->getStatements().end()) internalError(
        "node to replace not found in replacementPolicy_DoWhile_Statement()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_DoWhile_Statement>();

    return new_node;);
RP_FUNCTOR_IMPL(Match_Expression,
                BJOU_DEBUG_ASSERT(((Match *)parent)->getExpression() ==
                                  old_node);
                ((Match *)parent)->setExpression(new_node); return new_node;);
RP_FUNCTOR_IMPL(
    Match_With, Match * parent_match = (Match *)parent;
    auto found_node = std::find(parent_match->getWiths().begin(),
                                parent_match->getWiths().end(), old_node);
    if (found_node == parent_match->getWiths().end()) internalError(
        "node to replace not found in replacementPolicy_Match_With()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_Match_With>();

    return new_node;);
RP_FUNCTOR_IMPL(With_Expression,
                BJOU_DEBUG_ASSERT(((With *)parent)->getExpression() ==
                                  old_node);
                ((With *)parent)->setExpression(new_node); return new_node;);
RP_FUNCTOR_IMPL(
    With_Statement, With * parent_with = (With *)parent;
    auto found_node = std::find(parent_with->getStatements().begin(),
                                parent_with->getStatements().end(), old_node);
    if (found_node == parent_with->getStatements().end()) internalError(
        "node to replace not found in replacementPolicy_With_Statement()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_With_Statement>();

    return new_node;);
RP_FUNCTOR_IMPL(
    TemplateDefineList_Element,
    TemplateDefineList * parent_def_list = (TemplateDefineList *)parent;
    auto found_node = std::find(parent_def_list->getElements().begin(),
                                parent_def_list->getElements().end(), old_node);
    if (found_node == parent_def_list->getElements().end())
        internalError("node to replace not found in "
                      "replacementPolicy_TemplateDefineList_Element()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace = rpget<replacementPolicy_TemplateDefineList_Element>();

    return new_node;);
RP_FUNCTOR_IMPL(
    TemplateDefineTypeDescriptor_Bound,
    TemplateDefineTypeDescriptor * parent_desc =
        (TemplateDefineTypeDescriptor *)parent;
    auto found_node = std::find(parent_desc->getBounds().begin(),
                                parent_desc->getBounds().end(), old_node);
    if (found_node == parent_desc->getBounds().end())
        internalError("node to replace not found in "
                      "replacementPolicy_TemplateDefineTypeDescriptor_Bound()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace =
        rpget<replacementPolicy_TemplateDefineTypeDescriptor_Bound>();

    return new_node;);
RP_FUNCTOR_IMPL(TemplateDefineExpression_VarDecl,
                BJOU_DEBUG_ASSERT(((TemplateDefineExpression *)parent)
                                      ->getVarDecl() == old_node);
                ((TemplateDefineExpression *)parent)->setVarDecl(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(
    TemplateInstantiation_Element,
    TemplateInstantiation * parent_inst = (TemplateInstantiation *)parent;
    auto found_node = std::find(parent_inst->getElements().begin(),
                                parent_inst->getElements().end(), old_node);
    if (found_node == parent_inst->getElements().end())
        internalError("node to replace not found in "
                      "replacementPolicy_TemplateInstantiation_Element()");

    *found_node = new_node; new_node->parent = parent;
    new_node->replace =
        rpget<replacementPolicy_TemplateInstantiation_Element>();

    return new_node;);
RP_FUNCTOR_IMPL(TemplateAlias_Template,
                BJOU_DEBUG_ASSERT(((TemplateAlias *)parent)->getTemplate() ==
                                  old_node);
                ((TemplateAlias *)parent)->setTemplate(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(TemplateAlias_TemplateDef,
                BJOU_DEBUG_ASSERT(((TemplateAlias *)parent)->getTemplateDef() ==
                                  old_node);
                ((TemplateAlias *)parent)->setTemplateDef(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(TemplateStruct_Template,
                BJOU_DEBUG_ASSERT(((TemplateStruct *)parent)->getTemplate() ==
                                  old_node);
                ((TemplateStruct *)parent)->setTemplate(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(
    TemplateStruct_TemplateDef,
    BJOU_DEBUG_ASSERT(((TemplateStruct *)parent)->getTemplateDef() == old_node);
    ((TemplateStruct *)parent)->setTemplateDef(new_node); return new_node;);
RP_FUNCTOR_IMPL(TemplateProc_Template,
                BJOU_DEBUG_ASSERT(((TemplateProc *)parent)->getTemplate() ==
                                  old_node);
                ((TemplateProc *)parent)->setTemplate(new_node);
                return new_node;);
RP_FUNCTOR_IMPL(TemplateProc_TemplateDef,
                BJOU_DEBUG_ASSERT(((TemplateProc *)parent)->getTemplateDef() ==
                                  old_node);
                ((TemplateProc *)parent)->setTemplateDef(new_node);
                return new_node;);
} // namespace bjou
