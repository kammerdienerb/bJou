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

namespace bjou {
namespace Macros {
static ASTNode * hello(MacroUse * use) {
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
    use->setFlag(ASTNode::CT, true);
    use->getArgs()[0]->analyze();

    if (((Expression*)use->getArgs()[0])->eval().as_i64) {
        use->getArgs()[1]->addSymbols(use->getScope());
        use->getArgs()[1]->analyze();
        return use->getArgs()[1];
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
    
    ArgList * arglist = (ArgList*)call->getRight();

    BJOU_DEBUG_ASSERT(arglist);

    std::vector<Val> val_args;

    for (ASTNode * _expr : arglist->getExpressions()) {
        Expression * expr = (Expression*)_expr;
        if (!expr->isConstant())
            errorl(expr->getContext(),
                    "run: argument value to compile-time invokation of '" +
                    proc->getName() + "' must be known at compile time.") ;
        val_args.push_back(expr->eval());
    }

    auto start = Clock::now();
    void * ret = compilation->backEnd.run(proc, &val_args);
    auto end = Clock::now();
    milliseconds time = duration_cast<milliseconds>(end - start);

    if (compilation->args.time_arg.getValue())
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
    use->setFlag(ASTNode::CT, true);

    use->getArgs()[0]->analyze();
    Procedure * proc = (Procedure *)use->getArgs()[0];
    const ProcedureType * t = (const ProcedureType *)proc->getType();

    if (!equal(t, ProcedureType::get({}, VoidType::get())))
        errorl(proc->getNameContext(),
               "Anonymouse procedure in $do must have type <()>", true,
               "Procedure has type " + t->getDemangledName());

    std::vector<Val> no_args;
    compilation->backEnd.run(proc, &no_args);

    return nullptr;
}

static ASTNode * add_llvm_pass(MacroUse * use) {
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
    multi->setFlag(ASTNode::CT, true);

    return multi;
}

static ASTNode * op(MacroUse * use) {
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
            new _Symbol<Procedure>(op_str, proc, proc->inst);

        use->getScope()->addSymbol(symbol, &proc->getNameContext());

        return proc;
    } else {
        TemplateProc * tproc = (TemplateProc *)proc_arg;
        Procedure * proc = (Procedure *)tproc->_template;

        _Symbol<TemplateProc> * symbol =
            new _Symbol<TemplateProc>(op_str, tproc);

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
    data->setUnqualified("__data");

    access->setLeft(expr);
    access->setRight(data);

    access->addSymbols(use->getScope());
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
    data->setUnqualified("__capacity");

    access->setLeft(expr);
    access->setRight(data);

    access->addSymbols(use->getScope());
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
    data->setUnqualified("__used");

    access->setLeft(expr);
    access->setRight(data);

    access->addSymbols(use->getScope());
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
    data->setUnqualified("__data");

    access->setLeft(expr);
    access->setRight(data);

    access->addSymbols(use->getScope());
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
    data->setUnqualified("__len");

    access->setLeft(expr);
    access->setRight(data);

    access->addSymbols(use->getScope());
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

    lit->addSymbols(use->getScope());
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
        "static_if", Macros::static_if, {{ANY_EXPRESSION}, {ANY_NODE}}, false, {1}};
    macros["same_type"] = {"same_type",
                           Macros::same_type,
                           {{ANY_DECLARATOR, ASTNode::NodeKind::IDENTIFIER},
                            {ANY_DECLARATOR, ASTNode::NodeKind::IDENTIFIER}}};
    macros["run"] = {
        "run", Macros::run, {{ASTNode::NodeKind::CALL_EXPRESSION}}};
    macros["static_do"] = {
        "static_do", Macros::static_do, {{ASTNode::NodeKind::PROCEDURE}}, true};
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
    macros["typeisstruct"] = {"typeisstruct",
                           Macros::typeisstruct,
                           {{ANY_DECLARATOR, ASTNode::NodeKind::IDENTIFIER}}};
    macros["typeispointer"] = {"typeispointer",
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
               true, "expected " + std::to_string(nexpected),
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
        if (i == arg_index)
            return false;

    return true;
}

} // namespace bjou
