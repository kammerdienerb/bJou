//
//  Serialization.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 8/11/17.
//  Copyright © 2017 me. All rights reserved.
//

#if 0

#ifndef Serialization_hpp
#define Serialization_hpp

#include "ASTNode.hpp"

#include <set>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/set.hpp>

namespace bjou {
    boost::archive::text_oarchive create_oarchive(const char * fname);
    boost::archive::text_iarchive create_iarchive(const char * fname);
    void write_header(std::string header, boost::archive::text_oarchive& ar);
    std::string read_header(boost::archive::text_iarchive& ar);
    void serializeAST(std::vector<ASTNode*>& AST, boost::archive::text_oarchive& ar);
    void deserializeAST(std::vector<ASTNode*>& AST, boost::archive::text_iarchive& ar);
    void serializeTypeTable(std::unordered_map<std::string, Type*>&typeTable, boost::archive::text_oarchive& ar);
    void deserializeTypeTable(std::unordered_map<std::string, Type*>& typeTable, boost::archive::text_iarchive& ar);
    void serializeImportedIDs(std::set<std::string>& modulesImported, boost::archive::text_oarchive& ar);
    void deserializeImportedIDs(std::set<std::string>& modulesImported, boost::archive::text_iarchive& ar);
}

namespace boost {
namespace serialization {
    using namespace bjou;
    
    template <class Archive>
    void serialize(Archive& ar, replacementPolicy& rp, const unsigned int version) {  }
    
#define RP_FUNCTOR_SER(name)                                                                    \
    template <class Archive>                                                                    \
    void serialize(Archive& ar, replacementPolicy_##name& rp, const unsigned int version) {     \
        ar & boost::serialization::base_object<replacementPolicy>(rp);                          \
    }
    
    RP_FUNCTOR_SER(empty);
    RP_FUNCTOR_SER(Global_Node);
    RP_FUNCTOR_SER(ExpressionL);
    RP_FUNCTOR_SER(ExpressionR);
    RP_FUNCTOR_SER(InitializerList_ObjDeclarator);
    RP_FUNCTOR_SER(InitializerList_Expression);
    RP_FUNCTOR_SER(TupleLiteral_subExpression);
    RP_FUNCTOR_SER(Declarator_Identifier);
    RP_FUNCTOR_SER(Declarator_TemplateInst);
    RP_FUNCTOR_SER(ArrayDeclarator_ArrayOf);
    RP_FUNCTOR_SER(ArrayDeclarator_Expression);
    RP_FUNCTOR_SER(DynamicArrayDeclarator_ArrayOf);
    RP_FUNCTOR_SER(PointerDeclarator_PointerOf);
    RP_FUNCTOR_SER(MaybeDeclarator_MaybeOf);
    RP_FUNCTOR_SER(TupleDeclarator_subDeclarator);
    RP_FUNCTOR_SER(ProcedureDeclarator_ParamDeclarators);
    RP_FUNCTOR_SER(ProcedureDeclarator_RetDeclarator);
    RP_FUNCTOR_SER(Constant_TypeDeclarator);
    RP_FUNCTOR_SER(Constant_Initialization);
    RP_FUNCTOR_SER(VariableDeclaration_TypeDeclarator);
    RP_FUNCTOR_SER(VariableDeclaration_Initialization);
    RP_FUNCTOR_SER(Alias_Declarator);
    RP_FUNCTOR_SER(Struct_Extends);
    RP_FUNCTOR_SER(Struct_MemberVarDecl);
    RP_FUNCTOR_SER(Struct_ConstantDecl);
    RP_FUNCTOR_SER(Struct_MemberProc);
    RP_FUNCTOR_SER(Struct_MemberTemplateProc);
    RP_FUNCTOR_SER(Struct_InterfaceImpl);
    RP_FUNCTOR_SER(InterfaceDef_Proc);
    RP_FUNCTOR_SER(InterfaceImplementation_Identifier);
    RP_FUNCTOR_SER(InterfaceImplementation_Proc);
    RP_FUNCTOR_SER(ArgList_Expressions);
    RP_FUNCTOR_SER(Procedure_ParamVarDeclaration);
    RP_FUNCTOR_SER(Procedure_RetDeclarator);
    RP_FUNCTOR_SER(Procedure_ProcDeclarator);
    RP_FUNCTOR_SER(Procedure_Statement);
    RP_FUNCTOR_SER(Namespace_Node);
    RP_FUNCTOR_SER(Print_Args);
    RP_FUNCTOR_SER(Return_Expression);
    RP_FUNCTOR_SER(If_Conditional);
    RP_FUNCTOR_SER(If_Statement);
    RP_FUNCTOR_SER(If_Else);
    RP_FUNCTOR_SER(Else_Statement);
    RP_FUNCTOR_SER(For_Initialization);
    RP_FUNCTOR_SER(For_Conditional);
    RP_FUNCTOR_SER(For_Afterthought);
    RP_FUNCTOR_SER(For_Statement);
    RP_FUNCTOR_SER(While_Conditional);
    RP_FUNCTOR_SER(While_Statement);
    RP_FUNCTOR_SER(DoWhile_Conditional);
    RP_FUNCTOR_SER(DoWhile_Statement);
    RP_FUNCTOR_SER(Match_Expression);
    RP_FUNCTOR_SER(Match_With);
    RP_FUNCTOR_SER(With_Expression);
    RP_FUNCTOR_SER(With_Statement);
    RP_FUNCTOR_SER(TemplateDefineList_Element);
    RP_FUNCTOR_SER(TemplateDefineTypeDescriptor_Bound);
    RP_FUNCTOR_SER(TemplateDefineExpression_VarDecl);
    RP_FUNCTOR_SER(TemplateInstantiation_Element);
    RP_FUNCTOR_SER(TemplateAlias_Template);
    RP_FUNCTOR_SER(TemplateAlias_TemplateDef);
    RP_FUNCTOR_SER(TemplateStruct_Template);
    RP_FUNCTOR_SER(TemplateStruct_TemplateDef);
    RP_FUNCTOR_SER(TemplateProc_Template);
    RP_FUNCTOR_SER(TemplateProc_TemplateDef);
    
