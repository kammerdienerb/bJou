//
//  CompilerAPI.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/18.
//  Copyright © 2018 me. All rights reserved.
//

#include "CompilerAPI.hpp"

#include "CLI.hpp"
#include "Compile.hpp"
#include "FrontEnd.hpp"
#include "Misc.hpp"
#include "Parser.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace bjou {

extern "C" void bjou_StartDefaultCompilation(
    bool verbose_arg, bool front_arg, bool time_arg, bool symbols_arg,
    bool noparallel_arg, bool opt_arg, bool noabc_arg, bool module_arg,
    bool nopreload_arg, bool lld_arg, bool c_arg, bool emitllvm_arg,
    const char ** module_search_path_arg, int n_module_search_path_arg,
    const char * output_arg, const char * target_triple_arg, const char ** link_arg, int n_link_arg,
    const char ** files, int n_files) {

    std::vector<std::string> _module_search_path_arg;
    for (int i = 0; i < n_module_search_path_arg; i += 1)
        _module_search_path_arg.push_back(module_search_path_arg[i]);
    std::string _output_arg = output_arg;
    std::string _target_triple_arg = output_arg;
    std::vector<std::string> _link_arg;
    for (int i = 0; i < n_link_arg; i += 1)
        _link_arg.push_back(link_arg[i]);
    std::vector<std::string> _files;
    for (int i = 0; i < n_files; i += 1)
        _files.push_back(files[i]);

    bjou::ArgSet args = {verbose_arg,
                         front_arg,
                         time_arg,
                         symbols_arg,
                         noparallel_arg,
                         opt_arg,
                         noabc_arg,
                         module_arg,
                         nopreload_arg,
                         lld_arg,
                         c_arg,
                         emitllvm_arg,
                         _module_search_path_arg,
                         _output_arg,
                         _target_triple_arg,
                         _link_arg,
                         _files};

    StartDefaultCompilation(args);
}

extern "C" void bjou_dump(ASTNode ** nodes, int n_nodes, const char * fname,
                          bool dumpCT) {
    std::ofstream of(fname);

    if (!of) {
        Context err_ctx;
        error("Unable to open file '" + std::string(fname) +
              "' for bjou_dump().");
    }

    for (int i = 0; i < n_nodes; i += 1) {
        nodes[i]->dump(of, 0, dumpCT);
        of << "\n";
    }

    of.close();
}

extern "C" const char * bjou_makeUID(const char * hint) {
    std::string uid = compilation->frontEnd.makeUID(hint);
    return strdup(uid.c_str());
}

extern "C" bool bjou_isKeyword(const char * str) {
    std::string s(str);
    return is_kwd(s);
}

extern "C" Context * bjou_createContext(Loc * beg, Loc * end,
                                        const char * fname) {
    Context * c = new Context;
    c->begin = *beg;
    c->end = *end;
    c->filename = fname;

    return c;
}

extern "C" Context * bjou_getContext(ASTNode * node) {
    return &node->getContext();
}

extern "C" const char * bjou_contextGetFileName(Context * c) {
    return c->filename.c_str();
}

extern "C" void bjou_setContext(ASTNode * node, Context * c) {
    node->setContext(*c);
}

extern "C" void bjou_setProcNameContext(ASTNode * node, Context * c) {
    BJOU_DEBUG_ASSERT(node && node->nodeKind == ASTNode::PROCEDURE);
    ((Procedure *)node)->setNameContext(*c);
}

extern "C" void bjou_setVarDeclNameContext(ASTNode * node, Context * c) {
    BJOU_DEBUG_ASSERT(node && node->nodeKind == ASTNode::VARIABLE_DECLARATION);
    ((VariableDeclaration *)node)->setNameContext(*c);
}

extern "C" void bjou_setStructNameContext(ASTNode * node, Context * c) {
    BJOU_DEBUG_ASSERT(node && node->nodeKind == ASTNode::STRUCT);
    ((Struct *)node)->setNameContext(*c);
}

