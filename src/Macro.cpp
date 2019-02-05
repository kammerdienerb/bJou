//
//  Macro.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/8/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "Macro.hpp"
#include "BackEnd.hpp"
#include "CLI.hpp"
#include "FrontEnd.hpp"
#include "LLVMBackEnd.hpp"
#include "Misc.hpp"

#ifdef BJOU_DEBUG_BUILD
#define SAVE_BJOU_DEBUG_BUILD
#endif
#undef BJOU_DEBUG_BUILD
#include "tclap/CmdLine.h"
#ifdef SAVE_BJOU_DEBUG_BUILD
#define BJOU_DEBUG_BUILD
#endif

#include <cstdlib>
#include <fstream>

namespace bjou {
namespace Macros {
static ASTNode * hello(MacroUse * use) {
    if (!use->replace->block)
        errorl(use->getNameContext(),
               "hello: replaced by nothing -- the code will be missing "
               "something it depends on.");

    printf("compile time hello!\n");
    return nullptr;
}

static ASTNode * rand(MacroUse * use) {
    Expression * low = (Expression *)use->getArgs()[0];
    Expression * high = (Expression *)use->getArgs()[1];

    low->analyze();
    high->analyze();

    int l = (int)low->eval().as_i64;
    int h = (int)high->eval().as_i64;

    int r = std::rand() % h + l;

    IntegerLiteral * lit = new IntegerLiteral();
    lit->setContext(use->getContext());
    lit->setScope(use->getScope());
    lit->setContents(std::to_string(r));

    return lit;
}

static ASTNode * static_if(MacroUse * use) {
    if (!use->replace->block)
        errorl(use->getNameContext(),
               "static_if: If the condition is false, the code will be missing "
               "something it depends on.");

    use->setFlag(ASTNode::CT, true);

    // static_if is marked as 'no add symbols' below.
    // we have to add symbols here just for the expression
    // argument.
    use->getArgs()[0]->addSymbols(use->mod, use->getScope());
    use->getArgs()[0]->analyze();

    if (((Expression *)use->getArgs()[0])->eval().as_i64) {
        std::vector<ASTNode *> keep(use->getArgs().begin() + 1,
                                    use->getArgs().end());
        MultiNode * multi = new MultiNode;
        multi->take(keep);
        multi->addSymbols(use->mod, use->getScope());
        // multi->analyze();
        return multi;
    }

    return nullptr;
}

static ASTNode * same_type(MacroUse * use) {
    // for types that are not built in, the args will be
    // Identifier* rather than Declarator* but this is okay
    // as long as we don't do anything stupid
    use->getArgs()[0]->analyze();
    use->getArgs()[1]->analyze();
    const Type * t1 = use->getArgs()[0]->getType();
    const Type * t2 = use->getArgs()[1]->getType();

    bool same = equal(t1, t2);

    BooleanLiteral * lit = new BooleanLiteral();
    lit->setContext(use->getContext());
    lit->setScope(use->getScope());
    lit->setContents(same ? "true" : "false");

    return lit;
}

static ASTNode * run(MacroUse * use) {
    // @incomplete:
    // We should be replacing with return values, or raising errors
    // if there is no value. See the error in static_if

    use->setFlag(ASTNode::CT, true);

    CallExpression * call = (CallExpression *)use->getArgs()[0];
    call->analyze();
    Procedure * proc = (Procedure *)call->resolved_proc;
    const ProcedureType * t = nullptr;

    if (!proc) {
        if (call->getLeft()->nodeKind == ASTNode::IDENTIFIER) {
            Identifier * ident = (Identifier *)call->getLeft();
            Maybe<Symbol *> m_sym = ident->getScope()->getSymbol(
                ident->getScope(), ident, &ident->getContext(), true, true,
                false);
            Symbol * sym = nullptr;
            m_sym.assignTo(sym);
            BJOU_DEBUG_ASSERT(sym);
            BJOU_DEBUG_ASSERT(sym->isProc());
            proc = (Procedure *)sym->node();
            BJOU_DEBUG_ASSERT(proc->getFlag(Procedure::IS_EXTERN));
        }
    }

    BJOU_DEBUG_ASSERT(proc);

    ArgList * arglist = (ArgList *)call->getRight();

    BJOU_DEBUG_ASSERT(arglist);

    std::vector<Val> val_args;

    for (ASTNode * _expr : arglist->getExpressions()) {
        Expression * expr = (Expression *)_expr;
        if (!expr->isConstant())
            errorl(expr->getContext(),
                   "run: argument value to compile-time invocation of '" +
                       proc->getName() + "' must be known at compile time.");
        val_args.push_back(expr->eval());
    }

    auto start = Clock::now();
    void * ret = compilation->backEnd.run(proc, &val_args);
    auto end = Clock::now();
    milliseconds time = duration_cast<milliseconds>(end - start);

    if (compilation->args.time_arg)
        prettyPrintTimeMin(
            time,
            "\\run '" + proc->getName() + "' from " +
                use->getContext().filename +
                " :: " + std::to_string(use->getContext().begin.line) +
                " :: " + std::to_string(use->getContext().begin.character));

    compilation->frontEnd.ctruntime += time;

    return nullptr;
}

static ASTNode * static_do(MacroUse * use) {
    if (!use->replace->block)
        errorl(use->getNameContext(),
               "static_do: replaced by nothing -- the code will be missing "
               "something it depends on.");

    use->setFlag(ASTNode::CT, true);

    Procedure * proc = new Procedure;
    proc->setName(compilation->frontEnd.makeUID("__bjou_static_do"));
    proc->setContext(use->getContext());
    proc->setNameContext(use->getContext());
    Identifier * vi = new Identifier;
    vi->setSymName("void");
    vi->setContext(use->getContext());
    Declarator * vd = new Declarator;
    vd->setIdentifier(vi);
    vd->setContext(use->getContext());

    proc->setRetDeclarator(vd);

    for (auto arg : use->getArgs())
        proc->addStatement(arg->clone());

    proc->setFlag(ASTNode::CT, true);

    proc->addSymbols(use->mod, use->getScope());
    proc->analyze();

    std::vector<Val> no_args;
    compilation->backEnd.run(proc, &no_args);

    return nullptr;
}

static ASTNode * add_llvm_pass(MacroUse * use) {
    if (!use->replace->block)
        errorl(use->getNameContext(),
               "add_llvm_pass: replaced by nothing -- the code will be missing "
               "something it depends on.");

    ASTNode * p = use->parent;
    while (p && p->nodeKind == ASTNode::MULTINODE)
        p = p->parent;
    if (!p || p->nodeKind != ASTNode::PROCEDURE)
        errorl(use->getContext(),
               "add_llvm_pass must be used in the main body of a procedure");

    Procedure * proc = (Procedure *)p;

    StringLiteral * lit = (StringLiteral *)use->getArgs()[0];
    std::string pass_name = de_quote(lit->getContents());

    LLVMBackEnd * llbe = (LLVMBackEnd *)&compilation->backEnd;

    auto search = llbe->pass_create_by_name.find(pass_name);
    if (search == llbe->pass_create_by_name.end())
        errorl(lit->getContext(),
               "add_llvm_pass: no pass registered under '" + pass_name + "'");

    llbe->addProcedurePass(proc, pass_name);

    return nullptr;
}

static ASTNode * add_llvm_passes(MacroUse * use) {
    if (!use->replace->block)
        errorl(use->getNameContext(),
               "add_llvm_passes: replaced by nothing -- the code will be "
               "missing something it depends on.");

    ASTNode * p = use->parent;
    while (p && p->nodeKind == ASTNode::MULTINODE)
        p = p->parent;
    if (!p || p->nodeKind != ASTNode::PROCEDURE)
        errorl(use->getContext(),
               "add_llvm_passes must be used in the main body of a procedure");

    Procedure * proc = (Procedure *)p;

    LLVMBackEnd * llbe = (LLVMBackEnd *)&compilation->backEnd;

    for (ASTNode * arg : use->getArgs()) {
        if (arg->nodeKind != ASTNode::STRING_LITERAL)
            errorl(arg->getContext(),
                   "add_llvm_passes arguments must be string literals");

        StringLiteral * lit = (StringLiteral *)arg;
        std::string pass_name = de_quote(lit->getContents());

        auto search = llbe->pass_create_by_name.find(pass_name);
        if (search == llbe->pass_create_by_name.end())
            errorl(lit->getContext(),
                   "add_llvm_pass: no pass registered under '" + pass_name +
                       "'");

        llbe->addProcedurePass(proc, pass_name);
    }

    return nullptr;
}

static ASTNode * ct(MacroUse * use) {
    MultiNode * multi = new MultiNode(use->getArgs());

    printf("!!! CT\n");

    for (ASTNode * node : multi->nodes) {
        if (use->leaveMeAloneArgs.find(node) == use->leaveMeAloneArgs.end())
            node->setFlag(ASTNode::CT, true);
    }

    return multi;
}

static ASTNode * op(MacroUse * use) {
    if (!use->replace->block)
        errorl(use->getNameContext(),
               "op: replaced by nothing -- the code will be missing something "
               "it depends on.");

    const char * overloadable[]{"+", "<", ">", "[]", "==", "!="};

    ASTNode * op_arg = use->getArgs()[0];
    ASTNode * proc_arg = use->getArgs()[1];

    std::string op_str = de_quote(((Expression *)op_arg)->getContents());

    if (!s_in_a(op_str.c_str(), overloadable))
        errorl(op_arg->getContext(),
               "op: Overloading '" + op_str + "' not allowed.");

    if (proc_arg->nodeKind == ASTNode::NodeKind::PROCEDURE) {
        Procedure * proc = (Procedure *)proc_arg;

        _Symbol<Procedure> * symbol =
            new _Symbol<Procedure>(op_str, use->mod, "", proc, proc->inst, nullptr);

        use->getScope()->addSymbol(symbol, &proc->getNameContext());

        return proc;
    } else {
        TemplateProc * tproc = (TemplateProc *)proc_arg;
        Procedure * proc = (Procedure *)tproc->_template;

        _Symbol<TemplateProc> * symbol =
            new _Symbol<TemplateProc>(op_str, use->mod, "", tproc, nullptr, tproc->getTemplateDef());

        use->getScope()->addSymbol(symbol, &proc->getNameContext());

        return tproc;
    }

    return nullptr;
}

static ASTNode * __da_data(MacroUse * use) {
    Expression * expr = (Expression *)use->getArgs()[0];
    const Type * t = expr->getType()->unRef();

    if (!t->isDynamicArray()) {
        errorl(expr->getContext(),
               "__da_data: Expression is not a dynamic array.", true,
               "got '" + t->getDemangledName() + "'");
    }

    StructType * struct_t =
        (StructType *)((DynamicArrayType *)t)->getRealType();

    const Type * result_t =
        struct_t->memberTypes[struct_t->memberIndices["__data"]];

    AccessExpression * access = new AccessExpression;
    access->setContext(use->getContext());

    Identifier * data = new Identifier;
    data->setContext(use->getContext());
    data->setSymName("__data");

    access->setLeft(expr);
    access->setRight(data);

    access->addSymbols(use->mod, use->getScope());
    access->setType(result_t);
    access->setFlag(AccessExpression::ANALYZED, true);

    return access;
}

static ASTNode * __da_capacity(MacroUse * use) {
    Expression * expr = (Expression *)use->getArgs()[0];
    const Type * t = expr->getType()->unRef();

    if (!t->isDynamicArray()) {
        errorl(expr->getContext(),
               "__da_capacity: Expression is not a dynamic array.", true,
               "got '" + t->getDemangledName() + "'");
    }

    StructType * struct_t =
        (StructType *)((DynamicArrayType *)t)->getRealType();

    const Type * result_t =
        struct_t->memberTypes[struct_t->memberIndices["__capacity"]];

    AccessExpression * access = new AccessExpression;
    access->setContext(use->getContext());

    Identifier * data = new Identifier;
    data->setContext(use->getContext());
    data->setSymName("__capacity");

    access->setLeft(expr);
    access->setRight(data);

    access->addSymbols(use->mod, use->getScope());
    access->setType(result_t);
    access->setFlag(AccessExpression::ANALYZED, true);

    return access;
}

static ASTNode * __da_used(MacroUse * use) {
    Expression * expr = (Expression *)use->getArgs()[0];
    const Type * t = expr->getType()->unRef();

    if (!t->isDynamicArray()) {
        errorl(expr->getContext(),
               "__da_used: Expression is not a dynamic array.", true,
               "got '" + t->getDemangledName() + "'");
    }

    StructType * struct_t =
        (StructType *)((DynamicArrayType *)t)->getRealType();

    const Type * result_t =
        struct_t->memberTypes[struct_t->memberIndices["__used"]];

    AccessExpression * access = new AccessExpression;
    access->setContext(use->getContext());

    Identifier * data = new Identifier;
    data->setContext(use->getContext());
    data->setSymName("__used");

    access->setLeft(expr);
    access->setRight(data);

    access->addSymbols(use->mod, use->getScope());
    access->setType(result_t);
    access->setFlag(AccessExpression::ANALYZED, true);

    return access;
}

static ASTNode * __slice_data(MacroUse * use) {
    Expression * expr = (Expression *)use->getArgs()[0];
    const Type * t = expr->getType()->unRef();

    if (!t->isSlice()) {
        errorl(expr->getContext(), "__slice_data: Expression is not a slice.",
               true, "got '" + t->getDemangledName() + "'");
    }

    StructType * struct_t = (StructType *)((SliceType *)t)->getRealType();

    const Type * result_t =
        struct_t->memberTypes[struct_t->memberIndices["__data"]];

    AccessExpression * access = new AccessExpression;
    access->setContext(use->getContext());

    Identifier * data = new Identifier;
    data->setContext(use->getContext());
    data->setSymName("__data");

    access->setLeft(expr);
    access->setRight(data);

    access->addSymbols(use->mod, use->getScope());
    access->setType(result_t);
    access->setFlag(AccessExpression::ANALYZED, true);

    return access;
}

static ASTNode * __slice_len(MacroUse * use) {
    Expression * expr = (Expression *)use->getArgs()[0];
    const Type * t = expr->getType()->unRef();

    if (!t->isSlice()) {
        errorl(expr->getContext(), "__slice_len: Expression is not a slice.",
               true, "got '" + t->getDemangledName() + "'");
    }

    StructType * struct_t = (StructType *)((SliceType *)t)->getRealType();

    const Type * result_t =
        struct_t->memberTypes[struct_t->memberIndices["__len"]];

    AccessExpression * access = new AccessExpression;
    access->setContext(use->getContext());

    Identifier * data = new Identifier;
    data->setContext(use->getContext());
    data->setSymName("__len");

    access->setLeft(expr);
    access->setRight(data);

    access->addSymbols(use->mod, use->getScope());
    access->setType(result_t);
    access->setFlag(AccessExpression::ANALYZED, true);

    return access;
}

static ASTNode * abc(MacroUse * use) {
    bool abc = compilation->frontEnd.abc;

    BooleanLiteral * lit = new BooleanLiteral();
    lit->setContext(use->getContext());
    lit->setScope(use->getScope());
    lit->setContents(abc ? "true" : "false");

    return lit;
}

static ASTNode * error(MacroUse * use) {
    StringLiteral * lit = (StringLiteral *)use->getArgs()[0];
    std::string message = de_quote(lit->getContents());

    errorl(use->getContext(), message);

    return nullptr;
}

static ASTNode * os(MacroUse * use) {
    IntegerLiteral * lit = new IntegerLiteral;
#if defined(__linux__)
    lit->setContents("1");
#elif defined(__APPLE__)
    lit->setContents("2");
#elif defined(_WIN32)
    lit->setContents("3");
#else
    lit->setContents("0");
#endif

    lit->addSymbols(use->mod, use->getScope());
    lit->analyze();

    return lit;
}

static ASTNode * typeisstruct(MacroUse * use) {
    use->getArgs()[0]->analyze();
    const Type * t = use->getArgs()[0]->getType();

    bool isStruct = t->isStruct();

    BooleanLiteral * lit = new BooleanLiteral();
    lit->setContext(use->getContext());
    lit->setScope(use->getScope());
    lit->setContents(isStruct ? "true" : "false");

    return lit;
}

static ASTNode * typeispointer(MacroUse * use) {
    use->getArgs()[0]->analyze();
    const Type * t = use->getArgs()[0]->getType();

    bool isPointer = t->isPointer();

    BooleanLiteral * lit = new BooleanLiteral();
    lit->setContext(use->getContext());
    lit->setScope(use->getScope());
    lit->setContents(isPointer ? "true" : "false");

    return lit;
}

static ASTNode * typeisproc(MacroUse * use) {
    use->getArgs()[0]->analyze();
    const Type * t = use->getArgs()[0]->getType();

    bool isProc = t->isProcedure();

    BooleanLiteral * lit = new BooleanLiteral();
    lit->setContext(use->getContext());
    lit->setScope(use->getScope());
    lit->setContents(isProc ? "true" : "false");

    return lit;
}

static ASTNode * typeisint(MacroUse * use) {
    use->getArgs()[0]->analyze();
    const Type * t = use->getArgs()[0]->getType();

    bool isInt = t->isInt();

    BooleanLiteral * lit = new BooleanLiteral();
    lit->setContext(use->getContext());
    lit->setScope(use->getScope());
    lit->setContents(isInt ? "true" : "false");

    return lit;
}

static ASTNode * typeischar(MacroUse * use) {
    use->getArgs()[0]->analyze();
    const Type * t = use->getArgs()[0]->getType();

    bool isChar = t->isChar();

    BooleanLiteral * lit = new BooleanLiteral();
    lit->setContext(use->getContext());
    lit->setScope(use->getScope());
    lit->setContents(isChar ? "true" : "false");

    return lit;
}

static ASTNode * typeisfloat(MacroUse * use) {
    use->getArgs()[0]->analyze();
    const Type * t = use->getArgs()[0]->getType();

    bool isFloat = t->isFloat();

    BooleanLiteral * lit = new BooleanLiteral();
    lit->setContext(use->getContext());
    lit->setScope(use->getScope());
    lit->setContents(isFloat ? "true" : "false");

    return lit;
}

static ASTNode * front(MacroUse * use) {
    if (!use->replace->block)
        errorl(use->getNameContext(),
               "front: replaced by nothing -- the code will be missing "
               "something it depends on.");

    use->getArgs()[0]->analyze();

    bool f = ((Expression *)use->getArgs()[0])->eval().as_i64;

    compilation->args.front_arg = f;

    return nullptr;
}

static ASTNode * frontisset(MacroUse * use) {
    BooleanLiteral * result = new BooleanLiteral();
    result->setContext(use->getContext());
    result->setScope(use->getScope());
    result->setContents(compilation->args.front_arg ? "true" : "false");

    return result;
}

static ASTNode * canfindmodule(MacroUse * use) {
    StringLiteral * lit = (StringLiteral *)use->getArgs()[0];
    std::string path = de_quote(lit->getContents());

    std::ifstream in;
    for (std::string & spath : compilation->module_search_paths) {
        in.open(spath + path, std::ios::binary);
        if (in)
            break;
    }

    BooleanLiteral * result = new BooleanLiteral();
    result->setContext(use->getContext());
    result->setScope(use->getScope());
    result->setContents(in ? "true" : "false");

    in.close();

    return result;
}

static ASTNode * die(MacroUse * use) {
    Expression * expr = (Expression *)use->getArgs()[0];
    expr->analyze();

    const Type * t = expr->getType()->unRef();

    if (!t->isInt() &&
        !((t->isPointer() || t->isArray()) && t->under()->isChar())) {
        errorl(expr->getContext(),
               "die: Expression is not an errcode (int) or a message (char*).",
               true, "got '" + t->getDemangledName() + "'");
    }

    CallExpression * call = new CallExpression;
    call->setScope(use->getScope());
    call->setContext(use->getContext());

    Identifier * ident = new Identifier;
    ident->setScope(use->getScope());
    ident->setContext(use->getContext());
    ident->setSymMod("__die");
    ident->setSymName("__die");

    ArgList * args = new ArgList;
    args->setScope(use->getScope());
    args->setContext(use->getContext());

    StringLiteral * fl = new StringLiteral;
    fl->setScope(use->getScope());
    fl->setContext(use->getContext());
    fl->setContents(use->getContext().filename);

    StringLiteral * pn = new StringLiteral;
    pn->setScope(use->getScope());
    pn->setContext(use->getContext());
    ASTNode * p = use->getParent();
    while (p && p->nodeKind != ASTNode::PROCEDURE)
        p = p->getParent();
    if (p) {
        pn->setContents(((Procedure *)p)->getName());
    } else {
        pn->setContents("global statement");
    }

    IntegerLiteral * ln = new IntegerLiteral;
    ln->setScope(use->getScope());
    ln->setContext(use->getContext());
    ln->setContents(std::to_string(use->getContext().begin.line));

    fl->analyze();
    pn->analyze();
    ln->analyze();

    args->addExpression(expr->clone());
    args->addExpression(fl);
    args->addExpression(pn);
    args->addExpression(ln);

    call->setLeft(ident);
    call->setRight(args);

    return call;
}

static ASTNode * isbigendian(MacroUse * use) {
    BooleanLiteral * lit = new BooleanLiteral;
#if BYTE_ORDER == BIG_ENDIAN
    lit->setContents("true");
#else
    lit->setContents("false");
#endif

    lit->addSymbols(use->mod, use->getScope());
    lit->analyze();

    return lit;
}

static ASTNode * typetag(MacroUse * use) {
    IntegerLiteral * lit = new IntegerLiteral;
    
    const Type * t = use->getArgs()[0]->getType();
    lit->setContents(std::to_string(std::hash<std::string>{}(t->key)) + "u64");

    lit->addSymbols(use->mod, use->getScope());
    lit->analyze();

    return lit;
}

static ASTNode * add_global_using(MacroUse * use) {
    Identifier * ident = (Identifier*)use->getArgs()[0];
    compilation->frontEnd.globalScope->usings.push_back(ident->getSymName());

    return nullptr;
}

static ASTNode * volatile_r(MacroUse * use) {
    use->getArgs()[0]->setFlag(Expression::VOLATILE_R, true);

    return use->getArgs()[0];
}

static ASTNode * volatile_w(MacroUse * use) {
    use->getArgs()[0]->setFlag(Expression::VOLATILE_W, true);

    return use->getArgs()[0];
}

} // namespace Macros

Macro::Macro() : name(""), dispatch(nullptr), arg_kinds({}), isVararg(false) {}

Macro::Macro(std::string _name, MacroDispatch_fn _dispatch,
             std::vector<std::vector<ASTNode::NodeKind>> _arg_kinds,
             bool _isVararg, std::vector<int> _args_no_add_symbols)
    : name(_name), dispatch(_dispatch), arg_kinds(_arg_kinds),
      isVararg(_isVararg), args_no_add_symbols(_args_no_add_symbols) {}

MacroManager::MacroManager() {
    macros["hello"] = {"hello", Macros::hello, {}};
    macros["rand"] = {"rand",
                      Macros::rand,
                      {{ASTNode::NodeKind::INTEGER_LITERAL},
                       {ASTNode::NodeKind::INTEGER_LITERAL}}};
    macros["static_if"] = {
        "static_if", Macros::static_if, {{ANY_EXPRESSION}}, true, {-1}};
    macros["same_type"] = {"same_type",
                           Macros::same_type,
                           {{ANY_DECLARATOR, ASTNode::NodeKind::IDENTIFIER},
                            {ANY_DECLARATOR, ASTNode::NodeKind::IDENTIFIER}}};
    macros["run"] = {
        "run", Macros::run, {{ASTNode::NodeKind::CALL_EXPRESSION}}};
    macros["static_do"] = {"static_do", Macros::static_do, {}, true, {-1}};
    macros["add_llvm_pass"] = {"add_llvm_pass",
                               Macros::add_llvm_pass,
                               {{ASTNode::NodeKind::STRING_LITERAL}}};
    macros["add_llvm_passes"] = {
        "add_llvm_passes", Macros::add_llvm_passes, {}, true};
    macros["ct"] = {"ct", Macros::ct, {}, true};
    macros["op"] = {
        "op",
        Macros::op,
        {{ASTNode::NodeKind::STRING_LITERAL},
         {ASTNode::NodeKind::PROCEDURE, ASTNode::NodeKind::TEMPLATE_PROC}}};

    macros["__da_data"] = {"__da_data", Macros::__da_data, {{ANY_EXPRESSION}}};
    macros["__da_capacity"] = {
        "__da_capacity", Macros::__da_capacity, {{ANY_EXPRESSION}}};
    macros["__da_used"] = {"__da_used", Macros::__da_used, {{ANY_EXPRESSION}}};

    macros["__slice_data"] = {
        "__slice_data", Macros::__slice_data, {{ANY_EXPRESSION}}};
    macros["__slice_len"] = {
        "__slice_len", Macros::__slice_len, {{ANY_EXPRESSION}}};

    macros["abc"] = {"abc", Macros::abc, {}};

    macros["error"] = {
        "error", Macros::error, {{ASTNode::NodeKind::STRING_LITERAL}}};

    macros["os"] = {"os", Macros::os, {}};
    macros["typeisstruct"] = {
        "typeisstruct",
        Macros::typeisstruct,
        {{ANY_DECLARATOR, ASTNode::NodeKind::IDENTIFIER}}};
    macros["typeispointer"] = {
        "typeispointer",
        Macros::typeispointer,
        {{ANY_DECLARATOR, ASTNode::NodeKind::IDENTIFIER}}};
    macros["typeisproc"] = {"typeisproc",
                            Macros::typeisproc,
                            {{ANY_DECLARATOR, ASTNode::NodeKind::IDENTIFIER}}};
    macros["typeisint"] = {"typeisint",
                           Macros::typeisint,
                           {{ANY_DECLARATOR, ASTNode::NodeKind::IDENTIFIER}}};
    macros["typeischar"] = {"typeischar",
                            Macros::typeischar,
                            {{ANY_DECLARATOR, ASTNode::NodeKind::IDENTIFIER}}};
    macros["typeisfloat"] = {"typeisfloat",
                             Macros::typeisfloat,
                             {{ANY_DECLARATOR, ASTNode::NodeKind::IDENTIFIER}}};
    macros["front"] = {"front", Macros::front, {{ANY_EXPRESSION}}};
    macros["frontisset"] = {"frontisset", Macros::frontisset, {}};
    macros["canfindmodule"] = {"canfindmodule",
                               Macros::canfindmodule,
                               {{ASTNode::NodeKind::STRING_LITERAL}}};
    macros["die"] = {"die", Macros::die, {{ANY_EXPRESSION}}};
    macros["isbigendian"] = {"isbigendian", Macros::isbigendian, {}};
    macros["typetag"] = {"typetag",
                             Macros::typetag,
                             {{ANY_DECLARATOR, ASTNode::NodeKind::IDENTIFIER}}};
    macros["add_global_using"] = {"add_global_using", Macros::add_global_using, {{ASTNode::NodeKind::IDENTIFIER}}};
    macros["volatile_r"] = {"volatile_r", Macros::volatile_r, {{ASTNode::NodeKind::DEREF_EXPRESSION}}};
    macros["volatile_w"] = {"volatile_w", Macros::volatile_w, {{ASTNode::NodeKind::ASSIGNMENT_EXPRESSION}}};
}

ASTNode * MacroManager::invoke(MacroUse * use) {
    std::string & name = use->getMacroName();
    auto search = macros.find(name);
    if (search == macros.end())
        errorl(use->getContext(), "No macro named '" + name + "' found.");
    Macro & macro = search->second;

    BJOU_DEBUG_ASSERT(macro.dispatch);

    // verify args
    int nargs = (int)use->getArgs().size();
    int nexpected = (int)macro.arg_kinds.size();

    bool arg_err = false;
    Context errContext;

    if (nargs > nexpected && !macro.isVararg) {
        arg_err = true;
        errContext = use->getArgs()[nexpected]->getContext();
    } else if (nargs < nexpected) {
        arg_err = true;
        errContext = use->getContext();
    }

    if (arg_err)
        errorl(errContext, "Wrong number of args for macro '" + name + "'",
               true,
               "expected " + (macro.isVararg ? "at least " : std::string("")) +
                   std::to_string(nexpected),
               "got " + std::to_string(nargs));

    for (int i = 0; i < nexpected; i += 1) {
        ASTNode * arg = use->getArgs()[i];
        auto search = std::find(macro.arg_kinds[i].begin(),
                                macro.arg_kinds[i].end(), arg->nodeKind);

        if (arg->nodeKind != ASTNode::MACRO_USE &&
            search == macro.arg_kinds[i].end()) {
            errContext = arg->getContext();

            std::string expected;
            for (ASTNode::NodeKind & k : macro.arg_kinds[i]) {
                expected += compilation->frontEnd.kind2string[k];
                if (k != macro.arg_kinds[i].back())
                    expected += ", ";
            }
            errorl(errContext, "Macro argument is not of the correct AST kind",
                   true, "allowed: " + expected,
                   "got " + compilation->frontEnd.kind2string[arg->nodeKind]);
        }
    }

    return macro.dispatch(use);
}

bool MacroManager::shouldAddSymbols(MacroUse * use, int arg_index) {
    std::string & name = use->getMacroName();
    auto search = macros.find(name);
    if (search == macros.end())
        errorl(use->getContext(), "No macro named '" + name + "' found.");
    Macro & macro = search->second;

    for (int i : macro.args_no_add_symbols)
        if (i == arg_index || i == -1)
            return false;

    return true;
}

} // namespace bjou