    template <class Archive>
    void serialize(Archive& ar, Loc& loc, const unsigned int version) {
        ar
            & loc.line
            & loc.character
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Context& context, const unsigned int version) {
        ar
            & context.filename
            & context.begin
            & context.end
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, ASTNode& node, const unsigned int version) {
        ar
            & node.nodeKind
            & node.flags
            & node.context
            & node.nameContext
            // & node.scope // handled after import by addSymbols()
            & node.parent
            & node.replace
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Expression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(expr);
        ar
            & expr.contents
            & expr.type
            & expr.left
            & expr.right
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, BinaryExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<Expression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, AddExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, SubExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, MultExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, DivExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, ModExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, AssignmentExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, AddAssignExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, SubAssignExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, MultAssignExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, DivAssignExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, ModAssignExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, MaybeAssignExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, LssExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, LeqExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, GtrExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, GeqExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, EquExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, NeqExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, LogAndExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, LogOrExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, CallExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, SubscriptExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, AccessExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, InjectExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<BinaryExpression>(expr);
    }
    
    template <class Archive>
    void serialize(Archive& ar, UnaryPreExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<Expression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, NewExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<UnaryPreExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, DeleteExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<UnaryPreExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, SizeofExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<UnaryPreExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, NotExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<UnaryPreExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, DerefExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<UnaryPreExpression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, AddressExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<UnaryPreExpression>(expr);
    }
    
    template <class Archive>
    void serialize(Archive& ar, UnaryPostExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<Expression>(expr);
    }
    template <class Archive>
    void serialize(Archive& ar, AsExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<UnaryPostExpression>(expr);
    }
    
    template <class Archive>
    void serialize(Archive& ar, Identifier& ident, const unsigned int version) {
        ar & boost::serialization::base_object<Expression>(ident);
        
        ar
            & ident.unqualified
            & ident.qualified
            & ident.namespaces
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, InitializerList& ilist, const unsigned int version) {
        ar & boost::serialization::base_object<Expression>(ilist);
        
        ar
            & ilist.objDeclarator
            & ilist.memberNames
            & ilist.expressions
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, BooleanLiteral& literal, const unsigned int version) {
        ar & boost::serialization::base_object<Expression>(literal);
    }
    
    template <class Archive>
    void serialize(Archive& ar, IntegerLiteral& literal, const unsigned int version) {
        ar & boost::serialization::base_object<Expression>(literal);
    }
    
    template <class Archive>
    void serialize(Archive& ar, FloatLiteral& literal, const unsigned int version) {
        ar & boost::serialization::base_object<Expression>(literal);
    }
    
    template <class Archive>
    void serialize(Archive& ar, StringLiteral& literal, const unsigned int version) {
        ar & boost::serialization::base_object<Expression>(literal);
    }
    
    template <class Archive>
    void serialize(Archive& ar, CharLiteral& literal, const unsigned int version) {
        ar & boost::serialization::base_object<Expression>(literal);
    }
    
    template <class Archive>
    void serialize(Archive& ar, ProcLiteral& literal, const unsigned int version) {
        ar & boost::serialization::base_object<UnaryPreExpression>(literal);
    }
    