extern "C" void bjou_setAliasNameContext(ASTNode * node, Context * c) {
    BJOU_DEBUG_ASSERT(node && node->nodeKind == ASTNode::ALIAS);
    ((Alias *)node)->setNameContext(*c);
}

extern "C" void bjou_freeContext(Context * c) { delete c; }

extern "C" void bjou_error(Context * c, const char * message) {
    errorl(*c, message);
}

extern "C" const char * bjou_getVersionString() { return BJOU_VER_STR; }

extern "C" ASTNode * bjou_parseToMultiNode(const char * str) {
    BJOU_DEBUG_ASSERT(str);
    AsyncParser p(str);
    p();

    return new MultiNode(p.nodes);
}

extern "C" void bjou_parseAndAppend(const char * str) {
    BJOU_DEBUG_ASSERT(str);
    AsyncParser p(str);
    p();
    for (ASTNode * node : p.nodes) {
        // we are already in analysis stage, so we need to catch up
        std::string empty_mod_string = "";
        node->addSymbols(empty_mod_string, compilation->frontEnd.globalScope);
        // add the node
        compilation->frontEnd.deferredAST.push_back(node);
    }
}

extern "C" void bjou_appendNode(ASTNode * node) {
    compilation->frontEnd.deferredAST.push_back(node);
}

extern "C" void bjou_runTypeCompletion() { compilation->frontEnd.TypesStage(); }

extern "C" void bjou_setGlobalNodeRP(ASTNode * node) {
    node->replace = rpget<replacementPolicy_Global_Node>();
}

extern "C" Scope * bjou_getGlobalScope() {
    return compilation->frontEnd.globalScope;
}

extern "C" ASTNode * bjou_clone(ASTNode * node) { return node->clone(); }

extern "C" void bjou_preDeclare(ASTNode * node, Scope * scope) {
    BJOU_DEBUG_ASSERT(node->nodeKind == ASTNode::STRUCT);
    if (node->nodeKind == ASTNode::STRUCT && "node cannot be predeclared") {
        std::string empty_mod_string = "";
        ((Struct *)node)->preDeclare(empty_mod_string, scope);
    }
}

extern "C" void bjou_addSymbols(ASTNode * node, Scope * scope) {
    std::string empty_mod_string = "";
    node->addSymbols(empty_mod_string, scope);
}

extern "C" void bjou_analyze(ASTNode * node) { node->analyze(); }

extern "C" void bjou_forceAnalyze(ASTNode * node) { node->analyze(true); }

extern "C" const Type * bjou_getType(ASTNode * node) {
    return node->getType();
}

extern "C" const char * bjou_getStructName(ASTNode * node) {
    BJOU_DEBUG_ASSERT(node->nodeKind == ASTNode::STRUCT);
    return strdup(((Struct *)node)->getName().c_str());
}

extern "C" const char * bjou_getEnumName(ASTNode * node) {
    BJOU_DEBUG_ASSERT(node->nodeKind == ASTNode::ENUM);
    return strdup(((Enum *)node)->getName().c_str());
}

extern "C" void bjou_setVarDeclName(ASTNode * node, const char * name) {
    BJOU_DEBUG_ASSERT(node->nodeKind == ASTNode::VARIABLE_DECLARATION);
    ((VariableDeclaration *)node)->setName(name);
}

extern "C" ASTNode * bjou_makeZeroInitExpr(unsigned int typeKind, ASTNode * decl) {
    using Kind = bjou::Type::Kind;

    Kind kind = (Kind)typeKind;

    switch (kind) {
        case Kind::INT:
            return bjou_createIntegerLiteral("0");
        case Kind::CHAR:
            return bjou_createCharLiteral("'\0'");
        case Kind::FLOAT:
            return bjou_createFloatLiteral("0.0");
        case Kind::POINTER:
        case Kind::PROCEDURE: {
            ASTNode * n =
                bjou_createIdentifier("NULL");
            ASTNode * as =
                bjou_createAsExpression(
                    n,
                    decl);
            
            return as;
        }
        case Kind::STRUCT: {
            return bjou_createInitializerList(
                    decl,
                    nullptr, 0,
                    nullptr, 0);
        }
        default:
            BJOU_DEBUG_ASSERT(false && "did not create zero initializer");
    }
}

