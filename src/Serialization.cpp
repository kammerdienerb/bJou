//
//  Serialization.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 8/11/17.
//  Copyright © 2017 me. All rights reserved.
//

#if 0

#include "Serialization.hpp"
#include "FrontEnd.hpp"

#include <boost/serialization/export.hpp>

BOOST_CLASS_EXPORT(bjou::Loc)
BOOST_CLASS_EXPORT(bjou::Context)
BOOST_CLASS_EXPORT(bjou::ASTNode)
BOOST_CLASS_EXPORT(bjou::Expression)
BOOST_CLASS_EXPORT(bjou::BinaryExpression)
BOOST_CLASS_EXPORT(bjou::AddExpression)
BOOST_CLASS_EXPORT(bjou::SubExpression)
BOOST_CLASS_EXPORT(bjou::MultExpression)
BOOST_CLASS_EXPORT(bjou::DivExpression)
BOOST_CLASS_EXPORT(bjou::ModExpression)
BOOST_CLASS_EXPORT(bjou::AssignmentExpression)
BOOST_CLASS_EXPORT(bjou::AddAssignExpression)
BOOST_CLASS_EXPORT(bjou::SubAssignExpression)
BOOST_CLASS_EXPORT(bjou::MultAssignExpression)
BOOST_CLASS_EXPORT(bjou::DivAssignExpression)
BOOST_CLASS_EXPORT(bjou::ModAssignExpression)
BOOST_CLASS_EXPORT(bjou::MaybeAssignExpression)
BOOST_CLASS_EXPORT(bjou::LssExpression)
BOOST_CLASS_EXPORT(bjou::LeqExpression)
BOOST_CLASS_EXPORT(bjou::GtrExpression)
BOOST_CLASS_EXPORT(bjou::GeqExpression)
BOOST_CLASS_EXPORT(bjou::EquExpression)
BOOST_CLASS_EXPORT(bjou::NeqExpression)
BOOST_CLASS_EXPORT(bjou::LogAndExpression)
BOOST_CLASS_EXPORT(bjou::LogOrExpression)
BOOST_CLASS_EXPORT(bjou::CallExpression)
BOOST_CLASS_EXPORT(bjou::SubscriptExpression)
BOOST_CLASS_EXPORT(bjou::AccessExpression)
BOOST_CLASS_EXPORT(bjou::InjectExpression)
BOOST_CLASS_EXPORT(bjou::UnaryPreExpression)
BOOST_CLASS_EXPORT(bjou::NewExpression)
BOOST_CLASS_EXPORT(bjou::DeleteExpression)
BOOST_CLASS_EXPORT(bjou::SizeofExpression)
BOOST_CLASS_EXPORT(bjou::NotExpression)
BOOST_CLASS_EXPORT(bjou::DerefExpression)
BOOST_CLASS_EXPORT(bjou::AddressExpression)
BOOST_CLASS_EXPORT(bjou::UnaryPostExpression)
BOOST_CLASS_EXPORT(bjou::AsExpression)
BOOST_CLASS_EXPORT(bjou::Identifier)
BOOST_CLASS_EXPORT(bjou::InitializerList)
BOOST_CLASS_EXPORT(bjou::BooleanLiteral)
BOOST_CLASS_EXPORT(bjou::IntegerLiteral)
BOOST_CLASS_EXPORT(bjou::FloatLiteral)
BOOST_CLASS_EXPORT(bjou::StringLiteral)
BOOST_CLASS_EXPORT(bjou::CharLiteral)
BOOST_CLASS_EXPORT(bjou::ProcLiteral)
BOOST_CLASS_EXPORT(bjou::ExternLiteral)
BOOST_CLASS_EXPORT(bjou::SomeLiteral)
BOOST_CLASS_EXPORT(bjou::NothingLiteral)
BOOST_CLASS_EXPORT(bjou::TupleLiteral)
BOOST_CLASS_EXPORT(bjou::Declarator)
BOOST_CLASS_EXPORT(bjou::ArrayDeclarator)
BOOST_CLASS_EXPORT(bjou::DynamicArrayDeclarator)
BOOST_CLASS_EXPORT(bjou::PointerDeclarator)
BOOST_CLASS_EXPORT(bjou::MaybeDeclarator)
BOOST_CLASS_EXPORT(bjou::TupleDeclarator)
BOOST_CLASS_EXPORT(bjou::ProcedureDeclarator)
BOOST_CLASS_EXPORT(bjou::Constant)
BOOST_CLASS_EXPORT(bjou::VariableDeclaration)
BOOST_CLASS_EXPORT(bjou::Alias)
BOOST_CLASS_EXPORT(bjou::Struct)
BOOST_CLASS_EXPORT(bjou::InterfaceDef)
BOOST_CLASS_EXPORT(bjou::InterfaceImplementation)
BOOST_CLASS_EXPORT(bjou::Enum)
BOOST_CLASS_EXPORT(bjou::ArgList)
BOOST_CLASS_EXPORT(bjou::This)
BOOST_CLASS_EXPORT(bjou::Procedure)
BOOST_CLASS_EXPORT(bjou::Namespace)
BOOST_CLASS_EXPORT(bjou::Import)
BOOST_CLASS_EXPORT(bjou::Print)
BOOST_CLASS_EXPORT(bjou::Return)
BOOST_CLASS_EXPORT(bjou::Break)
BOOST_CLASS_EXPORT(bjou::Continue)
BOOST_CLASS_EXPORT(bjou::If)
BOOST_CLASS_EXPORT(bjou::Else)
BOOST_CLASS_EXPORT(bjou::For)
BOOST_CLASS_EXPORT(bjou::While)
BOOST_CLASS_EXPORT(bjou::DoWhile)
BOOST_CLASS_EXPORT(bjou::Match)
BOOST_CLASS_EXPORT(bjou::With)
BOOST_CLASS_EXPORT(bjou::TemplateDefineList)
BOOST_CLASS_EXPORT(bjou::TemplateDefineTypeDescriptor)
BOOST_CLASS_EXPORT(bjou::TemplateDefineVariadicTypeArgs)
BOOST_CLASS_EXPORT(bjou::TemplateDefineExpression)
BOOST_CLASS_EXPORT(bjou::TemplateInstantiation)
BOOST_CLASS_EXPORT(bjou::TemplateAlias)
BOOST_CLASS_EXPORT(bjou::TemplateStruct)
BOOST_CLASS_EXPORT(bjou::TemplateProc)
BOOST_CLASS_EXPORT(bjou::SLComment)