    template <class Archive>
    void serialize(Archive& ar, ExternLiteral& literal, const unsigned int version) {
        ar & boost::serialization::base_object<UnaryPreExpression>(literal);
    }
    
    template <class Archive>
    void serialize(Archive& ar, SomeLiteral& literal, const unsigned int version) {
        ar & boost::serialization::base_object<UnaryPreExpression>(literal);
    }
    
    template <class Archive>
    void serialize(Archive& ar, NothingLiteral& literal, const unsigned int version) {
        ar & boost::serialization::base_object<Expression>(literal);
    }
    
    template <class Archive>
    void serialize(Archive& ar, TupleLiteral& literal, const unsigned int version) {
        ar & boost::serialization::base_object<Expression>(literal);
        
        ar & literal.subExpressions;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Declarator& decl, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(decl);
        
        ar
            & decl.identifier
            & decl.templateInst
            & decl.typeSpecifiers
            & decl.createdFromType
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, ArrayDeclarator& decl, const unsigned int version) {
        ar & boost::serialization::base_object<Declarator>(decl);
        
        ar
            & decl.arrayOf
            & decl.expression
            & decl.size
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, DynamicArrayDeclarator& decl, const unsigned int version) {
        ar & boost::serialization::base_object<Declarator>(decl);
        
        ar & decl.arrayOf;
    }
    
    template <class Archive>
    void serialize(Archive& ar, PointerDeclarator& decl, const unsigned int version) {
        ar & boost::serialization::base_object<Declarator>(decl);
        
        ar & decl.pointerOf;
    }
    
    template <class Archive>
    void serialize(Archive& ar, MaybeDeclarator& decl, const unsigned int version) {
        ar & boost::serialization::base_object<Declarator>(decl);
        
        ar & decl.maybeOf;
    }
    
    template <class Archive>
    void serialize(Archive& ar, TupleDeclarator& decl, const unsigned int version) {
        ar & boost::serialization::base_object<Declarator>(decl);
        
        ar & decl.subDeclarators;
    }
    
    template <class Archive>
    void serialize(Archive& ar, ProcedureDeclarator& decl, const unsigned int version) {
        ar & boost::serialization::base_object<Declarator>(decl);
        
        ar
            & decl.paramDeclarators
            & decl.retDeclarator
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Constant& constant, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(constant);
        
        ar
            & constant.name
            & constant.mangledName
            & constant.typeDeclarator
            & constant.initialization
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, VariableDeclaration& var, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(var);
        
        ar
            & var.name
            & var.mangledName
            & var.typeDeclarator
            & var.initialization
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Alias& alias, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(alias);
        
        ar
            & alias.name
            & alias.mangledName
            & alias.declarator
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Struct& s, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(s);
        
        ar
            & s.name
            & s.mangledName
            & s.extends
            & s.memberVarDecls
            & s.constantDecls
            & s.memberProcs
            & s.memberTemplateProcs
            & s.interfaceImpls
            & s.inst
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, InterfaceDef& def, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(def);
        
        ar
            & def.name
            & def.mangledName
            & def.procs
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, InterfaceImplementation& impl, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(impl);
        
        ar
            & impl.identifier
            & impl.procs
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Enum& e, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(e);
        
        ar
            & e.name
            & e.mangledName
            & e.identifiers
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, ArgList& args, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(args);
        
        ar & args.expressions;
    }
    
    template <class Archive>
    void serialize(Archive& ar, This& t, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(t);
    }
    
    template <class Archive>
    void serialize(Archive& ar, Procedure& proc, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(proc);
        
        ar
            & proc.name
            & proc.mangledName
            & proc.paramVarDeclarations
            & proc.retDeclarator
            & proc.procDeclarator
            & proc.statements
            & proc.inst
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Namespace& nspace, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(nspace);
        
        ar
            & nspace.name
            & nspace.nodes
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Import& import, const unsigned int version) {
        // We don't want Import nodes to be exported. The modules' contents will
        // already be dumped into the main AST at this point.
        
        /*
        ar & boost::serialization::base_object<ASTNode>(import);
        
        ar & import.module;
         */
    }
    
    template <class Archive>
    void serialize(Archive& ar, Print& print, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(print);
        
        ar & print.args;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Return& ret, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(ret);
        
        ar & ret.expression;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Break& b, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(b);
    }
    
    template <class Archive>
    void serialize(Archive& ar, Continue& c, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(c);
    }
    
    template <class Archive>
    void serialize(Archive& ar, If& i, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(i);
        
        ar
            & i.conditional
            & i.statements
            & i._else
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Else& e, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(e);
        
        ar & e.statements;
    }
    