extern "C" ASTNode * bjou_createAddExpression(ASTNode * left, ASTNode * right) {
    AddExpression * node = new AddExpression;
    node->setContents("+");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createSubExpression(ASTNode * left, ASTNode * right) {
    SubExpression * node = new SubExpression;
    node->setContents("-");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createBSHLExpression(ASTNode * left, ASTNode * right) {
    BSHLExpression * node = new BSHLExpression;
    node->setContents("bshl");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createBSHRExpression(ASTNode * left, ASTNode * right) {
    BSHRExpression * node = new BSHRExpression;
    node->setContents("bshr");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createMultExpression(ASTNode * left,
                                               ASTNode * right) {
    MultExpression * node = new MultExpression;
    node->setContents("*");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createDivExpression(ASTNode * left, ASTNode * right) {
    DivExpression * node = new DivExpression;
    node->setContents("/");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createModExpression(ASTNode * left, ASTNode * right) {
    ModExpression * node = new ModExpression;
    node->setContents("%");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createAssignmentExpression(ASTNode * left,
                                                     ASTNode * right) {
    AssignmentExpression * node = new AssignmentExpression;
    node->setContents("=");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createAddAssignExpression(ASTNode * left,
                                                    ASTNode * right) {
    AddAssignExpression * node = new AddAssignExpression;
    node->setContents("+=");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createSubAssignExpression(ASTNode * left,
                                                    ASTNode * right) {
    SubAssignExpression * node = new SubAssignExpression;
    node->setContents("-=");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createMultAssignExpression(ASTNode * left,
                                                     ASTNode * right) {
    MultAssignExpression * node = new MultAssignExpression;
    node->setContents("*=");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createDivAssignExpression(ASTNode * left,
                                                    ASTNode * right) {
    DivAssignExpression * node = new DivAssignExpression;
    node->setContents("/=");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createModAssignExpression(ASTNode * left,
                                                    ASTNode * right) {
    ModAssignExpression * node = new ModAssignExpression;
    node->setContents("%=");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createLssExpression(ASTNode * left, ASTNode * right) {
    LssExpression * node = new LssExpression;
    node->setContents("<");
    node->setLeft(left);
    node->setRight(right);

    return node;
}
extern "C" ASTNode * bjou_createLeqExpression(ASTNode * left, ASTNode * right) {
    LeqExpression * node = new LeqExpression;
    node->setContents("<=");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createGtrExpression(ASTNode * left, ASTNode * right) {
    GtrExpression * node = new GtrExpression;
    node->setContents(">");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createGeqExpression(ASTNode * left, ASTNode * right) {
    GeqExpression * node = new GeqExpression;
    node->setContents(">=");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createEquExpression(ASTNode * left, ASTNode * right) {
    EquExpression * node = new EquExpression;
    node->setContents("==");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createNeqExpression(ASTNode * left, ASTNode * right) {
    NeqExpression * node = new NeqExpression;
    node->setContents("!=");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createLogAndExpression(ASTNode * left,
                                                 ASTNode * right) {
    LogAndExpression * node = new LogAndExpression;
    node->setContents("and");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createLogOrExpression(ASTNode * left,
                                                ASTNode * right) {
    LogOrExpression * node = new LogOrExpression;
    node->setContents("or");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createBANDExpression(ASTNode * left,
                                                ASTNode * right) {
    BANDExpression * node = new BANDExpression;
    node->setContents("band");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createBORExpression(ASTNode * left,
                                                ASTNode * right) {
    BORExpression * node = new BORExpression;
    node->setContents("bor");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createBXORExpression(ASTNode * left,
                                                ASTNode * right) {
    BXORExpression * node = new BXORExpression;
    node->setContents("bxor");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createCallExpression(ASTNode * left,
                                               ASTNode * right) {
    CallExpression * node = new CallExpression;
    node->setContents("()");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createSubscriptExpression(ASTNode * left,
                                                    ASTNode * right) {
    SubscriptExpression * node = new SubscriptExpression;
    node->setContents("[]");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createAccessExpression(ASTNode * left,
                                                 ASTNode * right) {
    AccessExpression * node = new AccessExpression;
    node->setContents(".");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createNewExpression(ASTNode * right) {
    NewExpression * node = new NewExpression;
    node->setContents("new");
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createDeleteExpression(ASTNode * right) {
    DeleteExpression * node = new DeleteExpression;
    node->setContents("delete");
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createSizeofExpression(ASTNode * right) {
    SizeofExpression * node = new SizeofExpression;
    node->setContents("sizeof");
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createNotExpression(ASTNode * right) {
    NotExpression * node = new NotExpression;
    node->setContents("not");
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createBNEGExpression(ASTNode * right) {
    BNEGExpression * node = new BNEGExpression;
    node->setContents("bneg");
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createDerefExpression(ASTNode * right) {
    DerefExpression * node = new DerefExpression;
    node->setContents("@");
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createAddressExpression(ASTNode * right) {
    AddressExpression * node = new AddressExpression;
    node->setContents("&");
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createRefExpression(ASTNode * right) {
    RefExpression * node = new RefExpression;
    node->setContents("~");
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createAsExpression(ASTNode * left, ASTNode * right) {
    AsExpression * node = new AsExpression;
    node->setContents("as");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createIdentifier(const char * unqualified) {
    Identifier * node = new Identifier;
    node->setSymName(unqualified);

    return node;
}

extern "C" ASTNode * bjou_createIdentifierWithInst(const char * unqualified, ASTNode * inst) {
    Identifier * node = new Identifier;
    node->setSymName(unqualified);
    node->setRight(inst);

    return node;
}

extern "C" ASTNode * bjou_createInitializerList(ASTNode * objDeclarator,
                                                char ** memberNames,
                                                int n_memberNames,
                                                ASTNode ** expressions,
                                                int n_expressions) {
    InitializerList * node = new InitializerList;
    if (objDeclarator)
        node->setObjDeclarator(objDeclarator);
    for (int i = 0; i < n_memberNames; i += 1)
        node->addMemberName(memberNames[i]);
    for (int i = 0; i < n_expressions; i += 1)
        node->addExpression(expressions[i]);

    return node;
}

extern "C" ASTNode * bjou_createBooleanLiteral(const char * contents) {
    BooleanLiteral * node = new BooleanLiteral;
    node->setContents(contents);

    return node;
}

extern "C" ASTNode * bjou_createIntegerLiteral(const char * contents) {
    IntegerLiteral * node = new IntegerLiteral;
    node->setContents(contents);

    return node;
}

extern "C" ASTNode * bjou_createFloatLiteral(const char * contents) {
    FloatLiteral * node = new FloatLiteral;
    node->setContents(contents);

    return node;
}

extern "C" ASTNode * bjou_createStringLiteral(const char * contents) {
    StringLiteral * node = new StringLiteral;
    node->setContents(contents);

    return node;
}

extern "C" ASTNode * bjou_createCharLiteral(const char * contents) {
    CharLiteral * node = new CharLiteral;
    node->setContents(contents);

    return node;
}

extern "C" ASTNode * bjou_createExprBlock(ASTNode ** statements, int n_statements) {
    ExprBlock * node = new ExprBlock;

    for (int i = 0; i < n_statements; i += 1) {
        node->addStatement(statements[i]);
    }

    return node;
}

extern "C" ASTNode * bjou_createDeclarator(ASTNode * ident, ASTNode * inst,
                                           char ** specs, int n_specs) {
    Declarator * node = new Declarator;
    node->setIdentifier(ident);
    if (inst)
        node->setTemplateInst(inst);
    for (int i = 0; i < n_specs; i += 1)
        node->addTypeSpecifier(specs[i]);

    return node;
}

extern "C" ASTNode * bjou_createArrayDeclarator(ASTNode * arrayOf,
                                                ASTNode * expression) {
    ArrayDeclarator * node = new ArrayDeclarator(arrayOf, expression);

    return node;
}

extern "C" ASTNode * bjou_createPointerDeclarator(ASTNode * pointerOf) {
    PointerDeclarator * node = new PointerDeclarator(pointerOf);

    return node;
}

extern "C" ASTNode * bjou_createProcedureDeclarator(ASTNode ** paramDecls,
                                                    int n_paramDecls,
                                                    ASTNode * retDecl,
                                                    bool isVararg) {
    ProcedureDeclarator * node = new ProcedureDeclarator;

    for (int i = 0; i < n_paramDecls; i += 1)
        node->addParamDeclarator(paramDecls[i]);

    node->setRetDeclarator(retDecl);

    node->setFlag(ProcedureDeclarator::IS_VARARG, isVararg);

    return node;
}

extern "C" ASTNode * bjou_createVariableDeclaration(const char * name,
                                                    ASTNode * typeDeclarator,
                                                    ASTNode * initialization) {
    VariableDeclaration * node = new VariableDeclaration;
    node->setName(name);
    if (typeDeclarator)
        node->setTypeDeclarator(typeDeclarator);
    if (initialization)
        node->setInitialization(initialization);

    return node;
}

extern "C" ASTNode * bjou_createFieldDeclaration(const char * name,
                                                 ASTNode * typeDeclarator,
                                                 ASTNode * initialization) {
    VariableDeclaration * node = new VariableDeclaration;
    node->setName(name);
    if (typeDeclarator)
        node->setTypeDeclarator(typeDeclarator);
    if (initialization)
        node->setInitialization(initialization);

    node->setFlag(VariableDeclaration::IS_TYPE_MEMBER, true);

    return node;
}

extern "C" ASTNode * bjou_createParamDeclaration(const char * name,
                                                 ASTNode * typeDeclarator,
                                                 ASTNode * initialization) {
    VariableDeclaration * node = new VariableDeclaration;
    node->setName(name);
    if (typeDeclarator)
        node->setTypeDeclarator(typeDeclarator);
    if (initialization)
        node->setInitialization(initialization);

    node->setFlag(VariableDeclaration::IS_PROC_PARAM, true);

    return node;
}

extern "C" ASTNode * bjou_createAlias(const char * name, ASTNode * decl) {
    Alias * node = new Alias;
    node->setName(name);
    node->setDeclarator(decl);

    return node;
}

extern "C" ASTNode *
bjou_createStruct(const char * name, ASTNode * extends,
                  ASTNode ** memberVarDecls, int n_memberVarDecls,
                  ASTNode ** constantDecls, int n_constantDecls,
                  ASTNode ** memberProcs, int n_memberProcs,
                  ASTNode ** memberTemplateProcs, int n_memberTemplateProcs) {

    Struct * node = new Struct;
    node->setName(name);
    if (extends)
        node->setExtends(extends);
    for (int i = 0; i < n_memberVarDecls; i += 1)
        node->addMemberVarDecl(memberVarDecls[i]);
    for (int i = 0; i < n_constantDecls; i += 1)
        node->addConstantDecl(constantDecls[i]);
    for (int i = 0; i < n_memberProcs; i += 1)
        node->addMemberProc(memberProcs[i]);
    for (int i = 0; i < n_memberTemplateProcs; i += 1)
        node->addMemberTemplateProc(memberTemplateProcs[i]);

    return node;
}

extern "C" ASTNode * bjou_createEnum(const char * name, const char ** identifiers, int n_identifiers) {
    Enum * node = new Enum;
    node->setName(name);

    for (int i = 0; i < n_identifiers; i += 1) {
        node->addIdentifier(identifiers[i]);
    }

    return node;
}

extern "C" ASTNode * bjou_createArgList(ASTNode ** expressions,
                                        int n_expressions) {
    ArgList * node = new ArgList;
    for (int i = 0; i < n_expressions; i += 1)
        node->addExpression(expressions[i]);

    return node;
}

extern "C" ASTNode *
bjou_createProcedure(const char * name, ASTNode ** paramVarDeclarations,
                     int n_paramVarDeclarations, bool isVararg,
                     ASTNode * retDeclarator, ASTNode ** statements,
                     int n_statements) {
    Procedure * node = new Procedure;
    node->setName(name);
    for (int i = 0; i < n_paramVarDeclarations; i += 1)
        node->addParamVarDeclaration(paramVarDeclarations[i]);
    node->setFlag(Procedure::IS_VARARG, isVararg);
    node->setRetDeclarator(retDeclarator);
    for (int i = 0; i < n_statements; i += 1)
        node->addStatement(statements[i]);

    return node;
}

extern "C" ASTNode * bjou_createExternProcedure(const char * name,
                                                ASTNode ** paramVarDeclarations,
                                                int n_paramVarDeclarations,
                                                bool isVararg,
                                                ASTNode * retDeclarator) {
    Procedure * node = new Procedure;
    node->setFlag(Procedure::IS_EXTERN, true);
    node->setName(name);
    for (int i = 0; i < n_paramVarDeclarations; i += 1)
        node->addParamVarDeclaration(paramVarDeclarations[i]);
    node->setFlag(Procedure::IS_VARARG, isVararg);
    node->setRetDeclarator(retDeclarator);

    return node;
}

extern "C" ASTNode * bjou_createReturn(ASTNode * expression) {
    Return * node = new Return;
    if (expression)
        node->setExpression(expression);

    return node;
}

extern "C" ASTNode * bjou_createBreak() { return new Break; }

extern "C" ASTNode * bjou_createContinue() { return new Continue; }

extern "C" ASTNode * bjou_createExprBlockYield(ASTNode * expression) {
    ExprBlockYield * node = new ExprBlockYield;

    node->setExpression(expression);

    return node;
}

extern "C" ASTNode * bjou_createIf(ASTNode * conditional, ASTNode ** statements,
                                   int n_statements, ASTNode * _else) {
    If * node = new If;
    node->setConditional(conditional);
    for (int i = 0; i < n_statements; i += 1)
        node->addStatement(statements[i]);
    if (_else)
        node->setElse(_else);

    return node;
}

extern "C" ASTNode * bjou_createElse(ASTNode ** statements, int n_statements) {
    Else * node = new Else;
    for (int i = 0; i < n_statements; i += 1)
        node->addStatement(statements[i]);

    return node;
}

extern "C" ASTNode *
bjou_createFor(ASTNode ** initializations, int n_initializations,
               ASTNode * conditional, ASTNode ** afterthoughts,
               int n_afterthoughts, ASTNode ** statements, int n_statements) {
    For * node = new For;
    for (int i = 0; i < n_initializations; i += 1)
        node->addInitialization(initializations[i]);
    node->setConditional(conditional);
    for (int i = 0; i < n_afterthoughts; i += 1)
        node->addAfterthought(afterthoughts[i]);
    for (int i = 0; i < n_statements; i += 1)
        node->addStatement(statements[i]);

    return node;
}

extern "C" ASTNode * bjou_createWhile(ASTNode * conditional,
                                      ASTNode ** statements, int n_statements) {
    While * node = new While;
    node->setConditional(conditional);
    for (int i = 0; i < n_statements; i += 1)
        node->addStatement(statements[i]);

    return node;
}

extern "C" ASTNode * bjou_createDoWhile(ASTNode * conditional,
                                        ASTNode ** statements,
                                        int n_statements) {
    DoWhile * node = new DoWhile;
    node->setConditional(conditional);
    for (int i = 0; i < n_statements; i += 1)
        node->addStatement(statements[i]);

    return node;
}

extern "C" ASTNode * bjou_createTemplateInst(ASTNode ** elements, int n_elements) {
    TemplateInstantiation * node = new TemplateInstantiation;
    for (int i = 0; i < n_elements; i += 1)
        node->addElement(elements[i]);

    return node;
}

extern "C" ASTNode * bjou_createMacroUse(const char * macroName,
                                         ASTNode ** args, int n_args) {
    MacroUse * node = new MacroUse;
    node->setMacroName(macroName);
    for (int i = 0; i < n_args; i += 1)
        node->addArg(args[i]);

    return node;
}

} // namespace bjou
