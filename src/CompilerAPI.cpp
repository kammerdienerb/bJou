//
//  CompilerAPI.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/18.
//  Copyright © 2018 me. All rights reserved.
//

#include "CompilerAPI.hpp"

#include "Compile.hpp"
#include "FrontEnd.hpp"
#include "Misc.hpp"
#include "Parser.hpp"

namespace bjou {

extern "C" Context * bjou_createContext(Loc * beg, Loc * end, const char * fname) {
    Context * c = new Context;
    c->begin = *beg;
    c->end = *end;
    c->filename = fname;

    return c;
}

extern "C" void bjou_freeContext(Context * c) { delete c; }

extern "C" void bjou_error(Context * c, const char * message) {
    errorl(*c, message);
}

extern "C" const char * bjou_getVersionString() { return BJOU_VER_STR; }

extern "C" void bjou_parseAndAppend(const char * str) {
    BJOU_DEBUG_ASSERT(str);
    AsyncParser p(str);
    p();
    for (ASTNode * node : p.nodes) {
        // we are already in analysis stage, so we need to catch up
        node->addSymbols(compilation->frontEnd.globalScope);
        // add the node
        compilation->frontEnd.deferredAST.push_back(node);
    }
}

extern "C" void bjou_appendNode(ASTNode * node) {
    compilation->frontEnd.deferredAST.push_back(node);
}

extern "C" void bjou_setGlobalNodeRP(ASTNode * node) {
    node->replace = rpget<replacementPolicy_Global_Node>();
}

extern "C" Scope * bjou_getGlobalScope() {
    return compilation->frontEnd.globalScope;
}

extern "C" ASTNode * bjou_clone(ASTNode * node) {
    return node->clone();
}

extern "C" void bjou_addSymbols(ASTNode * node, Scope * scope) {
    node->addSymbols(scope);
}

extern "C" void bjou_setContext(ASTNode * node, Context * c) {
    node->setContext(*c);
}

extern "C" void bjou_analyze(ASTNode * node) { node->analyze(); }

extern "C" void bjou_forceAnalyze(ASTNode * node) { node->analyze(true); }

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

extern "C" ASTNode * bjou_createAssignmentExpression(ASTNode * left, ASTNode * right) {
    AssignmentExpression * node = new AssignmentExpression;
    node->setContents("=");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createAddAssignExpression(ASTNode * left, ASTNode * right) {
    AddAssignExpression * node = new AddAssignExpression;
    node->setContents("+=");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createSubAssignExpression(ASTNode * left, ASTNode * right) {
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

extern "C" ASTNode * bjou_createDivAssignExpression(ASTNode * left, ASTNode * right) {
    DivAssignExpression * node = new DivAssignExpression;
    node->setContents("/=");
    node->setLeft(left);
    node->setRight(right);

    return node;
}

extern "C" ASTNode * bjou_createModAssignExpression(ASTNode * left, ASTNode * right) {
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

extern "C" ASTNode * bjou_createInjectExpression(ASTNode * left,
                                                 ASTNode * right) {
    InjectExpression * node = new InjectExpression;
    node->setContents("->");
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
    node->setUnqualified(unqualified);

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
                  ASTNode ** memberTemplateProcs, int n_memberTemplateProcs,
                  ASTNode ** interfaceImpls, int n_interfaceImpls) {

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
    for (int i = 0; i < n_interfaceImpls; i += 1)
        node->addInterfaceImpl(interfaceImpls[i]);

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

extern "C" ASTNode * bjou_createReturn(ASTNode * expression) {
    Return * node = new Return;
    if (expression)
        node->setExpression(expression);

    return node;
}

extern "C" ASTNode * bjou_createBreak() { return new Break; }

extern "C" ASTNode * bjou_createContinue() { return new Continue; }

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

extern "C" ASTNode * bjou_createMacroUse(const char * macroName, ASTNode ** args, int n_args) {
    MacroUse * node = new MacroUse;
    node->setMacroName(macroName);
    for (int i = 0; i < n_args; i += 1)
        node->addArg(args[i]);

    return node;
}

} // namespace bjou