BOOST_CLASS_EXPORT(bjou::Type)
BOOST_CLASS_EXPORT(bjou::InvalidType)
BOOST_CLASS_EXPORT(bjou::StructType)
BOOST_CLASS_EXPORT(bjou::EnumType)
BOOST_CLASS_EXPORT(bjou::AliasType)
BOOST_CLASS_EXPORT(bjou::ArrayType)
BOOST_CLASS_EXPORT(bjou::DynamicArrayType)
BOOST_CLASS_EXPORT(bjou::PointerType)
BOOST_CLASS_EXPORT(bjou::MaybeType)
BOOST_CLASS_EXPORT(bjou::TupleType)
BOOST_CLASS_EXPORT(bjou::ProcedureType)
BOOST_CLASS_EXPORT(bjou::TemplateStructType)
BOOST_CLASS_EXPORT(bjou::TemplateAliasType)

#define RP_FUNCTOR_SER_REG(name)                                               \
    BOOST_CLASS_EXPORT(bjou::replacementPolicy_##name);

BOOST_CLASS_EXPORT(bjou::replacementPolicy)
RP_FUNCTOR_SER_REG(empty);
RP_FUNCTOR_SER_REG(Global_Node);
RP_FUNCTOR_SER_REG(ExpressionL);
RP_FUNCTOR_SER_REG(ExpressionR);
RP_FUNCTOR_SER_REG(InitializerList_ObjDeclarator);
RP_FUNCTOR_SER_REG(InitializerList_Expression);
RP_FUNCTOR_SER_REG(TupleLiteral_subExpression);
RP_FUNCTOR_SER_REG(Declarator_Identifier);
RP_FUNCTOR_SER_REG(Declarator_TemplateInst);
RP_FUNCTOR_SER_REG(ArrayDeclarator_ArrayOf);
RP_FUNCTOR_SER_REG(ArrayDeclarator_Expression);
RP_FUNCTOR_SER_REG(DynamicArrayDeclarator_ArrayOf);
RP_FUNCTOR_SER_REG(PointerDeclarator_PointerOf);
RP_FUNCTOR_SER_REG(MaybeDeclarator_MaybeOf);
RP_FUNCTOR_SER_REG(TupleDeclarator_subDeclarator);
RP_FUNCTOR_SER_REG(ProcedureDeclarator_ParamDeclarators);
RP_FUNCTOR_SER_REG(ProcedureDeclarator_RetDeclarator);
RP_FUNCTOR_SER_REG(Constant_TypeDeclarator);
RP_FUNCTOR_SER_REG(Constant_Initialization);
RP_FUNCTOR_SER_REG(VariableDeclaration_TypeDeclarator);
RP_FUNCTOR_SER_REG(VariableDeclaration_Initialization);
RP_FUNCTOR_SER_REG(Alias_Declarator);
RP_FUNCTOR_SER_REG(Struct_Extends);
RP_FUNCTOR_SER_REG(Struct_MemberVarDecl);
RP_FUNCTOR_SER_REG(Struct_ConstantDecl);
RP_FUNCTOR_SER_REG(Struct_MemberProc);
RP_FUNCTOR_SER_REG(Struct_MemberTemplateProc);
RP_FUNCTOR_SER_REG(Struct_InterfaceImpl);
RP_FUNCTOR_SER_REG(InterfaceDef_Proc);
RP_FUNCTOR_SER_REG(InterfaceImplementation_Identifier);
RP_FUNCTOR_SER_REG(InterfaceImplementation_Proc);
RP_FUNCTOR_SER_REG(ArgList_Expressions);
RP_FUNCTOR_SER_REG(Procedure_ParamVarDeclaration);
RP_FUNCTOR_SER_REG(Procedure_RetDeclarator);
RP_FUNCTOR_SER_REG(Procedure_ProcDeclarator);
RP_FUNCTOR_SER_REG(Procedure_Statement);
RP_FUNCTOR_SER_REG(Namespace_Node);
RP_FUNCTOR_SER_REG(Print_Args);
RP_FUNCTOR_SER_REG(Return_Expression);
RP_FUNCTOR_SER_REG(If_Conditional);
RP_FUNCTOR_SER_REG(If_Statement);
RP_FUNCTOR_SER_REG(If_Else);
RP_FUNCTOR_SER_REG(Else_Statement);
RP_FUNCTOR_SER_REG(For_Initialization);
RP_FUNCTOR_SER_REG(For_Conditional);
RP_FUNCTOR_SER_REG(For_Afterthought);
RP_FUNCTOR_SER_REG(For_Statement);
RP_FUNCTOR_SER_REG(While_Conditional);
RP_FUNCTOR_SER_REG(While_Statement);
RP_FUNCTOR_SER_REG(DoWhile_Conditional);
RP_FUNCTOR_SER_REG(DoWhile_Statement);
RP_FUNCTOR_SER_REG(Match_Expression);
RP_FUNCTOR_SER_REG(Match_With);
RP_FUNCTOR_SER_REG(With_Expression);
RP_FUNCTOR_SER_REG(With_Statement);
RP_FUNCTOR_SER_REG(TemplateDefineList_Element);
RP_FUNCTOR_SER_REG(TemplateDefineTypeDescriptor_Bound);
RP_FUNCTOR_SER_REG(TemplateDefineExpression_VarDecl);
RP_FUNCTOR_SER_REG(TemplateInstantiation_Element);
RP_FUNCTOR_SER_REG(TemplateAlias_Template);
RP_FUNCTOR_SER_REG(TemplateAlias_TemplateDef);
RP_FUNCTOR_SER_REG(TemplateStruct_Template);
RP_FUNCTOR_SER_REG(TemplateStruct_TemplateDef);
RP_FUNCTOR_SER_REG(TemplateProc_Template);
RP_FUNCTOR_SER_REG(TemplateProc_TemplateDef);

namespace bjou {
    void write_header(std::string header, boost::archive::text_oarchive& ar) {
        ar << header;
    }
    
    std::string read_header(boost::archive::text_iarchive& ar) {
        std::string header;
        ar >> header;
        return header;
    }
    
    void serializeAST(std::vector<ASTNode*>& AST, boost::archive::text_oarchive& ar) {
        ar << AST;
    }
    
    void deserializeAST(std::vector<ASTNode*>& AST, boost::archive::text_iarchive& ar) {
        BJOU_DEBUG_ASSERT(AST.empty());
        ar >> AST;
    }
    
    void serializeTypeTable(std::unordered_map<std::string, Type*>& typeTable, boost::archive::text_oarchive& ar) {
        ar << typeTable;
    }
    
    void deserializeTypeTable(std::unordered_map<std::string, Type*>& typeTable, boost::archive::text_iarchive& ar) {
        ar >> typeTable;
    }
    
    void serializeImportedIDs(std::set<std::string>& modulesImported, boost::archive::text_oarchive& ar) {
        ar << modulesImported;
    }
    
    void deserializeImportedIDs(std::set<std::string>& modulesImported, boost::archive::text_iarchive& ar) {
        ar >> modulesImported;
    }
}

#endif
