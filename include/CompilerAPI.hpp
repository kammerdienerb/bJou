//
//  CompilerAPI.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/18.
//  Copyright © 2018 me. All rights reserved.
//

#ifndef CompilerAPI_hpp
#define CompilerAPI_hpp

#include "ASTNode.hpp"

namespace bjou {

extern "C" void bjou_StartDefaultCompilation(
    bool verbose_arg, bool front_arg, bool time_arg, bool symbols_arg,
    bool noparallel_arg, bool opt_arg, bool noabc_arg, bool module_arg,
    bool nopreload_arg, bool lld_arg, bool c_arg, bool emitllvm_arg,
    const char ** module_search_path_arg, int n_module_search_path_arg,
    const char * output_arg, const char * target_triple_arg, const char ** link_arg, int n_link_arg,
    const char ** files, int n_files);

extern "C" void bjou_dump(ASTNode ** nodes, int n_nodes, const char * fname,
                          bool dumpCT);

extern "C" const char * bjou_makeUID(const char * hint);

extern "C" bool bjou_isKeyword(const char * str);

extern "C" Context * bjou_createContext(Loc * beg, Loc * end,
                                        const char * fname);
extern "C" Context * bjou_getContext(ASTNode * node);
extern "C" const char * bjou_contextGetFileName(Context * c);
extern "C" void bjou_setContext(ASTNode * node, Context * c);
extern "C" void bjou_setProcNameContext(ASTNode * node, Context * c);
extern "C" void bjou_setVarDeclNameContext(ASTNode * node, Context * c);
extern "C" void bjou_setStructNameContext(ASTNode * node, Context * c);
extern "C" void bjou_setAliasNameContext(ASTNode * node, Context * c);
extern "C" void bjou_freeContext(Context * c);

extern "C" void bjou_error(Context * c, const char * message);

extern "C" const char * bjou_getVersionString();

extern "C" ASTNode * bjou_parseToMultiNode(const char * str);
extern "C" void bjou_parseAndAppend(const char * str);
extern "C" void bjou_appendNode(ASTNode * node);

extern "C" void bjou_runTypeCompletion();

extern "C" void bjou_setGlobalNodeRP(ASTNode * node);

extern "C" Scope * bjou_getGlobalScope();

extern "C" ASTNode * bjou_clone(ASTNode * node);
extern "C" void bjou_preDeclare(ASTNode * node, Scope * scope);
extern "C" void bjou_addSymbols(ASTNode * node, Scope * scope);
extern "C" void bjou_analyze(ASTNode * node);
extern "C" void bjou_forceAnalyze(ASTNode * node);
extern "C" const Type * bjou_getType(ASTNode * node);

extern "C" const char * bjou_getStructName(ASTNode * node);
extern "C" const char * bjou_getEnumName(ASTNode * node);
extern "C" void bjou_setVarDeclName(ASTNode * node, const char * name);

extern "C" ASTNode * bjou_makeZeroInitExpr(unsigned int typeKind, ASTNode * decl);

extern "C" ASTNode * bjou_createAddExpression(ASTNode * left, ASTNode * right);
extern "C" ASTNode * bjou_createSubExpression(ASTNode * left, ASTNode * right);
extern "C" ASTNode * bjou_createBSHLExpression(ASTNode * left, ASTNode * right);
extern "C" ASTNode * bjou_createBSHRExpression(ASTNode * left, ASTNode * right);
extern "C" ASTNode * bjou_createMultExpression(ASTNode * left, ASTNode * right);
extern "C" ASTNode * bjou_createDivExpression(ASTNode * left, ASTNode * right);
extern "C" ASTNode * bjou_createModExpression(ASTNode * left, ASTNode * right);

extern "C" ASTNode * bjou_createAssignmentExpression(ASTNode * left,
                                                     ASTNode * right);
extern "C" ASTNode * bjou_createAddAssignExpression(ASTNode * left,
                                                    ASTNode * right);
extern "C" ASTNode * bjou_createSubAssignExpression(ASTNode * left,
                                                    ASTNode * right);
extern "C" ASTNode * bjou_createMultAssignExpression(ASTNode * left,
                                                     ASTNode * right);
extern "C" ASTNode * bjou_createDivAssignExpression(ASTNode * left,
                                                    ASTNode * right);
extern "C" ASTNode * bjou_createModAssignExpression(ASTNode * left,
                                                    ASTNode * right);

extern "C" ASTNode * bjou_createLssExpression(ASTNode * left, ASTNode * right);
extern "C" ASTNode * bjou_createLeqExpression(ASTNode * left, ASTNode * right);
extern "C" ASTNode * bjou_createGtrExpression(ASTNode * left, ASTNode * right);
extern "C" ASTNode * bjou_createGeqExpression(ASTNode * left, ASTNode * right);
extern "C" ASTNode * bjou_createEquExpression(ASTNode * left, ASTNode * right);
extern "C" ASTNode * bjou_createNeqExpression(ASTNode * left, ASTNode * right);

extern "C" ASTNode * bjou_createLogAndExpression(ASTNode * left,
                                                 ASTNode * right);
extern "C" ASTNode * bjou_createLogOrExpression(ASTNode * left,
                                                ASTNode * right);
extern "C" ASTNode * bjou_createBANDExpression(ASTNode * left, ASTNode * right);
extern "C" ASTNode * bjou_createBORExpression(ASTNode * left, ASTNode * right);
extern "C" ASTNode * bjou_createBXORExpression(ASTNode * left, ASTNode * right);

extern "C" ASTNode * bjou_createCallExpression(ASTNode * left, ASTNode * right);

extern "C" ASTNode * bjou_createSubscriptExpression(ASTNode * left,
                                                    ASTNode * right);

extern "C" ASTNode * bjou_createAccessExpression(ASTNode * left,
                                                 ASTNode * right);

extern "C" ASTNode * bjou_createNewExpression(ASTNode * right);
extern "C" ASTNode * bjou_createDeleteExpression(ASTNode * right);

extern "C" ASTNode * bjou_createSizeofExpression(ASTNode * right);

extern "C" ASTNode * bjou_createNotExpression(ASTNode * right);
extern "C" ASTNode * bjou_createBNEGExpression(ASTNode * right);

extern "C" ASTNode * bjou_createDerefExpression(ASTNode * right);
extern "C" ASTNode * bjou_createAddressExpression(ASTNode * right);
extern "C" ASTNode * bjou_createRefExpression(ASTNode * right);

extern "C" ASTNode * bjou_createAsExpression(ASTNode * left, ASTNode * right);

extern "C" ASTNode * bjou_createIdentifier(const char * unqualified);
extern "C" ASTNode * bjou_createIdentifierWithInst(const char * unqualified, ASTNode * inst);

extern "C" ASTNode * bjou_createInitializerList(ASTNode * objDeclarator,
                                                char ** memberNames,
                                                int n_memberNames,
                                                ASTNode ** expressions,
                                                int n_expressions);

extern "C" ASTNode * bjou_createBooleanLiteral(const char * contents);
extern "C" ASTNode * bjou_createIntegerLiteral(const char * contents);
extern "C" ASTNode * bjou_createFloatLiteral(const char * contents);
extern "C" ASTNode * bjou_createStringLiteral(const char * contents);
extern "C" ASTNode * bjou_createCharLiteral(const char * contents);
extern "C" ASTNode * bjou_createExprBlock(ASTNode ** statements, int n_statements);

extern "C" ASTNode * bjou_createDeclarator(ASTNode * ident, ASTNode * inst,
                                           char ** specs, int n_specs);
extern "C" ASTNode * bjou_createArrayDeclarator(ASTNode * arrayOf,
                                                ASTNode * expression);
extern "C" ASTNode * bjou_createPointerDeclarator(ASTNode * pointerOf);

extern "C" ASTNode * bjou_createVariableDeclaration(const char * name,
                                                    ASTNode * typeDeclarator,
                                                    ASTNode * initialization);

extern "C" ASTNode * bjou_createFieldDeclaration(const char * name,
                                                 ASTNode * typeDeclarator,
                                                 ASTNode * initialization);

extern "C" ASTNode * bjou_createParamDeclaration(const char * name,
                                                 ASTNode * typeDeclarator,
                                                 ASTNode * initialization);

extern "C" ASTNode * bjou_createProcedureDeclarator(ASTNode ** paramDecls,
                                                    int n_paramDecls,
                                                    ASTNode * retDecl,
                                                    bool isVararg);

extern "C" ASTNode * bjou_createAlias(const char * name, ASTNode * decl);

extern "C" ASTNode * bjou_createStruct(
    const char * name, ASTNode * extends, ASTNode ** memberVarDecls,
    int n_memberVarDecls, ASTNode ** constantDecls, int n_constantDecls,
    ASTNode ** memberProcs, int n_memberProcs, ASTNode ** memberTemplateProcs,
    int n_memberTemplateProcs);

extern "C" ASTNode * bjou_createEnum(const char * name, const char ** identifiers, int n_identifiers);

extern "C" ASTNode * bjou_createArgList(ASTNode ** expressions,
                                        int n_expressions);

extern "C" ASTNode *
bjou_createProcedure(const char * name, ASTNode ** paramVarDeclarations,
                     int n_paramVarDeclarations, bool isVararg,
                     ASTNode * retDeclarator, ASTNode ** statements,
                     int n_statements);

extern "C" ASTNode * bjou_createExternProcedure(const char * name,
                                                ASTNode ** paramVarDeclarations,
                                                int n_paramVarDeclarations,
                                                bool isVararg,
                                                ASTNode * retDeclarator);

extern "C" ASTNode * bjou_createReturn(ASTNode * expression);
extern "C" ASTNode * bjou_createBreak();
extern "C" ASTNode * bjou_createContinue();

extern "C" ASTNode * bjou_createExprBlockYield(ASTNode * expression);

extern "C" ASTNode * bjou_createIf(ASTNode * conditional, ASTNode ** statements,
                                   int n_statements, ASTNode * _else);
extern "C" ASTNode * bjou_createElse(ASTNode ** statements, int n_statements);
extern "C" ASTNode *
bjou_createFor(ASTNode ** initializations, int n_initializations,
               ASTNode * conditional, ASTNode ** afterthoughts,
               int n_afterthoughts, ASTNode ** statements, int n_statements);
extern "C" ASTNode * bjou_createWhile(ASTNode * conditional,
                                      ASTNode ** statements, int n_statements);
extern "C" ASTNode * bjou_createDoWhile(ASTNode * conditional,
                                        ASTNode ** statements,
                                        int n_statements);

extern "C" ASTNode * bjou_createTemplateInst(ASTNode ** elements, int n_elements);

extern "C" ASTNode * bjou_createMacroUse(const char * macroName,
                                         ASTNode ** args, int n_args);

} // namespace bjou

#endif