    template <class Archive>
    void serialize(Archive& ar, For& f, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(f);
        
        ar
            & f.initializations
            & f.conditional
            & f.afterthoughts
            & f.statements
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, While& w, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(w);
        
        ar
            & w.conditional
            & w.statements
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, DoWhile& d, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(d);
        
        ar
            & d.conditional
            & d.statements
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Match& m, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(m);
        
        ar
            & m.expression
            & m.withs
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, With& w, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(w);
        
        ar
            & w.expression
            & w.statements
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, TemplateDefineList& def, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(def);
        
        ar & def.elements;
    }
    
    template <class Archive>
    void serialize(Archive& ar, TemplateDefineElement& elem, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(elem);
        
        ar & elem.name;
    }
    
    template <class Archive>
    void serialize(Archive& ar, TemplateDefineTypeDescriptor& desc, const unsigned int version) {
        ar & boost::serialization::base_object<TemplateDefineElement>(desc);
        
        ar & desc.bounds;
    }
    
    template <class Archive>
    void serialize(Archive& ar, TemplateDefineVariadicTypeArgs& vargs, const unsigned int version) {
        ar & boost::serialization::base_object<TemplateDefineElement>(vargs);
    }
    
    template <class Archive>
    void serialize(Archive& ar, TemplateDefineExpression& expr, const unsigned int version) {
        ar & boost::serialization::base_object<TemplateDefineElement>(expr);
        
        ar & expr.varDecl;
    }
    
    template <class Archive>
    void serialize(Archive& ar, TemplateInstantiation& inst, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(inst);
        
        ar & inst.elements;
    }
    
    template <class Archive>
    void serialize(Archive& ar, TemplateAlias& alias, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(alias);
        
        ar
            & alias._template
            & alias.templateDef
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, TemplateStruct& s, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(s);
        
        ar
            & s._template
            & s.templateDef
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, TemplateProc& proc, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(proc);
        
        ar
            & proc.mangledName
            & proc._template
            & proc.templateDef
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, SLComment& comment, const unsigned int version) {
        ar & boost::serialization::base_object<ASTNode>(comment);
        
        ar & comment.contents;
    }
    
    template <class Archive>
    void serialize(Archive& ar, Type& t, const unsigned int version) {
        ar
            & t.kind
            & t.sign
            & t.size
            & t.code
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, InvalidType& t, const unsigned int version) {
        ar & boost::serialization::base_object<Type>(t);
    }
    
    template <class Archive>
    void serialize(Archive& ar, StructType& t, const unsigned int version) {
        ar & boost::serialization::base_object<Type>(t);
        
        ar
            & t.isAbstract
            & t._struct
            & t.inst
            & t.extends
            & t.memberIndices
            & t.memberTypes
            & t.constantMap
            & t.memberProcs
            & t.interfaceIndexMap
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, EnumType& t, const unsigned int version) {
        ar & boost::serialization::base_object<Type>(t);
        
        ar & t.valMap;
    }
    
    template <class Archive>
    void serialize(Archive& ar, AliasType& t, const unsigned int version) {
        ar & boost::serialization::base_object<Type>(t);
        
        ar
            & t.alias
            & t.alias_of
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, ArrayType& t, const unsigned int version) {
        ar & boost::serialization::base_object<Type>(t);
        
        ar
            & t.array_of
            & t.expression
            & t.size
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, DynamicArrayType& t, const unsigned int version) {
        ar & boost::serialization::base_object<Type>(t);
        
        ar & t.array_of;
    }
    
    template <class Archive>
    void serialize(Archive& ar, PointerType& t, const unsigned int version) {
        ar & boost::serialization::base_object<Type>(t);
        
        ar & t.pointer_of;
    }
    
    template <class Archive>
    void serialize(Archive& ar, MaybeType& t, const unsigned int version) {
        ar & boost::serialization::base_object<Type>(t);
        
        ar & t.maybe_of;
    }
    
    template <class Archive>
    void serialize(Archive& ar, TupleType& t, const unsigned int version) {
        ar & boost::serialization::base_object<Type>(t);
        
        ar & t.subTypes;
    }
    
    template <class Archive>
    void serialize(Archive& ar, ProcedureType& t, const unsigned int version) {
        ar & boost::serialization::base_object<Type>(t);
        
        ar
            & t.paramTypes
            & t.isVararg
            & t.retType
        ;
    }
    
    template <class Archive>
    void serialize(Archive& ar, TemplateStructType& t, const unsigned int version) {
        ar & boost::serialization::base_object<Type>(t);
    }
    
    template <class Archive>
    void serialize(Archive& ar, TemplateAliasType& t, const unsigned int version) {
        ar & boost::serialization::base_object<Type>(t);
    }
    
} // namespace serialization
} // namespace boost

#endif /* Serialization_hpp */

#endif
