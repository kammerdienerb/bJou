//
//  ASTNode.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/4/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "ASTNode.hpp"
#include "CLI.hpp"
#include "Compile.hpp"
#include "Evaluate.hpp"
#include "FrontEnd.hpp"
#include "Global.hpp"
#include "Misc.hpp"
#include "Operator.hpp"
#include "Symbol.hpp"
#include "Template.hpp"

#include <bitset>
#include <iostream>
#include <iterator>
#include <mutex>
#include <string.h> // bzero

namespace bjou {

SelfDestruct::SelfDestruct() : node(nullptr) {}
SelfDestruct::SelfDestruct(ASTNode * _node) : node(_node) {}
void SelfDestruct::set(ASTNode * _node) { node = _node; }
void SelfDestruct::defuse() { node = nullptr; }
SelfDestruct::~SelfDestruct() {
    if (node)
        delete node;
}

// ~~~~~ ASTNode ~~~~~

ASTNode::ASTNode()
    : nodeKind(NONE), flags(0), context({}), scope(nullptr), parent(nullptr),
      replace(nullptr) {
    //
    // compilation->frontEnd.n_nodes++;
}

ASTNode::~ASTNode() {
    Val v;
    //
    // compilation->frontEnd.n_nodes--;
}

int ASTNode::getFlags() const { return flags; }
void ASTNode::setFlags(int _flags) { flags = _flags; }

#define FLAG_GET(flags, bit) (((flags) << (31 - bit) >> 31))
#define FLAG_PUT(flags, bit, val) ((flags) ^= (-(val) ^ (flags)) & (1 << (bit)))

int ASTNode::getFlag(int flag) const { return FLAG_GET(getFlags(), flag); }
void ASTNode::setFlag(int flag, bool val) {
    int flags = getFlags();
    FLAG_PUT(flags, flag, (int)val);
    setFlags(flags);
}

bool isCT(ASTNode * node) {
    while (node) {
        if (node->getFlag(ASTNode::CT))
            return true;
        node = node->parent;
    }
    return false;
}

void ASTNode::printFlags() { std::cout << std::bitset<32>(getFlags()) << "\n"; }

Context & ASTNode::getContext() { return context; }
void ASTNode::setContext(Context _context) { context = _context; }

Context & ASTNode::getNameContext() { return nameContext; }
void ASTNode::setNameContext(Context _context) { nameContext = _context; }

Scope * ASTNode::getScope() const { return scope; }
void ASTNode::setScope(Scope * _scope) {
    if (IS_DECLARATOR(this))
        ((Declarator *)this)->propagateScope(_scope);
    else
        scope = _scope;
}

void ASTNode::unwrap(std::vector<ASTNode *> & terminals) {}
ASTNode * ASTNode::clone() { return nullptr; }
void ASTNode::desugar() {}
void * ASTNode::generate(BackEnd & backEnd, bool flag) { return nullptr; }
const Type * ASTNode::getType() { return nullptr; }
bool ASTNode::isStatement() const { return false; }

#define HANDLE_FORCE()                                                         \
    if (getFlag(ANALYZED) && !force)                                           \
    return

MultiNode::MultiNode(std::vector<ASTNode *> & _nodes) : nodes(_nodes) {
    nodeKind = MULTINODE;
    for (ASTNode * node : nodes) {
        node->parent = this;
        node->replace = rpget<replacementPolicy_MultiNode_Node>();
    }
}

void MultiNode::analyze(bool force) {
    for (ASTNode * node : nodes)
        node->analyze(force);
}

void MultiNode::addSymbols(Scope * _scope) {
    setScope(_scope);
    for (ASTNode * node : nodes) {
        if (node->nodeKind != STRUCT && node->nodeKind != INTERFACE_DEF)
            node->addSymbols(_scope);
    }
}

void MultiNode::unwrap(std::vector<ASTNode *> & terminals) {
    for (ASTNode * node : nodes)
        node->unwrap(terminals);
}

ASTNode * MultiNode::clone() {
    MultiNode * clone = new MultiNode(*this);
    return clone;
}

MultiNode::~MultiNode() {
    for (ASTNode * node : nodes)
        delete node;
}

// ~~~~~ Expression ~~~~~

Expression::Expression()
    : contents({}), type(nullptr), left(nullptr), right(nullptr) {
    nodeKind = EXPRESSION;
}

std::string & Expression::getContents() { return contents; }
void Expression::setContents(std::string _contents) { contents = _contents; }

void Expression::setType(const Type * _type) { type = _type; }

ASTNode * Expression::getLeft() const { return left; }
void Expression::setLeft(ASTNode * _left) {
    left = _left;
    left->parent = this;
    left->replace = rpget<replacementPolicy_ExpressionL>();
}

ASTNode * Expression::getRight() const { return right; }
void Expression::setRight(ASTNode * _right) {
    right = _right;
    right->parent = this;
    right->replace = rpget<replacementPolicy_ExpressionR>();
}

Val Expression::eval() {
    errorl(getContext(), "Expression type does not implement eval().", false,
           "expression contents: '" + getContents() + "'");
    internalError("Could not evaluate expression.");
    return {};
}

bool Expression::opOverload() {
    BJOU_DEBUG_ASSERT(!getContents().empty());

    const char * op = getContents().c_str();

    if (getLeft())
        getLeft()->analyze();
    if (getRight())
        getRight()->analyze();

    Symbol * sym = nullptr;
    Maybe<Symbol *> m_sym = getScope()->getSymbol(getScope(), getContents(),
                                                  nullptr, true, false, false);

    if (m_sym.assignTo(sym)) {
        BJOU_DEBUG_ASSERT(sym->isProcSet());

        ArgList * args = new ArgList;

        if (binary(op)) {
            args->addExpression(getLeft()->clone());
            args->addExpression(getRight()->clone());
        } else {
            if (rightAssoc(op))
                args->addExpression(getRight()->clone());
            else
                args->addExpression(getLeft()->clone());
        }

        ProcSet * set = (ProcSet *)sym->node();
        Procedure * proc = set->get(args, nullptr, nullptr, false);
        if (proc) {
            // need to check args here since there may only be one
            // overload that is returned without checking
            std::vector<const Type *> arg_types;
            for (ASTNode * expr : args->getExpressions())
                arg_types.push_back(expr->getType());

            const Type * compare_type =
                ProcedureType::get(arg_types, VoidType::get());

            if (argMatch(compare_type, proc->getType())) {
                CallExpression * call = new CallExpression;

                call->setContext(getContext());
                call->setLeft(
                    mangledStringtoIdentifier(proc->getMangledName()));
                call->setRight(args);

                call->addSymbols(getScope());
                call->analyze();

                setType(call->getType());

                (*replace)(parent, this, call);
                return true;
            }
        }

        delete args;
    }

    return false;
}

bool Expression::canBeLVal() {
    if (getType()->isRef())
        return true;

    if (getFlag(Expression::IDENT)) {
        Maybe<Symbol *> m_sym =
            getScope()->getSymbol(getScope(), (Identifier *)this, &getContext(),
                                  true, true, false, false);
        Symbol * sym = nullptr;
        m_sym.assignTo(sym);
        BJOU_DEBUG_ASSERT(sym);

        if (!sym->isVar())
            return false;
    } else {
        if (getFlag(Expression::TERMINAL))
            return false;

        // const char * assignableOps[] = {"[]", ".", "=", "??", "@"};

        if (nodeKind == SUBSCRIPT_EXPRESSION) {
            if (getLeft()->getType()->isSlice())
                return false;
        } else if (nodeKind == ACCESS_EXPRESSION) {
            if (getLeft()->nodeKind == IDENTIFIER) {
                getLeft()->analyze();
                Identifier * ident = (Identifier *)getLeft();
                if (ident->resolved) {
                    if (ident->resolved->nodeKind != VARIABLE_DECLARATION)
                        return false;
                }
            }
        } else if (nodeKind == ASSIGNMENT_EXPRESSION) {
        } else if (nodeKind == DEREF_EXPRESSION) {
        }
    }

    return true;
}

// Node interface
void Expression::unwrap(std::vector<ASTNode *> & terminals) {
    if (getFlag(TERMINAL))
        terminals.push_back(this);

    if (getLeft())
        getLeft()->unwrap(terminals);
    if (getRight())
        getRight()->unwrap(terminals);
}

const Type * Expression::getType() {
    analyze();
    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    return type;
}

bool Expression::isStatement() const { return true; }

void Expression::analyze(bool force) {
    HANDLE_FORCE();
    // is Expression abstract?
    internalError("I think Expression is abstract.");
    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

void Expression::addSymbols(Scope * _scope) {
    setScope(_scope);
    ASTNode * left = getLeft();
    ASTNode * right = getRight();
    if (left)
        left->addSymbols(scope);
    if (right)
        right->addSymbols(scope);
}

void Expression::desugar() {
    if (getLeft())
        getLeft()->desugar();
    if (getRight())
        getRight()->desugar();
}

Expression::~Expression() {
    // if (type)
    // delete type;
    if (nodeKind == ACCESS_EXPRESSION &&
        ((AccessExpression *)this)->injection) {
        // ownership of left and right given away to a different node..
    } else {
        if (left)
            delete left;
        if (right)
            delete right;
    }
}
//

// ~~~~~ BinaryExpression ~~~~~

BinaryExpression::BinaryExpression() { nodeKind = BINARY_EXPRESSION; }

bool BinaryExpression::isConstant() {
    analyze();

    Expression * left = (Expression *)getLeft();
    Expression * right = (Expression *)getRight();

    return left->isConstant() & right->isConstant();
}

// Node interface
void BinaryExpression::analyze(bool force) {

    // don't think we should ever call this
    BJOU_DEBUG_ASSERT(false);

    /*
    HANDLE_FORCE();

    std::string & contents = getContents();

    ASTNode * left = getLeft();
    left->analyze();
    const Type * lt = ((Expression *)left)->getType();

    ASTNode * right = getRight();
    right->analyze();
    const Type * rt = ((Expression *)right)->getType();

    BJOU_DEBUG_ASSERT(binary(contents.c_str()) && "operator is not binary");
    BJOU_DEBUG_ASSERT(left && right && "missing operands to binary expression");

    if (!lt->opApplies(contents) || !lt->isValidOperand(rt, contents))
        errorl(getContext(), "Could not match '" + lt->getDemangledName() +
                                 "' with '" + rt->getDemangledName() +
                                 "' using the operator '" + contents + "'.");

    setType(lt->binResultType(rt, contents));

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
    */
}
//

static void emplaceConversion(Expression * expr, const Type * dest_t) {
    if ((dest_t->isRef() && conv(dest_t, expr->getType())) ||
        (expr->getType()->isRef() && conv(dest_t, expr->getType()->unRef())))
        return;

    ASTNode * p = expr->parent;

    AsExpression * conversion = new AsExpression;
    conversion->setContext(expr->getContext());
    conversion->setScope(expr->getScope());
    conversion->setContents("as");

    (*expr->replace)(p, expr, conversion);

    conversion->setLeft(expr);
    conversion->setRight(dest_t->getGenericDeclarator());
    conversion->getRight()->setContext(conversion->getContext());
    conversion->getRight()->setScope(conversion->getScope());

    conversion->analyze(true);
}

static void convertOperands(BinaryExpression * expr, const Type * dest_t) {
    const Type * lt = expr->getLeft()->getType()->unRef();
    const Type * rt = expr->getRight()->getType()->unRef();

    // handled by convertAssignmentOperand()
    if (isAssignmentOp(expr->getContents()))
        return;

    if (!equal(lt, dest_t))
        emplaceConversion((Expression *)expr->getLeft(), dest_t);
    if (!equal(rt, dest_t))
        emplaceConversion((Expression *)expr->getRight(), dest_t);
}

// ~~~~~ AddExpression ~~~~~

AddExpression::AddExpression() {
    contents = "+";

    nodeKind = ADD_EXPRESSION;
}

bool AddExpression::isConstant() { return BinaryExpression::isConstant(); }

Val AddExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalAdd(a, b, getType());
}

// Node interface
void AddExpression::analyze(bool force) {
    HANDLE_FORCE();

    if (opOverload())
        return;

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    if (lt->isInt()) {
        if (rt->isInt() || rt->isFloat()) {
            const Type * dest_t = conv(lt, rt);
            convertOperands(this, dest_t);
            setType(dest_t);
        } else if (rt->isPointer()) {
            setType(rt);
        } else
            goto err;
    } else if (lt->isFloat()) {
        if (rt->isInt() || rt->isFloat()) {
            const Type * dest_t = conv(lt, rt);
            convertOperands(this, dest_t);
            setType(dest_t);
        } else
            goto err;
    } else if (lt->isPointer()) {
        if (rt->isInt()) {
            setType(lt);
        } else
            goto err;
    } else
        goto err;
    goto out;
err:
    errorl(getContext(), "Could not match '" + lt->getDemangledName() +
                             "' with '" + rt->getDemangledName() +
                             "' using the operator '" + contents + "'.");
out:
    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * AddExpression::clone() { return ExpressionClone(this); }

//

// ~~~~~ SubExpression ~~~~~

SubExpression::SubExpression() {
    contents = "-";

    nodeKind = SUB_EXPRESSION;
}

bool SubExpression::isConstant() { return BinaryExpression::isConstant(); }

Val SubExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalSub(a, b, getType());
}

// Node interface
void SubExpression::analyze(bool force) {
    HANDLE_FORCE();

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    if (lt->isInt()) {
        if (rt->isInt() || rt->isFloat()) {
            const Type * dest_t = conv(lt, rt);
            convertOperands(this, dest_t);
            setType(dest_t);
        } else
            goto err;
    } else if (lt->isFloat()) {
        if (rt->isInt() || rt->isFloat()) {
            const Type * dest_t = conv(lt, rt);
            convertOperands(this, dest_t);
            setType(dest_t);
        } else
            goto err;
    } else if (lt->isPointer()) {
        if (rt->isInt()) {
            setType(lt);
        } else
            goto err;
    } else
        goto err;
    goto out;
err:
    errorl(getContext(), "Could not match '" + lt->getDemangledName() +
                             "' with '" + rt->getDemangledName() +
                             "' using the operator '" + contents + "'.");
out:

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * SubExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ MultExpression ~~~~~

MultExpression::MultExpression() {
    contents = "*";

    nodeKind = MULT_EXPRESSION;
}

bool MultExpression::isConstant() { return BinaryExpression::isConstant(); }

Val MultExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalMult(a, b, getType());
}

// Node interface
void MultExpression::analyze(bool force) {
    HANDLE_FORCE();

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    if (lt->isInt()) {
        if (rt->isInt() || rt->isFloat()) {
            const Type * dest_t = conv(lt, rt);
            convertOperands(this, dest_t);
            setType(dest_t);
        } else
            goto err;
    } else if (lt->isFloat()) {
        if (rt->isInt() || rt->isFloat()) {
            const Type * dest_t = conv(lt, rt);
            convertOperands(this, dest_t);
            setType(dest_t);
        } else
            goto err;
    } else
        goto err;
    goto out;
err:
    errorl(getContext(), "Could not match '" + lt->getDemangledName() +
                             "' with '" + rt->getDemangledName() +
                             "' using the operator '" + contents + "'.");
out:

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * MultExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ DivExpression ~~~~~

DivExpression::DivExpression() {
    contents = "/";

    nodeKind = DIV_EXPRESSION;
}

bool DivExpression::isConstant() { return BinaryExpression::isConstant(); }

Val DivExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalDiv(a, b, getType());
}

// Node interface
void DivExpression::analyze(bool force) {
    HANDLE_FORCE();

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    if (lt->isInt()) {
        if (rt->isInt() || rt->isFloat()) {
            const Type * dest_t = conv(lt, rt);
            convertOperands(this, dest_t);
            setType(dest_t);
        } else
            goto err;
    } else if (lt->isFloat()) {
        if (rt->isInt() || rt->isFloat()) {
            const Type * dest_t = conv(lt, rt);
            convertOperands(this, dest_t);
            setType(dest_t);
        } else
            goto err;
    } else
        goto err;
    goto out;
err:
    errorl(getContext(), "Could not match '" + lt->getDemangledName() +
                             "' with '" + rt->getDemangledName() +
                             "' using the operator '" + contents + "'.");
out:

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * DivExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ ModExpression ~~~~~

ModExpression::ModExpression() {
    contents = "%";

    nodeKind = MOD_EXPRESSION;
}

bool ModExpression::isConstant() { return BinaryExpression::isConstant(); }

Val ModExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }

    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalMod(a, b, getType());
}

// Node interface
void ModExpression::analyze(bool force) {
    HANDLE_FORCE();

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    if (lt->isInt()) {
        if (rt->isInt()) {
            const Type * dest_t = conv(lt, rt);
            convertOperands(this, dest_t);
            setType(dest_t);
        } else
            goto err;
    } else
        goto err;
    goto out;
err:
    errorl(getContext(), "Could not match '" + lt->getDemangledName() +
                             "' with '" + rt->getDemangledName() +
                             "' using the operator '" + contents + "'.");
out:

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * ModExpression::clone() { return ExpressionClone(this); }
//

static void convertAssignmentOperand(BinaryExpression * assign) {
    const Type * lt = assign->getLeft()->getType()->unRef();
    const Type * rt = assign->getRight()->getType()->unRef();

    if (!equal(lt, rt) && conv(lt, rt))
        emplaceConversion((Expression *)assign->getRight(), lt);
    /*
    if (lt->isPrimative() && rt->isPrimative())
        if (!equal(lt, rt))
            emplaceConversion((Expression *)assign->getRight(), lt);
    */
}

// ~~~~~ AssignmentExpression ~~~~~

static void AssignmentCommon(BinaryExpression * expr,
                             const Type * result_t = nullptr) {
    std::string & contents = expr->getContents();

    BJOU_DEBUG_ASSERT(binary(contents.c_str()) && "operator is not binary");
    BJOU_DEBUG_ASSERT(expr->getLeft() && expr->getRight() &&
                      "missing operands to binary expression");

    expr->getRight()->analyze();

    // It is important that this part comes before we analyze left.
    // We want to cut in and set the symbol's initialized field before that
    // throws an error in regular analysis.

    // We are going to run analysis on right twice -- once to check the symbol,
    // then again once we have the lval type in place
    if (expr->getLeft()->getFlag(Expression::IDENT)) {
        Maybe<Symbol *> m_sym = expr->getLeft()->getScope()->getSymbol(
            expr->getLeft()->getScope(), (Identifier *)expr->getLeft(),
            &expr->getLeft()->getContext(), true, true, false);
        Symbol * sym = nullptr;
        m_sym.assignTo(sym);
        BJOU_DEBUG_ASSERT(sym && sym->isVar());

        sym->initializedInScopes.insert(expr->getScope());
    }

    if (!((Expression *)expr->getLeft())->canBeLVal())
        errorl(expr->getLeft()->getContext(),
               "Operand left of '" + contents +
                   "' operator must be assignable.");

    const Type * lt = expr->getLeft()->getType()->unRef();
    compilation->frontEnd.lValStack.push(lt);
    const Type * rt = expr->getRight()->getType()->unRef();

    if (!result_t)
        result_t = rt;

    if (!conv(lt, result_t))
        errorl(expr->getContext(), "Can not assign to type '" +
                                       lt->getDemangledName() + "' with '" +
                                       result_t->getDemangledName() + "'.");

    convertAssignmentOperand(expr);

    expr->setType(lt);

    compilation->frontEnd.lValStack.pop();
}

AssignmentExpression::AssignmentExpression() {
    contents = "=";

    nodeKind = ASSIGNMENT_EXPRESSION;
}

bool AssignmentExpression::isConstant() { return false; }

void AssignmentExpression::analyze(bool force) {
    HANDLE_FORCE();

    AssignmentCommon(this);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * AssignmentExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ AddAssignExpression ~~~~~

AddAssignExpression::AddAssignExpression() {
    contents = "+=";

    nodeKind = ADD_ASSIGN_EXPRESSION;
}

bool AddAssignExpression::isConstant() { return false; }

// Node interface
void AddAssignExpression::analyze(bool force) {
    HANDLE_FORCE();

    ((AddExpression *)this)->AddExpression::analyze(force);
    AssignmentCommon(this, type);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * AddAssignExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ SubAssignExpression ~~~~~

SubAssignExpression::SubAssignExpression() {
    contents = "-=";

    nodeKind = SUB_ASSIGN_EXPRESSION;
}

bool SubAssignExpression::isConstant() { return false; }

// Node interface
void SubAssignExpression::analyze(bool force) {
    HANDLE_FORCE();

    ((SubExpression *)this)->SubExpression::analyze(force);
    AssignmentCommon(this, type);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * SubAssignExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ MultAssignExpression ~~~~~

MultAssignExpression::MultAssignExpression() {
    contents = "*=";

    nodeKind = MULT_ASSIGN_EXPRESSION;
}

bool MultAssignExpression::isConstant() { return false; }

// Node interface
void MultAssignExpression::analyze(bool force) {
    HANDLE_FORCE();

    ((MultExpression *)this)->MultExpression::analyze(force);
    AssignmentCommon(this, type);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * MultAssignExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ DivAssignExpression ~~~~~

DivAssignExpression::DivAssignExpression() {
    contents = "/=";

    nodeKind = DIV_ASSIGN_EXPRESSION;
}

bool DivAssignExpression::isConstant() { return false; }

// Node interface
void DivAssignExpression::analyze(bool force) {
    HANDLE_FORCE();

    ((DivExpression *)this)->DivExpression::analyze(force);
    AssignmentCommon(this, type);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * DivAssignExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ ModAssignExpression ~~~~~

ModAssignExpression::ModAssignExpression() {
    contents = "%=";

    nodeKind = MOD_ASSIGN_EXPRESSION;
}

bool ModAssignExpression::isConstant() { return false; }

// Node interface
void ModAssignExpression::analyze(bool force) {
    HANDLE_FORCE();

    ((ModExpression *)this)->ModExpression::analyze(force);
    AssignmentCommon(this, type);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * ModAssignExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ MaybeAssignExpression ~~~~~

MaybeAssignExpression::MaybeAssignExpression() {
    nodeKind = MAYBE_ASSIGN_EXPRESSION;
}

bool MaybeAssignExpression::isConstant() { return false; }

// Node interface
void MaybeAssignExpression::analyze(bool force) {
    HANDLE_FORCE();

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * MaybeAssignExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ LssExpression ~~~~~

LssExpression::LssExpression() {
    contents = "<";

    nodeKind = LSS_EXPRESSION;
}

bool LssExpression::isConstant() { return BinaryExpression::isConstant(); }

static void comparisonSignWarn(Context & context, const Type * lt,
                               const Type * rt) {
    // @temporary this was just getting annoying
    return;

#define SIGN_STR(t) (t->sign == Type::Sign::SIGNED ? "signed" : "unsigned")

    if (lt->isInt() && rt->isInt()) {
        const IntType * ilt = (const IntType *)lt;
        const IntType * irt = (const IntType *)rt;
        if (ilt->sign != irt->sign)
            warningl(context, "Comparing types with differing signs.",
                     SIGN_STR(ilt) + std::string(" versus ") + SIGN_STR(irt));
    }
}

// Node interface
void LssExpression::analyze(bool force) {
    HANDLE_FORCE();

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    comparisonSignWarn(getContext(), lt, rt);

    if (lt->isInt()) {
        if (rt->isInt() || rt->isFloat() || rt->isChar()) {
            const Type * dest_t = conv(lt, rt);
            convertOperands(this, dest_t);
        } else
            goto err;
    } else if (lt->isFloat()) {
        if (rt->isInt() || rt->isFloat()) {
            const Type * dest_t = conv(lt, rt);
            convertOperands(this, dest_t);
        } else
            goto err;
    } else if (lt->isChar()) {
        if (rt->isChar() || rt->isInt()) {
            const Type * dest_t = conv(lt, rt);
            convertOperands(this, dest_t);
        } else
            goto err;
    } else if (lt->isPointer()) {
        if (rt->isPointer()) {
            // setType(lt);
        } else
            goto err;
    } else
        goto err;
    goto out;
err:
    errorl(getContext(), "Could not match '" + lt->getDemangledName() +
                             "' with '" + rt->getDemangledName() +
                             "' using the operator '" + contents + "'.");
out:

    setType(BoolType::get());

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * LssExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ LeqExpression ~~~~~

LeqExpression::LeqExpression() {
    contents = "<=";

    nodeKind = LEQ_EXPRESSION;
}

bool LeqExpression::isConstant() { return BinaryExpression::isConstant(); }

// Node interface
void LeqExpression::analyze(bool force) {
    HANDLE_FORCE();

    ((LssExpression *)this)->LssExpression::analyze(force);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * LeqExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ GtrExpression ~~~~~

GtrExpression::GtrExpression() {
    contents = ">";

    nodeKind = GTR_EXPRESSION;
}

bool GtrExpression::isConstant() { return BinaryExpression::isConstant(); }

// Node interface
void GtrExpression::analyze(bool force) {
    HANDLE_FORCE();

    ((LssExpression *)this)->LssExpression::analyze(force);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * GtrExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ GeqExpression ~~~~~

GeqExpression::GeqExpression() {
    contents = ">=";

    nodeKind = GEQ_EXPRESSION;
}

bool GeqExpression::isConstant() { return BinaryExpression::isConstant(); }

// Node interface
void GeqExpression::analyze(bool force) {
    HANDLE_FORCE();

    ((LssExpression *)this)->LssExpression::analyze(force);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * GeqExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ EquExpression ~~~~~

EquExpression::EquExpression() {
    contents = "==";

    nodeKind = EQU_EXPRESSION;
}

bool EquExpression::isConstant() { return BinaryExpression::isConstant(); }

Val EquExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalEqu(a, b, getType());
}

// Node interface
void EquExpression::analyze(bool force) {
    HANDLE_FORCE();

    ((LssExpression *)this)->LssExpression::analyze(force);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * EquExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ NeqExpression ~~~~~

NeqExpression::NeqExpression() {
    contents = "!=";

    nodeKind = NEQ_EXPRESSION;
}

bool NeqExpression::isConstant() { return BinaryExpression::isConstant(); }

Val NeqExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalNeq(a, b, getType());
}

// Node interface
void NeqExpression::analyze(bool force) {
    HANDLE_FORCE();

    ((LssExpression *)this)->LssExpression::analyze(force);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * NeqExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ LogAndExpression ~~~~~

LogAndExpression::LogAndExpression() {
    contents = "and";

    nodeKind = LOG_AND_EXPRESSION;
}

bool LogAndExpression::isConstant() { return BinaryExpression::isConstant(); }

// Node interface
void LogAndExpression::analyze(bool force) {
    HANDLE_FORCE();

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();
    const Type * _bool = BoolType::get();

    ASTNode * badop = nullptr;

    if (!equal(lt, _bool))
        badop = getLeft();
    if (!equal(rt, _bool))
        badop = getRight();

    if (badop)
        errorl(badop->getContext(),
               "Operands of logical operator '" + getContents() +
                   "' must be of type 'bool'.",
               true, "got '" + badop->getType()->getDemangledName() + "'");

    setType(_bool);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * LogAndExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ LogOrExpression ~~~~~

LogOrExpression::LogOrExpression() {
    contents = "or";

    nodeKind = LOG_OR_EXPRESSION;
}

bool LogOrExpression::isConstant() { return BinaryExpression::isConstant(); }

// Node interface
void LogOrExpression::analyze(bool force) {
    HANDLE_FORCE();

    ((LogAndExpression *)this)->LogAndExpression::analyze(force);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * LogOrExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ SubscriptExpression ~~~~~

SubscriptExpression::SubscriptExpression() {
    contents = "[]";

    nodeKind = SUBSCRIPT_EXPRESSION;
}

// @temporary
// I guess in some circumstances, subscript could be made to be constant
bool SubscriptExpression::isConstant() { return false; }

// Node interface
void SubscriptExpression::analyze(bool force) {
    HANDLE_FORCE();

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    if (!lt->isPointer() && !lt->isArray() && !lt->isSlice() &&
        !lt->isDynamicArray())
        errorl(getContext(), "Could not match '" + lt->getDemangledName() +
                                 "' with subscript operator.");

    if (!rt->isInt())
        errorl(getRight()->getContext(),
               "Operand right of subscript operator must be an integer type.");

    if (lt->isPointer() || lt->isSlice() || lt->isDynamicArray())
        setType(lt->under());
    else if (lt->isArray()) {
        getRight()->analyze();
        if (getLeft()->nodeKind == ASTNode::INITIALZER_LIST)
            if (!((Expression *)getRight())->isConstant())
                errorl(getRight()->getContext(),
                       "Cannot index into constant array "
                       "initializer list with non-constant "
                       "index.");

        if (((Expression *)getRight())->isConstant()) {
            ArrayType * at = (ArrayType *)lt;
            Val rv = ((Expression *)getRight())->eval();
            if (rv.as_i64 >= at->width)
                errorl(
                    getRight()->getContext(), "Array index is out of bounds.",
                    true,
                    "array contains " + std::to_string(at->width) + " elements",
                    "attempting to index element " + std::to_string(rv.as_i64));
        }

        setType(lt->under());
    }

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");

    setFlag(ANALYZED, true);

    desugar();
}

ASTNode * SubscriptExpression::clone() { return ExpressionClone(this); }

void SubscriptExpression::desugar() {
    if (getLeft()->getType()->unRef()->isSlice()) {
        CallExpression * call = new CallExpression;
        call->setContext(getContext());

        // __bjou_slice_subscript
        Identifier * __bjou_slice_subscript = new Identifier;
        __bjou_slice_subscript->setUnqualified("__bjou_slice_subscript");

        ArgList * r = new ArgList;
        // (slice, index)
        r->addExpression(getLeft());
        r->addExpression(getRight());

        if (compilation->frontEnd.abc) {
            // ... filename, line, col)
            StringLiteral * f = new StringLiteral;
            f->setContents(getRight()->getContext().filename);
            IntegerLiteral * l = new IntegerLiteral;
            l->setContents(std::to_string(getRight()->getContext().begin.line));
            IntegerLiteral * c = new IntegerLiteral;
            c->setContents(
                std::to_string(getRight()->getContext().begin.character));

            r->addExpression(f);
            r->addExpression(l);
            r->addExpression(c);
        }

        call->setLeft(__bjou_slice_subscript);
        call->setRight(r);

        call->addSymbols(getScope());

        (*replace)(parent, this, call);

        call->analyze();
    } else if (getLeft()->getType()->unRef()->isDynamicArray()) {
        CallExpression * call = new CallExpression;
        call->setContext(getContext());

        // __bjou_dynamic_array_subscript
        Identifier * __bjou_dynamic_array_subscript = new Identifier;
        __bjou_dynamic_array_subscript->setUnqualified(
            "__bjou_dynamic_array_subscript");

        ArgList * r = new ArgList;
        // (dynamic_array, index)
        r->addExpression(getLeft());
        r->addExpression(getRight());

        if (compilation->frontEnd.abc) {
            // ... filename, line, col)
            StringLiteral * f = new StringLiteral;
            f->setContents(getRight()->getContext().filename);
            IntegerLiteral * l = new IntegerLiteral;
            l->setContents(std::to_string(getRight()->getContext().begin.line));
            IntegerLiteral * c = new IntegerLiteral;
            c->setContents(
                std::to_string(getRight()->getContext().begin.character));

            r->addExpression(f);
            r->addExpression(l);
            r->addExpression(c);
        }

        call->setLeft(__bjou_dynamic_array_subscript);
        call->setRight(r);

        call->addSymbols(getScope());

        (*replace)(parent, this, call);

        call->analyze();
    } else if (getLeft()->getType()->unRef()->isArray() &&
               compilation->frontEnd.abc) {
        CallExpression * call = new CallExpression;
        call->setContext(getContext());

        // __bjou_array_subscript
        Identifier * __bjou_array_subscript = new Identifier;
        __bjou_array_subscript->setUnqualified("__bjou_array_subscript");

        ArgList * r = new ArgList;
        // (array, len, index, filename, line, col)
        r->addExpression(getLeft());
        IntegerLiteral * len = new IntegerLiteral;
        len->setContents(
            std::to_string(((ArrayType *)getLeft()->getType())->width));
        r->addExpression(len);
        r->addExpression(getRight());

        StringLiteral * f = new StringLiteral;
        f->setContents(getRight()->getContext().filename);
        IntegerLiteral * l = new IntegerLiteral;
        l->setContents(std::to_string(getRight()->getContext().begin.line));
        IntegerLiteral * c = new IntegerLiteral;
        c->setContents(
            std::to_string(getRight()->getContext().begin.character));

        r->addExpression(f);
        r->addExpression(l);
        r->addExpression(c);

        call->setLeft(__bjou_array_subscript);
        call->setRight(r);

        call->addSymbols(getScope());

        (*replace)(parent, this, call);

        call->analyze();
    }
}

//

// ~~~~~ CallExpression ~~~~~

CallExpression::CallExpression() : resolved_proc(nullptr) {
    contents = "()";
    nodeKind = CALL_EXPRESSION;
}

bool CallExpression::isConstant() { return false; }

void CallExpression::analyze(bool force) {
    HANDLE_FORCE();

    ProcSet * procSet = nullptr;
    Procedure * proc = nullptr;
    const Type * lt = nullptr;

    getRight()->analyze(force);

    ASTNode * old_left = getLeft();
    bool tryufc = old_left->nodeKind == ASTNode::ACCESS_EXPRESSION;
    getLeft()->analyze(force);

    if (tryufc && old_left->getFlag(AccessExpression::UFC) &&
        getLeft()->nodeKind == ASTNode::IDENTIFIER) {
        return;
    }

    ArgList * args = (ArgList *)getRight();
    Expression * l = (Expression *)getLeft();

    if (l->getFlag(Expression::IDENT)) {
        Maybe<Symbol *> m_sym =
            getScope()->getSymbol(getScope(), getLeft(), &l->getContext());
        Symbol * sym = nullptr;
        m_sym.assignTo(sym);
        BJOU_DEBUG_ASSERT(sym);
        if (sym->node()->nodeKind == PROC_SET) {
            procSet = (ProcSet *)sym->node();
            proc = procSet->get(args, l->getRight(), &args->getContext());
            ((Identifier *)getLeft())->qualified = proc->getMangledName();
            if (l->getRight())      // TemplateInstantiation
                l->right = nullptr; // l->setRight(nullptr);
            lt = proc->getType();
        } else {
            lt = getLeft()->getType()->unRef();
        }
        l->setType(lt);
    } else {
        lt = getLeft()->getType()->unRef();
    }

    if (proc) {
        if (getLeft()->nodeKind == ASTNode::IDENTIFIER)
            ((Identifier *)getLeft())->resolved = proc;

        resolved_proc = proc;
        if (proc->parent &&
            proc->parent->nodeKind == ASTNode::INTERFACE_IMPLEMENTATION) {
            Struct * s = proc->getParentStruct();
            BJOU_DEBUG_ASSERT(s);
            if (s->getFlag(Struct::IS_ABSTRACT))
                setFlag(CallExpression::INTERFACE_CALL, true);
        }
    }

    BJOU_DEBUG_ASSERT(lt);
    if (!lt->isProcedure())
        errorl(getLeft()->getContext(),
               "Expression is not a procedure, but is being called like one.");

    ProcedureType * plt = (ProcedureType *)lt;

    int nargs = (int)args->getExpressions().size();
    int nexpected = (int)plt->paramTypes.size();

    bool arg_err = false;
    bool l_val_err = false;
    Context errContext;

    if (nargs > nexpected && !plt->isVararg) {
        arg_err = true;
        errContext = args->getExpressions()[nexpected]->getContext();
    } else if (nargs < nexpected) {
        arg_err = true;
        errContext = args->getContext();
    }

    if (!arg_err) {
        for (int i = 0; i < nexpected; i += 1) {
            const Type * expected_t = plt->paramTypes[i];
            const Type * arg_t = args->getExpressions()[i]->getType();

            if (expected_t->isRef() &&
                !((Expression *)args->getExpressions()[i])->canBeLVal()) {
                arg_err = true;
                l_val_err = true;
                errContext = args->getExpressions()[i]->getContext();
                break;
            }

            if (!conv(expected_t, arg_t)) {
                arg_err = true;
                errContext = args->getExpressions()[i]->getContext();
                break;
            }

            if (!equal(expected_t, arg_t))
                emplaceConversion((Expression *)args->getExpressions()[i],
                                  expected_t);
        }
    }

    if (arg_err) {
        std::string passedTypes;
        for (ASTNode *& expr : args->getExpressions()) {
            passedTypes += expr->getType()->getDemangledName();
            if (&expr != &args->getExpressions().back())
                passedTypes += ", ";
        }

        std::vector<std::string> help = {
            "Note: procedure type: " + plt->getDemangledName(),
            "arguments passed were (" + passedTypes + ")"};

        if (l_val_err)
            help.insert(help.begin(),
                        "expression can't be used as a reference");

        if (proc)
            errorl(errContext,
                   "No matching call for '" +
                       demangledString(
                           mangledIdentifier((Identifier *)getLeft())) +
                       "' found.",
                   true, help);
        else
            errorl(errContext, "No matching call to indirect procedure.", true,
                   help);
    }

    setType(plt->retType);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * CallExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ AccessExpression ~~~~~

AccessExpression::AccessExpression() {
    contents = ".";

    nodeKind = ACCESS_EXPRESSION;
    injection = nullptr;
}

bool AccessExpression::isConstant() { return false; }

// T.ident
static Identifier * createIdentifierFromAccess(AccessExpression * access,
                                               Declarator * decl,
                                               Identifier * ident) {
    const Type * t = decl->getType();
    StructType * struct_t = nullptr;

    if (t->isStruct()) {
        struct_t = (StructType *)t;
    } else if (t->isPointer() || t->isRef()) {
        if (t->under()->isStruct())
            struct_t = (StructType *)t->under();
    }

    BJOU_DEBUG_ASSERT(struct_t);

    Identifier * proc_ident = new Identifier;
    proc_ident->setContext(access->getContext());
    proc_ident->setScope(access->getScope());
    std::string lookup =
        struct_t->_struct->getMangledName() + "." + ident->getUnqualified();
    proc_ident->setUnqualified(lookup);
    proc_ident->addSymbols(access->getScope());

    proc_ident->setFlag(ASTNode::CT, isCT(access));

    proc_ident->analyze();

    return proc_ident;
}

// T.interface.ident
static Identifier * createIdentifierFromAccess(AccessExpression * access,
                                               Declarator * decl,
                                               Identifier * iface_ident,
                                               Identifier * ident) {
    const Type * t = decl->getType()->unRef();
    StructType * struct_t = nullptr;

    if (t->isStruct()) {
        struct_t = (StructType *)t;
    } else if (t->isPointer() || t->isRef()) {
        if (t->under()->isStruct())
            struct_t = (StructType *)t->under();
    }

    BJOU_DEBUG_ASSERT(struct_t);

    Identifier * proc_ident = new Identifier;
    proc_ident->setContext(access->getContext());
    proc_ident->setScope(access->getScope());
    std::string lookup = struct_t->_struct->getMangledName() + "." +
                         mangledIdentifier(iface_ident) + "." +
                         ident->getUnqualified();
    proc_ident->setUnqualified(lookup);
    proc_ident->addSymbols(access->getScope());

    proc_ident->setFlag(ASTNode::CT, isCT(access));

    proc_ident->analyze();

    return proc_ident;
}

// T.interface.ident
static Identifier * createIdentifierFromAccess(AccessExpression * access,
                                               Identifier * obj_ident,
                                               Identifier * iface_ident,
                                               Identifier * ident) {
    const Type * t = obj_ident->getType()->unRef();
    StructType * struct_t = nullptr;

    if (t->isStruct()) {
        struct_t = (StructType *)t;
    } else if (t->isPointer()) {
        if (t->under()->isStruct())
            struct_t = (StructType *)t->under();
    }

    BJOU_DEBUG_ASSERT(struct_t);

    Identifier * proc_ident = new Identifier;
    proc_ident->setContext(access->getContext());
    proc_ident->setScope(access->getScope());
    std::string lookup = struct_t->_struct->getMangledName() + "." +
                         mangledIdentifier(iface_ident) + "." +
                         ident->getUnqualified();
    proc_ident->setUnqualified(lookup);
    proc_ident->addSymbols(access->getScope());

    proc_ident->analyze();

    return proc_ident;
}

CallExpression * AccessExpression::nextCall() {
    if (parent && parent->nodeKind == CALL_EXPRESSION)
        return (CallExpression *)parent;
    return nullptr;
}

int AccessExpression::handleInterfaceSpecificCall() {
    // Special case
    //
    // We want to support a transformation from T.interface.proc to the correct
    // mangled ProcSet Identifier. However, the tree is set up a little
    // awkwardly for that. ((T.interface).proc). So we are going to scan the LHS
    // and see if it matches the pattern T.interface.

    if (getLeft()->nodeKind == ACCESS_EXPRESSION &&
        getRight()->nodeKind == IDENTIFIER) {
        Identifier * r_id = (Identifier *)getRight();
        AccessExpression * l = (AccessExpression *)getLeft();
        // only base declarators
        if (l->getRight()->nodeKind == IDENTIFIER) {
            Identifier * interface_ident = (Identifier *)l->getRight();
            const Type * t = l->getLeft()->getType()->unRef();
            StructType * struct_t = nullptr;

            if (t->isStruct()) {
                struct_t = (StructType *)t;
            } else if (t->isPointer()) {
                if (t->under()->isStruct())
                    struct_t = (StructType *)t->under();
            }

            BJOU_DEBUG_ASSERT(struct_t);

            if (struct_t->interfaces.find(mangledIdentifier(interface_ident)) !=
                struct_t->interfaces.end()) {
                if (l->getLeft()->nodeKind == DECLARATOR) {
                    Declarator * decl = (Declarator *)l->getLeft();
                    Identifier * proc_ident = createIdentifierFromAccess(
                        this, decl, interface_ident, r_id);
                    (*this->replace)(parent, this, proc_ident);
                    setType(proc_ident->type);
                    return -1;
                } else if (l->getLeft()->nodeKind == IDENTIFIER) {
                    Identifier * obj_ident = (Identifier *)l->getLeft();
                    Identifier * proc_ident = createIdentifierFromAccess(
                        this, obj_ident, interface_ident, r_id);

                    if (nextCall()) {
                        (*getRight()->replace)(this, getRight(),
                                               proc_ident); // @lol this works
                        (*l->replace)(this, l, l->getLeft());
                        // don't return.. fall to check handleInjection()
                    } else {
                        (*this->replace)(parent, this, proc_ident);
                        setType(proc_ident->type);
                        if (!type)
                            proc_ident->getType(); // identifier error
                        return -1;
                    }
                }
            }
        }
    }

    // end special case
    return 0;
}

int AccessExpression::handleThroughTemplate() {
    // Special case
    // Consider:
    //
    //          type Type!(T) {
    //              proc create(arg : T) : Type!(T) { ... }
    //          }
    //
    // We would like to be able to say Type.create(12345) which would treat
    // Type.create() as
    //
    //          proc create!(T)(arg : T) : Type!(T) { ... }
    //
    // thus using template deduction from arguments to return the correct type
    // Type!(int).
    //
    // To accomplish this, we will take the procedure from Type and wrap it in
    // its own TemplateProc and transfer the TemplateDefineList from Type to the
    // new TemplateProc. Then all we must do is instantiate the proc and replace
    // this node with a reference to it.

    Expression * next_call = nextCall();

    // Only base declarators.
    if (next_call && getLeft()->nodeKind == ASTNode::DECLARATOR &&
        !((Declarator *)getLeft())->templateInst &&
        getRight()->nodeKind == ASTNode::IDENTIFIER) {
        Declarator * decl = (Declarator *)getLeft();
        Identifier * r_id = (Identifier *)getRight();

        // Taken from Declarator::analyze()
        std::string mangled = decl->mangleSymbol();
        Maybe<Symbol *> m_sym = decl->getScope()->getSymbol(
            decl->getScope(), mangled, &decl->getContext(),
            /* traverse = */ true, /* fail = */ false);
        Symbol * sym = nullptr;

        if (m_sym.assignTo(sym)) {
            if (sym->isTemplateType()) {
                TemplateStruct * ttype = (TemplateStruct *)sym->node();
                Struct * s = (Struct *)ttype->_template;
                s->setMangledName(s->getName());
                ProcSet set;
                std::vector<std::string> delete_keys;
                set.name = r_id->getUnqualified();

                for (ASTNode * _proc : s->getMemberProcs()) {
                    Procedure * proc = (Procedure *)_proc;
                    proc->setScope(ttype->getScope());
                    if (proc->getName() == set.name) {
                        TemplateProc * tproc = new TemplateProc;
                        tproc->setFlag(TemplateProc::FROM_THROUGH_TEMPLATE,
                                       true);
                        tproc->setFlag(TemplateProc::IS_TYPE_MEMBER, true);
                        tproc->parent = s;
                        tproc->setScope(proc->getScope());
                        tproc->setContext(proc->getContext());
                        tproc->setNameContext(proc->getNameContext());
                        tproc->setTemplateDef(ttype->getTemplateDef()->clone());
                        tproc->setTemplate(proc);

                        proc->desugarThis();

                        _Symbol<TemplateProc> * symbol =
                            new _Symbol<TemplateProc>(set.name, tproc);
                        set.procs[symbol->mangled(tproc->getScope())->name] =
                            symbol;
                    }
                }

                for (ASTNode * _tproc : s->getMemberTemplateProcs()) {
                    TemplateProc * tproc = (TemplateProc *)_tproc;
                    Procedure * proc = (Procedure *)tproc->_template;
                    proc->setScope(ttype->getScope());

                    if (proc->getName() == set.name) {
                        TemplateProc * new_tproc =
                            (TemplateProc *)tproc->clone();
                        new_tproc->setFlag(TemplateProc::FROM_THROUGH_TEMPLATE,
                                           true);
                        new_tproc->setScope(proc->getScope());
                        new_tproc->setContext(proc->getContext());
                        new_tproc->setNameContext(proc->getNameContext());

                        TemplateDefineList * new_tproc_def =
                            (TemplateDefineList *)new_tproc->getTemplateDef();
                        std::vector<ASTNode *> save_elems =
                            new_tproc_def->getElements();
                        new_tproc_def->getElements().clear();
                        for (ASTNode * elem :
                             ((TemplateDefineList *)ttype->getTemplateDef())
                                 ->getElements())
                            new_tproc_def->addElement(elem);
                        for (ASTNode * elem : save_elems)
                            new_tproc_def->addElement(elem);

                        new_tproc->setTemplate(proc);

                        proc->desugarThis();

                        _Symbol<TemplateProc> * symbol =
                            new _Symbol<TemplateProc>(set.name, tproc);
                        std::string key =
                            symbol->mangled(tproc->getScope())->name;
                        set.procs[key] = symbol;
                        delete_keys.push_back(key);
                    }
                }

                if (set.procs.empty())
                    errorl(
                        r_id->getContext(),
                        "Template type '" +
                            ((Identifier *)decl->identifier)->getUnqualified() +
                            "' does not define a procedure named '" +
                            r_id->getUnqualified() + "'.");

                Procedure * proc =
                    set.get(next_call->getRight(), r_id->getRight(),
                            &getContext(), true);

                Identifier * proc_ident = new Identifier;
                proc_ident->setContext(getContext());
                proc_ident->setScope(getScope());
                proc_ident->setUnqualified(proc->getMangledName());
                proc_ident->addSymbols(getScope());

                proc_ident->analyze();

                (*this->replace)(parent, this, proc_ident);

                // cleanup
                for (auto & key : delete_keys)
                    delete set.procs[key]->node();
                for (auto & p : set.procs)
                    delete p.second;

                return -1;
            }
        }
    }

    // end special case
    return 0;
}

int AccessExpression::handleAccessThroughDeclarator(bool force) {
    const Type * lt = getLeft()->getType();

    Identifier * r_id = (Identifier *)getRight();

    if (IS_DECLARATOR(getLeft())) {
        getLeft()->analyze(force);
        lt = getLeft()->getType();
        if (lt->isStruct()) {
            StructType * struct_t =
                (StructType *)lt; // can't have constness here
            Maybe<Symbol *> m_interface_sym;
            Symbol * interface_sym;
            if (getRight()->nodeKind == ASTNode::IDENTIFIER) {
                if (struct_t->constantMap.count(r_id->getUnqualified()) > 0) {
                    if (nodeKind == INJECT_EXPRESSION)
                        errorl(getContext(), "To access type data members, use "
                                             "'.' instead of '->'.");

                    Expression * const_expr =
                        (Expression *)struct_t
                            ->constantMap[r_id->getUnqualified()]
                            ->getInitialization();
                    const_expr = (Expression *)const_expr->clone();
                    const_expr->setScope(getScope());
                    const_expr->setContext(getContext());
                    (*this->replace)(parent, this, const_expr);
                    setType(const_expr->getType());
                    return -1;
                } else if (struct_t->memberProcs.count(r_id->getUnqualified()) >
                           0) {
                    Identifier * proc_ident = createIdentifierFromAccess(
                        this, (Declarator *)getLeft(), r_id);
                    (*this->replace)(parent, this, proc_ident);
                    setType(proc_ident->type);
                    if (!type && !nextCall())
                        proc_ident->getType(); // identifier error
                    return -1;
                } else if ((m_interface_sym =
                                getScope()->getSymbol(getScope(), r_id)) &&
                           m_interface_sym.assignTo(interface_sym) &&
                           interface_sym->isInterface()) {

                    Identifier * proc_ident = createIdentifierFromAccess(
                        this, (Declarator *)getLeft(), r_id);
                    (*this->replace)(parent, this, proc_ident);
                    setType(proc_ident->type);
                    return -1;
                } else {
                    if (struct_t->memberIndices.count(r_id->getUnqualified()) >
                        0)
                        errorl(getRight()->getContext(),
                               "Type '" + lt->getDemangledName() +
                                   "' does not define a constant named '" +
                                   r_id->getUnqualified() + "'.",
                               true,
                               "'" + r_id->getUnqualified() +
                                   "' is a member variable and can only be "
                                   "accessed by an instance of '" +
                                   lt->getDemangledName() + "'");
                    else
                        errorl(getRight()->getContext(),
                               "Type '" + lt->getDemangledName() +
                                   "' does not define a constant named '" +
                                   r_id->getUnqualified() + "'.");
                }
            } else
                errorl(getRight()->getContext(), "Expected identifier.");
        } else
            errorl(getLeft()->getContext(),
                   "'" + lt->getDemangledName() + "' does not have members.");
    }

    return 0;
}

int AccessExpression::handleContainerAccess() {
    const Type * lt = getLeft()->getType()->unRef();
    Identifier * r_id = (Identifier *)getRight();
    Expression * next_call = nextCall();

    // check for regular access
    if (lt->isStruct() || (lt->isPointer() && lt->under()->isStruct())) {

        StructType * struct_t = (StructType *)lt; // can't have constness here
        if (lt->isPointer())
            struct_t = (StructType *)lt->under();

        if (getRight()->nodeKind == ASTNode::IDENTIFIER) {
            // don't analyze here else we probably get 'use of undeclared
            // identifier' r_id->analyze(force);
            if (struct_t->memberIndices.count(r_id->getUnqualified()) > 0) {
                if (nodeKind == INJECT_EXPRESSION)
                    errorl(getContext(), "To access type data members, use '.' "
                                         "instead of '->'.");
                setType(struct_t->memberTypes
                            [struct_t->memberIndices[r_id->getUnqualified()]]);
                return 1;
            } else if (struct_t->constantMap.count(r_id->getUnqualified()) >
                       0) {
                if (nodeKind == INJECT_EXPRESSION)
                    errorl(getContext(), "To access type data members, use '.' "
                                         "instead of '->'.");
                setType(
                    struct_t->constantMap[r_id->getUnqualified()]->getType());
                return 1;
            } else if (struct_t->memberProcs.count(r_id->getUnqualified()) >
                       0) {
                Identifier * proc_ident = createIdentifierFromAccess(
                    this, (Declarator *)getLeft(), r_id);
                (*getRight()->replace)(this, getRight(),
                                       proc_ident); // @lol this works
                // don't return.. fall to check handleInjection()
                return 1;
            } else if (!next_call)
                errorl(getRight()->getContext(),
                       "No member named '" + r_id->getUnqualified() + "' in '" +
                           struct_t->getDemangledName() + "'.");
        } else if (next_call) {
            // @incomplete interface stuff
        } else
            errorl(getRight()->getContext(), "Invalid structure accessor.",
                   true, "expected member name");
    } else if (lt->isTuple() || (lt->isPointer() && lt->under()->isTuple())) {
        TupleType * tuple_t = (TupleType *)lt;

        if (nodeKind == INJECT_EXPRESSION)
            errorl(getContext(),
                   "To access tuple elements, use '.' instead of '->'.");

        if (lt->isPointer())
            tuple_t = (TupleType *)lt->under();

        if (getRight()->nodeKind == ASTNode::INTEGER_LITERAL) {
            int elem = (int)((Expression *)getRight())->eval().as_i64;
            if (elem < 0)
                errorl(getRight()->getContext(),
                       "Tuple element access must be non-negative.");
            int nsubtypes = (int)tuple_t->types.size();
            if (elem >= nsubtypes)
                errorl(getRight()->getContext(),
                       "Attempting to access tuple element " +
                           std::to_string(elem) + " in tuple that has " +
                           std::to_string(nsubtypes) + " elements.",
                       true,
                       "tuple type: '" + tuple_t->getDemangledName() + "'",
                       "Note: Tuple element access begins at 0");

            setType(tuple_t->types[elem]);
            return 1;
        } else if (!next_call) {
            if (getRight()->nodeKind == ASTNode::FLOAT_LITERAL)
                errorl(getRight()->getContext(),
                       "Invalid tuple element access.", true,
                       "Note: try putting the first tuple element access in "
                       "parentheses");
            else
                errorl(getRight()->getContext(),
                       "Invalid tuple element access.");
        }
    }

    return 0;
}

bool AccessExpression::handleInjection() {
    Expression * next_call = nextCall();
    if (next_call) {
        // injection
        // we give away ownership of both left and right nodes

        if (nodeKind == INJECT_EXPRESSION) {
            if (getLeft()->getFlag(IDENT)) {
                Maybe<Symbol *> m_sym =
                    getScope()->getSymbol(getScope(), (Identifier *)getLeft(),
                                          &getLeft()->getContext());
                Symbol * sym = nullptr;
                m_sym.assignTo(sym);
                BJOU_DEBUG_ASSERT(sym);
                if (sym->isType())
                    errorl(getLeft()->getContext(),
                           "Operand left of '->' operator must be addressable.",
                           true, "Cannot take the address of a type.");
                else if (sym->isProcSet())
                    errorl(getLeft()->getContext(),
                           "Operand left of '->' operator must be addressable.",
                           true,
                           "Use of '" + sym->demangledString() +
                               "' is ambiguous."); // @bad
                else if (sym->isInterface())
                    errorl(getLeft()->getContext(),
                           "Operand left of '->' operator must be addressable.",
                           true, "Cannot take the address of an interface.");
                else if (sym->node()->getFlag(ASTNode::IS_TEMPLATE))
                    errorl(getLeft()->getContext(),
                           "Operand left of '->' operator must be addressable.",
                           true,
                           "Cannot take the address of a template definition.");
            } else {
                if (getLeft()->getFlag(TERMINAL) ||
                    !((Expression *)getLeft())->canBeLVal())
                    errorl(
                        getLeft()->getContext(),
                        "Operand left of '->' operator must be addressable.");
            }

            AddressExpression * a = new AddressExpression;
            a->setScope(getLeft()->getScope());
            a->setContext(getLeft()->getContext());
            ASTNode * l = getLeft();
            (*getLeft()->replace)(this, getLeft(), a);
            a->setRight(l);
        }

        ArgList * args = (ArgList *)next_call->getRight();
        args->addExpressionToFront(getLeft());
        // args->getExpressions().insert(args->getExpressions().begin(),
        // getLeft());
        next_call->setLeft(getRight());
        next_call->analyze();
        injection = (CallExpression *)next_call;
        setType(getRight()->getType());
        // delete this; // @refactor? @leak?
        setFlag(UFC, true);
        return true;
    }
    return false;
}

void AccessExpression::analyze(bool force) {
    HANDLE_FORCE();

    // These functions return either 0, 1, or -1
    //  0:  DID NOT apply, continue
    //  1:  DID apply, skip other cases and continue to handleInjection()
    // -1:  DID apply and made a replacement, return immediately

    int i = 0;

    if ((i = handleInterfaceSpecificCall())) {
        if (i == -1) {
            setFlag(ANALYZED, true);
            return;
        }
    } else if (getLeft()->analyze(force), (i = handleThroughTemplate())) {
        if (i == -1) {
            setFlag(ANALYZED, true);
            return;
        }
    } else if ((i = handleAccessThroughDeclarator(force))) {
        if (i == -1) {
            setFlag(ANALYZED, true);
            return;
        }
    } else if ((i = handleContainerAccess())) {
        if (i == -1) {
            setFlag(ANALYZED, true);
            return;
        }
    }

    if (!type) {
        if (handleInjection()) {
            setFlag(ANALYZED, true);
            return;
        } else
            errorl(getContext(),
                   "Access using the '.' operator only applies to struct types "
                   "and pointers to them.",
                   true,
                   "attempting to use '.' on '" +
                       getLeft()->getType()->getDemangledName() + "'");
    }

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * AccessExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ InjectExpression ~~~~~

InjectExpression::InjectExpression() {
    contents = "->";

    nodeKind = INJECT_EXPRESSION;
}

bool InjectExpression::isConstant() { return false; }

void InjectExpression::analyze(bool force) {
    HANDLE_FORCE();

    ((AccessExpression *)this)->AccessExpression::analyze();

    // InjectExpressions are always replaced
    // BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * InjectExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ UnaryPreExpression ~~~~~

UnaryPreExpression::UnaryPreExpression() { nodeKind = UNARY_PRE_EXPRESSION; }

bool UnaryPreExpression::isConstant() {
    analyze();
    return ((Expression *)getRight())->isConstant();
}

void UnaryPreExpression::analyze(bool force) {
    HANDLE_FORCE();

    // should never be called
    BJOU_DEBUG_ASSERT(false);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

// ~~~~~ NewExpression ~~~~~

NewExpression::NewExpression() {
    contents = "new";

    nodeKind = NEW_EXPRESSION;
}

bool NewExpression::isConstant() { return false; }

void NewExpression::analyze(bool force) {
    HANDLE_FORCE();

    BJOU_DEBUG_ASSERT(IS_DECLARATOR(getRight()));
    // we have given the resposibility of everything below to the backend

    /*
    // we want to create an expression that looks like:
    //          malloc(sizeof [decl])
    // or       malloc(n * sizeof [decl])

    Declarator * save_decl = (Declarator*)getRight();

    // ()
    CallExpression * l = new CallExpression;
    l->setScope(getScope());
    l->setContext(getContext());
    l->setContents("()");

    // malloc()
    Identifier * i = new Identifier;
    i->setScope(getScope());
    i->setUnqualified("malloc");

    l->setLeft(i);

    ArgList * a = new ArgList;
    a->setScope(getScope());
    a->setContext(getContext());

    l->setRight(a);

    // sizeof
    Expression * size_expr = new SizeofExpression;
    size_expr->setScope(getScope());
    size_expr->setContext(getContext());
    size_expr->setContents("sizeof");

    if (save_decl->nodeKind == ARRAY_DECLARATOR) {
        Expression * array_len_expr =
    (Expression*)((ArrayDeclarator*)save_decl)->getExpression();

        save_decl = (Declarator*)((ArrayDeclarator*)save_decl)->arrayOf;

        size_expr->setRight(save_decl);

        // n * sizeof [decl]
        BinaryExpression * mult_expr = new BinaryExpression;
        mult_expr->setScope(getScope());
        mult_expr->setContext(getContext());
        mult_expr->setContents("*");
        mult_expr->setLeft(array_len_expr);
        mult_expr->setRight(size_expr);

        size_expr = mult_expr;
    } else size_expr->setRight(save_decl);

    // malloc([size_expr])
    a->getExpressions().push_back(size_expr);
    a->analyze(true); // force with new expr

    // malloc([size_expr]) as [decl]*
    AsExpression * as = new AsExpression;
    as->setScope(getScope());
    as->setContext(getContext());
    as->setContents("as");
    as->setLeft(l);
    // @bad this is an ugly way to copy the declarator
    Declarator * new_save_decl = save_decl->getType()->getGenericDeclarator();
    new_save_decl->setScope(getScope());
    as->setRight(new PointerDeclarator(new_save_decl));

    // @leak
    replace(parent, this, as)->analyze();

    setType(as->getType());

    // @incomplete
    // IDEA: node replacement policy:
    //      Each node has a function pointer that takes the node's parent as an
    //      argument. replacemewith(parent, newnode) where replacemewith is the
    //      replacement "policy" (func ptr) will do something like
    //
    //                    ((Expression*)parent)->setLeft(newnode);
    //
    //      How to delete old node??

     */

    const Type * r_t = getRight()->getType();

    if (r_t->isArray())
        setType(r_t->under()->getPointer());
    else
        setType(r_t->getPointer());

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * NewExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ DeleteExpression ~~~~~

DeleteExpression::DeleteExpression() {
    contents = "delete";

    nodeKind = DELETE_EXPRESSION;
}

bool DeleteExpression::isConstant() { return false; }

void DeleteExpression::analyze(bool force) {
    HANDLE_FORCE();

    getRight()->analyze();
    const Type * t = getRight()->getType()->unRef();

    if (!t->isPointer())
        errorl(getRight()->getContext(), "Operand of delete must be a pointer.",
               true, "got type '" + t->getDemangledName() + "'");

    std::string & r_c = ((Expression *)getRight())->getContents();
    if (getRight()->nodeKind == IDENTIFIER ||
        (r_c == "&" || r_c == "." || r_c == "[]" || r_c == "()"))
        setType(compilation->frontEnd
                    .typeTable[compilation->frontEnd.getBuiltinVoidTypeName()]);
    else
        errorl(getRight()->getContext(),
               "Operand of delete must be a data container.");

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * DeleteExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ SizeofExpression ~~~~~

SizeofExpression::SizeofExpression() {
    contents = "sizeof";

    nodeKind = SIZEOF_EXPRESSION;
}

bool SizeofExpression::isConstant() { return true; }

void SizeofExpression::analyze(bool force) {
    HANDLE_FORCE();

    getRight()->analyze();

    if (getRight()->getType() == VoidType::get())
        errorl(getContext(), "Taking sizeof void.");

    setType(IntType::get(Type::UNSIGNED, 64));

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * SizeofExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ NotExpression ~~~~~

NotExpression::NotExpression() {
    contents = "not";

    nodeKind = NOT_EXPRESSION;
}

Val NotExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a;
    a = ((Expression *)getRight())->eval();
    return evalNot(a, getType());
}

void NotExpression::analyze(bool force) {
    HANDLE_FORCE();

    getRight()->analyze(force);

    const Type * rt = getRight()->getType()->unRef();
    const Type * _bool = BoolType::get();

    if (!equal(rt, _bool))
        errorl(getRight()->getContext(),
               "Operand right operator '" + getContents() +
                   "' must be convertible to type 'bool'.",
               true, "got '" + rt->getDemangledName() + "'");

    setType(_bool);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * NotExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ DerefExpression ~~~~~

DerefExpression::DerefExpression() {
    contents = "@";

    nodeKind = DEREF_EXPRESSION;
}

bool DerefExpression::isConstant() { return false; }

void DerefExpression::analyze(bool force) {
    HANDLE_FORCE();

    getRight()->analyze(force);

    const Type * rt = getRight()->getType()->unRef();

    if (!rt->isPointer())
        errorl(getRight()->getContext(),
               "Operand right of '" + getContents() + "' must be a pointer.",
               true, "got '" + rt->getDemangledName() + "'");

    setType(rt->under());

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * DerefExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ AddressExpression ~~~~~

AddressExpression::AddressExpression() {
    contents = "&";

    nodeKind = ADDRESS_EXPRESSION;
}

bool AddressExpression::isConstant() { return false; }

void AddressExpression::analyze(bool force) {
    HANDLE_FORCE();

    ASTNode * right = getRight();

    right->analyze(force);

    const Type * rt = right->getType();

    if (rt->isRef()) {
        rt = rt->unRef();
    } else {
        rt = rt->unRef();

        if (right->getFlag(IDENT)) {
            Maybe<Symbol *> m_sym = getScope()->getSymbol(
                getScope(), (Identifier *)right, &right->getContext());
            Symbol * sym = nullptr;
            m_sym.assignTo(sym);
            BJOU_DEBUG_ASSERT(sym);
            if (sym->isType())
                errorl(right->getContext(),
                       "Cannot take the address of a type.");
            else if (sym->isProcSet())
                errorl(right->getContext(), "Use of '" +
                                                sym->demangledString() +
                                                "' is ambiguous."); // @bad
            else if (sym->isInterface())
                errorl(right->getContext(),
                       "Cannot take the address of an interface.");
            else if (sym->node()->getFlag(ASTNode::IS_TEMPLATE))
                errorl(right->getContext(),
                       "Cannot take the address of a template definition.");
        } else {
            if (right->getFlag(TERMINAL) || !((Expression *)right)->canBeLVal())
                errorl(right->getContext(),
                       "Operand right of '" + contents +
                           "' operator must be addressable.");
        }
    }

    setType(rt->getPointer());

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * AddressExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ RefExpression ~~~~~

RefExpression::RefExpression() {
    contents = "~";

    nodeKind = REF_EXPRESSION;
}

bool RefExpression::isConstant() { return false; }

void RefExpression::analyze(bool force) {
    HANDLE_FORCE();

    ASTNode * right = getRight();

    right->analyze(force);

    const Type * rt = right->getType()->unRef();

    if (right->getFlag(IDENT)) {
        Maybe<Symbol *> m_sym = getScope()->getSymbol(
            getScope(), (Identifier *)right, &right->getContext());
        Symbol * sym = nullptr;
        m_sym.assignTo(sym);
        BJOU_DEBUG_ASSERT(sym);
        if (sym->isType())
            errorl(right->getContext(), "Cannot take a reference of a type.");
        else if (sym->isProcSet())
            errorl(right->getContext(), "Use of '" + sym->demangledString() +
                                            "' is ambiguous."); // @bad
        else if (sym->isInterface())
            errorl(right->getContext(),
                   "Cannot take a reference of an interface.");
        else if (sym->node()->getFlag(ASTNode::IS_TEMPLATE))
            errorl(right->getContext(),
                   "Cannot take a reference of a template definition.");
    } else {
        if (right->getFlag(TERMINAL) || !((Expression *)right)->canBeLVal())
            errorl(right->getContext(), "Operand right of '" + contents +
                                            "' operator must be addressable.");
    }

    setType(rt->getRef());

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * RefExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ UnaryPostExpression ~~~~~

UnaryPostExpression::UnaryPostExpression() { nodeKind = UNARY_POST_EXPRESSION; }

bool UnaryPostExpression::isConstant() {
    analyze();
    return ((Expression *)left)->isConstant();
}

void UnaryPostExpression::analyze(bool force) {
    HANDLE_FORCE();

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

// ~~~~~ AsExpression ~~~~~

AsExpression::AsExpression() {
    contents = "as";

    nodeKind = AS_EXPRESSION;
}

bool AsExpression::isConstant() {
    return ((Expression *)getLeft())->isConstant();
}

void AsExpression::analyze(bool force) {
    HANDLE_FORCE();

    getLeft()->analyze();
    getRight()->analyze();

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    if (equal(lt, rt)) {
        if (!rt->isPointer())
            errorl(getContext(),
                   "Cast to same type (" + lt->getDemangledName() + " to " +
                       rt->getDemangledName() + ") does nothing.");
    }

    if (!(
            // @incomplete
            (conv(lt, rt) ||
             // (lt->isInt() && rt->isPointer()) || // for a NULL
             (lt->isPointer() && rt->isPointer()) ||
             (lt->isArray() && rt->isPointer() &&
              (equal(lt->under(), rt->under()) ||
               rt->under() == VoidType::get())) ||
             (lt->isPointer() && rt->isProcedure()) || // @temporary
             (lt->isProcedure() && rt->isPointer())    // @temporary
             ))) {
        errorl(getContext(), "Invalid cast: '" + lt->getDemangledName() +
                                 "' to '" + rt->getDemangledName() + "'.");
    }

    setType(rt);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * AsExpression::clone() { return ExpressionClone(this); }
//

// ~~~~~ Identifier ~~~~~

Identifier::Identifier() : unqualified({}), namespaces({}), resolved(nullptr) {
    nodeKind = IDENTIFIER;
    setFlag(IDENT, true);
}

bool Identifier::isConstant() {
    Maybe<Symbol *> m_sym =
        getScope()->getSymbol(getScope(), this, &this->getContext());
    Symbol * sym = nullptr;
    m_sym.assignTo(sym);
    BJOU_DEBUG_ASSERT(sym);

    return !sym->isVar();
}

Val Identifier::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();

    Maybe<Symbol *> m_sym =
        getScope()->getSymbol(getScope(), this, &this->getContext());
    Symbol * sym = nullptr;
    m_sym.assignTo(sym);
    BJOU_DEBUG_ASSERT(sym);

    if (sym->isConstant()) {
        Constant * c = (Constant *)sym->node();
        return ((Expression *)c->getInitialization())->eval();
    } else
        internalError("Could not evaluate expression from Identifier.");
    return {};
}

std::string & Identifier::getUnqualified() { return unqualified; }
void Identifier::setUnqualified(std::string _unqualified) {
    unqualified = _unqualified;
}

std::vector<std::string> & Identifier::getNamespaces() { return namespaces; }
void Identifier::setNamespaces(std::vector<std::string> _namespaces) {
    namespaces = _namespaces;
}
void Identifier::addNamespace(std::string _nspace) {
    namespaces.push_back(_nspace);
}

// Node interface
const Type * Identifier::getType() {
    analyze();

    if (!type) {
        Maybe<Symbol *> m_sym;
        Symbol * sym = nullptr;

        if (getFlag(DIRECT_PROC_REF)) {
            m_sym = getScope()->getSymbol(getScope(), getUnqualified());
            m_sym.assignTo(sym);
            BJOU_DEBUG_ASSERT(sym);
            sym = ((ProcSet *)sym->node())->procs[qualified];
        } else {
            m_sym =
                getScope()->getSymbol(getScope(), this, &this->getContext());
            m_sym.assignTo(sym);
            BJOU_DEBUG_ASSERT(sym);

            if (qualified.empty())
                qualified = mangledIdentifier(this);
        }

        if (sym->isProcSet()) {
            ProcSet * set = (ProcSet *)sym->node();
            Procedure * proc = set->get(nullptr, getRight(), &getContext());
            BJOU_DEBUG_ASSERT(proc);
            qualified = proc->getMangledName();
            setType(proc->getType());
        } else {
            std::string sym_kind = "???";

            if (sym->isType() || sym->isTemplateType())
                sym_kind = "Type declarator";
            else if (sym->isInterface())
                sym_kind = "Interface name";

            errorl(getContext(),
                   sym_kind + " '" + sym->demangledString() +
                       "' not allowed here."); // @bad error message
        }
    }

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    return type;
}

void Identifier::analyze(bool force) {
    HANDLE_FORCE();

    Maybe<Symbol *> m_sym;
    Symbol * sym = nullptr;

    if (getFlag(DIRECT_PROC_REF)) {
        m_sym = getScope()->getSymbol(getScope(), getUnqualified());
        m_sym.assignTo(sym);
        BJOU_DEBUG_ASSERT(sym);
        sym = ((ProcSet *)sym->node())->procs[qualified];
    } else {
        m_sym = getScope()->getSymbol(getScope(), this, &this->getContext());
        m_sym.assignTo(sym);
        BJOU_DEBUG_ASSERT(sym);

        if (qualified.empty())
            qualified = mangledIdentifier(this);
    }

    if (sym->isVar() || sym->isConstant() || sym->isProc()) {
        setType(sym->node()->getType());
    } else if (sym->isProcSet()) {
        ProcSet * set = (ProcSet *)sym->node();
        Procedure * proc =
            set->get(nullptr, getRight(), &getContext(),
                     false); // don't fail.. will catch in getType()
        if (proc) {
            qualified = proc->getMangledName();
            resolved = proc;
            setType(proc->getType());
        }
    } else if (sym->isType() || sym->isTemplateType()) {
        // @what did I do here?
        /* if (compilation->frontEnd.lValStack.empty())
            errorl(getContext(), "Type declarator not allowed here.");
        else errorl(getContext(), "Type declarator not allowed here.", true,
        "Note: no viable procedure found for l-value type " +
        compilation->frontEnd.lValStack.top()->getDemangledName());
        */
        /*
        if (sym->isType())
            setType(sym->node()->getType());
        else internalError("Identifier error."); // @bad!
         */

        if (parent && IS_EXPRESSION(parent) || parent->nodeKind == ARG_LIST) {
            if (parent->nodeKind != ASTNode::ACCESS_EXPRESSION)
                errorl(getContext(), "Can't use type name '" + unqualified +
                                         "' as expression.");
        }

        Declarator * decl = new Declarator;
        decl->setContext(getContext());
        decl->setScope(getScope());
        if (getRight() &&
            getRight()->nodeKind == ASTNode::TEMPLATE_INSTANTIATION)
            decl->setTemplateInst(getRight());
        (*replace)(parent, this, decl);
        decl->setIdentifier(this);
    }

    if (!resolved && sym && !sym->isProcSet())
        resolved = sym->node();

    if (resolved) {
        if (!isCT(this) && isCT(resolved))
            errorl(getContext(), "Referenced symbol '" + unqualified +
                                     "' is only available at compile time.");
    }

    // Identifier get's its type lazily because it might be a reference
    // to an overloaded proc
    // BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * Identifier::clone() { return ExpressionClone(this); }

Identifier::~Identifier() {}
//

// ~~~~~ InitializerList ~~~~~

InitializerList::InitializerList() : objDeclarator(nullptr), expressions({}) {
    nodeKind = INITIALZER_LIST;
}

bool InitializerList::isConstant() {
    bool c = true;
    analyze();
    for (ASTNode * _expr : getExpressions()) {
        Expression * expr = (Expression *)_expr;
        c &= expr->isConstant();
    }
    return c;
}

ASTNode * InitializerList::getObjDeclarator() const { return objDeclarator; }
void InitializerList::setObjDeclarator(ASTNode * _decl) {
    objDeclarator = _decl;
    objDeclarator->parent = this;
    objDeclarator->replace =
        rpget<replacementPolicy_InitializerList_ObjDeclarator>();
}

std::vector<std::string> & InitializerList::getMemberNames() {
    return memberNames;
}
void InitializerList::setMemberNames(std::vector<std::string> _memberNames) {
    memberNames = _memberNames;
}
void InitializerList::addMemberName(std::string memberName) {
    memberNames.push_back(memberName);
}

std::vector<ASTNode *> & InitializerList::getExpressions() {
    return expressions;
}
void InitializerList::setExpressions(std::vector<ASTNode *> _expressions) {
    expressions = _expressions;
}
void InitializerList::addExpression(ASTNode * _expression) {
    _expression->parent = this;
    _expression->replace =
        rpget<replacementPolicy_InitializerList_Expression>();
    expressions.push_back(_expression);
}

// Node interface
void InitializerList::unwrap(std::vector<ASTNode *> & terminals) {
    if (getObjDeclarator())
        getObjDeclarator()->unwrap(terminals);
    for (ASTNode * expression : getExpressions())
        expression->unwrap(terminals);
}

void InitializerList::addSymbols(Scope * _scope) {
    Expression::addSymbols(_scope);
    if (getObjDeclarator())
        getObjDeclarator()->addSymbols(_scope);
    for (ASTNode * expr : getExpressions())
        expr->addSymbols(_scope);
}

void InitializerList::analyze(bool force) {
    HANDLE_FORCE();

    if (getObjDeclarator()) {
        getObjDeclarator()->analyze(force);
        const Type * t = getObjDeclarator()->getType();
        BJOU_DEBUG_ASSERT(t);
        if (!t->isStruct())
            errorl(getObjDeclarator()->getContext(),
                   "Type declarator in initializer list does not denote a "
                   "structure type.");
        StructType * s_t = (StructType *)t; // no constness
        // if (getExpressions().size() != s_t->memberTypes.size())
        // errorl(getContext(), "Number of elements in '" +
        // s_t->getDemangledName() + "' literal does not match definition.
        // Expected " + std::to_string(s_t->memberTypes.size()) + ".");
        std::vector<std::string> & names = getMemberNames();
        std::vector<ASTNode *> & expressions = getExpressions();
        BJOU_DEBUG_ASSERT(names.size() == expressions.size());
        for (int i = 0; i < (int)names.size(); i += 1) {
            std::string & name = names[i];
            if (s_t->memberIndices.count(name) == 0)
                errorl(expressions[i]->getContext(),
                       "No member variable named '" + name + "' in '" +
                           s_t->getDemangledName() + "'.");
            int index = s_t->memberIndices[name];
            const Type * mt = s_t->memberTypes[index];
            compilation->frontEnd.lValStack.push(mt);
            const Type * expr_t = expressions[i]->getType();
            compilation->frontEnd.lValStack.pop();
            if (!conv(expr_t, mt))
                errorl(expressions[i]->getContext(),
                       "Element for '" + name + "' in '" +
                           s_t->getDemangledName() +
                           "' literal differs from expected type '" +
                           mt->getDemangledName() + "'.",
                       true, "got '" + expr_t->getDemangledName() + "'");
            if (expr_t->isPrimative() && mt->isPrimative())
                if (!equal(expr_t, mt))
                    emplaceConversion((Expression *)expressions[i], mt);
        }
        setType(getObjDeclarator()->getType());
    } else {
        // @refactor @bad
        // Using the lvalstack in this way doesn't seem correct.
        // Consider:
        /*
            proc p() : int[4] { # lval push int[4]
                array := { 1, 2, 3, 4, 5 }
            }
         */
        // Currently we will complain about excess elements because we use the
        // top lval int[4] which is irrelevant.

        if (getExpressions().empty())
            errorl(getContext(), "Empty array initializer list not allowed.");

        std::stack<const Type *> & lValStack = compilation->frontEnd.lValStack;
        if (!lValStack.empty()) {
            const Type * t = lValStack.top();
            if (t->isArray()) {
                const ArrayType * a_t = (const ArrayType *)t;
                int destLen = a_t->width;
                if (destLen < (int)getExpressions().size())
                    errorl(getExpressions()[destLen]->getContext(),
                           "Excess element in array initializer.");
            }
        }
        const Type * first_t = getExpressions()[0]->getType();
        for (ASTNode * expr : getExpressions()) {
            const Type * a_t = expr->getType();
            if (!conv(a_t, first_t))
                errorl(expr->getContext(),
                       "Element in '" + first_t->getDemangledName() +
                           "' array literal differs in type.");
        }

        const Type * array_t =
            ArrayType::get(first_t, (int)getExpressions().size());

        setType(array_t);
    }

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * InitializerList::clone() {
    InitializerList * c = ExpressionClone(this);
    ASTNode * o = c->getObjDeclarator();
    std::vector<ASTNode *> & my_expressions = c->getExpressions();
    std::vector<ASTNode *> expressions = my_expressions;
    if (o)
        c->setObjDeclarator(o->clone());
    my_expressions.clear();
    for (ASTNode * e : expressions)
        c->addExpression(e->clone());
    return c;
}

void InitializerList::desugar() {
    if (getObjDeclarator())
        getObjDeclarator()->desugar();
    for (ASTNode * node : getExpressions())
        node->desugar();
}

InitializerList::~InitializerList() {
    if (objDeclarator)
        delete objDeclarator;
    for (ASTNode * expr : expressions)
        delete expr;
}
//

// ~~~~~ SliceExpression ~~~~~

SliceExpression::SliceExpression()
    : src(nullptr), start(nullptr), length(nullptr) {
    nodeKind = SLICE_EXPRESSION;
}

bool SliceExpression::isConstant() {
    if (!((Expression *)getSrc())->isConstant())
        return false;
    if (!((Expression *)getStart())->isConstant())
        return false;
    if (!((Expression *)getLength())->isConstant())
        return false;
    return true;
}

ASTNode * SliceExpression::getSrc() const { return src; }
void SliceExpression::setSrc(ASTNode * _src) {
    src = _src;
    src->parent = this;
    src->replace = rpget<replacementPolicy_SliceExpression_Src>();
}

ASTNode * SliceExpression::getStart() const { return start; }
void SliceExpression::setStart(ASTNode * _start) {
    start = _start;
    start->parent = this;
    start->replace = rpget<replacementPolicy_SliceExpression_Start>();
}

ASTNode * SliceExpression::getLength() const { return length; }
void SliceExpression::setLength(ASTNode * _length) {
    length = _length;
    length->parent = this;
    length->replace = rpget<replacementPolicy_SliceExpression_Length>();
}

// Node interface
void SliceExpression::unwrap(std::vector<ASTNode *> & terminals) {
    src->unwrap(terminals);
    start->unwrap(terminals);
    length->unwrap(terminals);
}

void SliceExpression::addSymbols(Scope * _scope) {
    setScope(_scope);
    src->addSymbols(_scope);
    start->addSymbols(_scope);
    length->addSymbols(_scope);
}

void SliceExpression::analyze(bool force) {
    HANDLE_FORCE();

    getSrc()->analyze(force);
    getStart()->analyze(force);
    getLength()->analyze(force);

    const Type * src_t = getSrc()->getType()->unRef();
    const Type * start_t = getStart()->getType()->unRef();
    const Type * length_t = getLength()->getType()->unRef();

    if (!src_t->isPointer() && !src_t->isArray() &&
        !src_t->isSlice()) // dynamic array?
        errorl(getSrc()->getContext(),
               "Slice source must be either a pointer, an array, or another "
               "slice.",
               true, "got '" + src_t->getDemangledName() + "'");

    if (!conv(IntType::get(Type::Sign::UNSIGNED, 64), start_t))
        errorl(getStart()->getContext(),
               "Can't use expression of type '" + start_t->getDemangledName() +
                   "' as starting index in slice expression.");

    if (!conv(IntType::get(Type::Sign::UNSIGNED, 64), length_t))
        errorl(getLength()->getContext(),
               "Can't use expression of type '" + length_t->getDemangledName() +
                   "' as length in slice expression.");

    setType(SliceType::get(src_t->under()));

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");

    setFlag(ANALYZED, true);

    desugar();
}

ASTNode * SliceExpression::clone() {
    SliceExpression * c = ExpressionClone(this);

    c->setSrc(getSrc());
    c->setStart(getStart());
    c->setLength(getLength());

    return c;
}

void SliceExpression::desugar() {
    getSrc()->desugar();
    getStart()->desugar();
    getLength()->desugar();

    CallExpression * call = new CallExpression;
    call->setContext(getContext());

    // __bjou_slice!(T).create
    Declarator * slice_decl =
        ((SliceType *)getType())->getRealType()->getGenericDeclarator();
    Identifier * create = new Identifier;
    create->setUnqualified("create");
    AccessExpression * l = new AccessExpression;
    l->setLeft(slice_decl);
    l->setRight(create);

    // (src, start, len)
    ArgList * r = new ArgList;
    r->addExpression(getSrc());
    r->addExpression(getStart());
    r->addExpression(getLength());

    call->setLeft(l);
    call->setRight(r);

    call->addSymbols(getScope());

    slice_decl->desugar();

    call->analyze();

    call->setType(getType());

    (*replace)(parent, this, call);
}

SliceExpression::~SliceExpression() {
    BJOU_DEBUG_ASSERT(src);
    delete src;
    BJOU_DEBUG_ASSERT(start);
    delete start;
    BJOU_DEBUG_ASSERT(length);
    delete length;
}
//

// ~~~~~ DynamicArrayExpression ~~~~~

DynamicArrayExpression::DynamicArrayExpression() : typeDeclarator(nullptr) {
    nodeKind = DYNAMIC_ARRAY_EXPRESSION;
}

bool DynamicArrayExpression::isConstant() { return false; }

ASTNode * DynamicArrayExpression::getTypeDeclarator() const {
    return typeDeclarator;
}
void DynamicArrayExpression::setTypeDeclarator(ASTNode * _decl) {
    typeDeclarator = _decl;
    typeDeclarator->parent = this;
    typeDeclarator->replace =
        rpget<replacementPolicy_DynamicArrayExpression_TypeDeclarator>();
}

// Node interface
void DynamicArrayExpression::unwrap(std::vector<ASTNode *> & terminals) {
    typeDeclarator->unwrap(terminals);
}

void DynamicArrayExpression::addSymbols(Scope * _scope) {
    setScope(_scope);
    typeDeclarator->addSymbols(_scope);
}

void DynamicArrayExpression::analyze(bool force) {
    HANDLE_FORCE();

    getTypeDeclarator()->analyze(force);

    const Type * t = getTypeDeclarator()->getType();

    setType(DynamicArrayType::get(t));

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");

    setFlag(ANALYZED, true);

    desugar();
}

ASTNode * DynamicArrayExpression::clone() {
    DynamicArrayExpression * c = ExpressionClone(this);

    c->setTypeDeclarator(getTypeDeclarator());

    return c;
}

void DynamicArrayExpression::desugar() {
    getTypeDeclarator()->desugar();

    CallExpression * call = new CallExpression;
    call->setContext(getContext());

    // __bjou_dynamic_array!(T).create
    Declarator * da_decl =
        ((DynamicArrayType *)getType())->getRealType()->getGenericDeclarator();
    Identifier * create = new Identifier;
    create->setUnqualified("create");
    AccessExpression * l = new AccessExpression;
    l->setLeft(da_decl);
    l->setRight(create);

    // ()
    ArgList * r = new ArgList;

    call->setLeft(l);
    call->setRight(r);

    call->addSymbols(getScope());

    da_decl->desugar();

    call->analyze();

    call->setType(getType());

    (*replace)(parent, this, call);
}

DynamicArrayExpression::~DynamicArrayExpression() {
    BJOU_DEBUG_ASSERT(typeDeclarator);
    delete typeDeclarator;
}
//

// ~~~~~ LenExpression ~~~~~

LenExpression::LenExpression() : expr(nullptr) { nodeKind = LEN_EXPRESSION; }

bool LenExpression::isConstant() {
    if (((Expression *)getLeft())->isConstant()) {
        if (getLeft()->getType()->isArray())
            return true;
    }
    return false;
}

ASTNode * LenExpression::getExpr() const { return expr; }
void LenExpression::setExpr(ASTNode * _expr) {
    expr = _expr;
    expr->parent = this;
    expr->replace = rpget<replacementPolicy_LenExpression_Expr>();
}

// Node interface
void LenExpression::unwrap(std::vector<ASTNode *> & terminals) {
    expr->unwrap(terminals);
}

void LenExpression::addSymbols(Scope * _scope) {
    setScope(_scope);
    expr->addSymbols(_scope);
}

void LenExpression::analyze(bool force) {
    HANDLE_FORCE();

    const Type * expr_t = getExpr()->getType()->unRef();

    getExpr()->analyze(force);

    if (!expr_t->isArray() && !expr_t->isSlice() && !expr_t->isDynamicArray())
        errorl(getExpr()->getContext(),
               "Object of cardinality expression must be an array, slice, or "
               "dynamic array.",
               true, "got '" + expr_t->getDemangledName() + "'");

    setType(IntType::get(Type::Sign::UNSIGNED, 64));

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");

    setFlag(ANALYZED, true);

    desugar();
}

ASTNode * LenExpression::clone() {
    LenExpression * c = ExpressionClone(this);

    c->setExpr(getExpr());

    return c;
}

void LenExpression::desugar() {
    const Type * expr_t = getExpr()->getType()->unRef();

    ASTNode * replacement = nullptr;

    if (expr_t->isArray()) {
        AsExpression * as = new AsExpression;
        as->setContext(getContext());

        ArrayType * array_t = (ArrayType *)expr_t;
        unsigned int len = array_t->width;

        IntegerLiteral * lit = new IntegerLiteral;
        lit->setContents(std::to_string(len));
        lit->setContext(getContext());

        Declarator * decl =
            IntType::get(Type::Sign::UNSIGNED, 64)->getGenericDeclarator();
        decl->setContext(getContext());

        as->setLeft(lit);
        as->setRight(decl);

        replacement = as;
    } else if (expr_t->isSlice()) {
        AccessExpression * access = new AccessExpression;
        access->setContext(getContext());

        Identifier * __len = new Identifier;
        __len->setContext(getContext());
        __len->setUnqualified("__len");

        Expression * l = (Expression *)getExpr();

        l->setType(((SliceType *)expr_t)->getRealType());
        l->setFlag(ANALYZED, true);

        access->setLeft(l->clone());
        access->setRight(__len);

        replacement = access;
    } else if (expr_t->isDynamicArray()) {
        AccessExpression * access = new AccessExpression;
        access->setContext(getContext());

        Identifier * __len = new Identifier;
        __len->setContext(getContext());
        __len->setUnqualified("__used");

        Expression * l = (Expression *)getExpr();

        l->setType(((DynamicArrayType *)expr_t)->getRealType());
        l->setFlag(ANALYZED, true);

        access->setLeft(l->clone());
        access->setRight(__len);

        replacement = access;
    } else
        BJOU_DEBUG_ASSERT(false);

    replacement->addSymbols(getScope());
    replacement->desugar();
    // @bad hack
    replacement->setFlag(ANALYZED, true);
    ((Expression *)replacement)
        ->setType(IntType::get(Type::Sign::UNSIGNED, 64));
    (*replace)(parent, this, replacement);
}

LenExpression::~LenExpression() {
    BJOU_DEBUG_ASSERT(expr);
    delete expr;
}
//

// ~~~~~ BooleanLiteral ~~~~~

BooleanLiteral::BooleanLiteral() { nodeKind = BOOLEAN_LITERAL; }

bool BooleanLiteral::isConstant() { return true; }
Val BooleanLiteral::eval() {
    Val v;
    analyze();
    v.t = getType();
    v.as_i64 = getContents() == "true" ? 1 : 0;
    return v;
}

void BooleanLiteral::analyze(bool force) {
    HANDLE_FORCE();

    setType(BoolType::get());

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * BooleanLiteral::clone() { return ExpressionClone(this); }
//

// ~~~~~ IntegerLiteral ~~~~~

IntegerLiteral::IntegerLiteral() { nodeKind = INTEGER_LITERAL; }

bool IntegerLiteral::isConstant() { return true; }
Val IntegerLiteral::eval() {
    Val v;
    analyze();
    v.t = getType();
    v.as_i64 = atoll(getContents().c_str());
    return v;
}

void IntegerLiteral::analyze(bool force) {
    HANDLE_FORCE();

    setType(IntType::get(Type::SIGNED, 32));

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * IntegerLiteral::clone() { return ExpressionClone(this); }
//

// ~~~~~ FloatLiteral ~~~~~

FloatLiteral::FloatLiteral() { nodeKind = FLOAT_LITERAL; }

bool FloatLiteral::isConstant() { return true; }
Val FloatLiteral::eval() {
    Val v;
    analyze();
    v.t = getType();
    v.as_f64 = atof(getContents().c_str());
    return v;
}

void FloatLiteral::analyze(bool force) {
    HANDLE_FORCE();

    setType(FloatType::get(32));

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * FloatLiteral::clone() { return ExpressionClone(this); }
//

// ~~~~~ StringLiteral ~~~~~

StringLiteral::StringLiteral() { nodeKind = STRING_LITERAL; }

bool StringLiteral::isConstant() { return true; }
Val StringLiteral::eval() {
    Val v;
    analyze();
    v.t = getType();
    v.as_string = str_escape(getContents());
    return v;
}

void StringLiteral::analyze(bool force) {
    HANDLE_FORCE();

    std::string str = str_escape(getContents());
    setContents(de_quote(str));

    setType(CharType::get()->getPointer());

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * StringLiteral::clone() { return ExpressionClone(this); }
//

// ~~~~~ CharLiteral ~~~~~

CharLiteral::CharLiteral() { nodeKind = CHAR_LITERAL; }

bool CharLiteral::isConstant() { return true; }
Val CharLiteral::eval() {
    Val v;
    analyze();
    v.t = getType();
    v.as_i64 = get_ch_value(getContents());
    return v;
}

void CharLiteral::analyze(bool force) {
    HANDLE_FORCE();

    setType(CharType::get());

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * CharLiteral::clone() { return ExpressionClone(this); }
//

// ~~~~~ ProcLiteral ~~~~~

ProcLiteral::ProcLiteral() { nodeKind = PROC_LITERAL; }

bool ProcLiteral::isConstant() { return true; }

void ProcLiteral::analyze(bool force) {
    HANDLE_FORCE();

    BJOU_DEBUG_ASSERT(getRight());

    if (getRight()->getFlag(ASTNode::IS_TEMPLATE))
        errorl(getRight()->getNameContext(),
               "Can't determine type of expression containing uninstantiated "
               "template procedure.");

    Procedure * proc = (Procedure *)getRight();

    // proc->addSymbols(compilation->frontEnd.globalScope);
    proc->analyze(force);
    compilation->frontEnd.deferredAST.push_back(proc);

    const Type * rt = proc->getType();
    setType(rt);

    Identifier * proc_ident = new Identifier;
    proc_ident->setScope(getScope());
    proc_ident->setUnqualified(proc->getName());
    proc_ident->qualified = proc->getMangledName();
    proc_ident->setFlag(Identifier::DIRECT_PROC_REF, true);
    proc_ident->setFlag(Expression::TERMINAL, true);
    proc_ident->addSymbols(getScope());

    (*this->replace)(parent, this, proc_ident);

    proc_ident->analyze();

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

void ProcLiteral::addSymbols(Scope * _scope) {
    setScope(_scope);
    ASTNode * left = getLeft();
    ASTNode * right = getRight();
    if (left)
        left->addSymbols(scope);
    if (right)
        right->addSymbols(compilation->frontEnd.globalScope);
}

ASTNode * ProcLiteral::clone() { return ExpressionClone(this); }
//

// ~~~~~ ExternLiteral ~~~~~

ExternLiteral::ExternLiteral() { nodeKind = EXTERN_LITERAL; }

bool ExternLiteral::isConstant() { return true; }

void ExternLiteral::analyze(bool force) {
    HANDLE_FORCE();

    BJOU_DEBUG_ASSERT(getRight());

    getRight()->analyze(force);

    setType(getRight()->getType());

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

void ExternLiteral::addSymbols(Scope * _scope) {
    setScope(_scope);
    ASTNode * left = getLeft();
    ASTNode * right = getRight();
    if (left)
        left->addSymbols(scope);
    if (right)
        right->addSymbols(compilation->frontEnd.globalScope);
}

ASTNode * ExternLiteral::clone() { return ExpressionClone(this); }
//

// ~~~~~ SomeLiteral ~~~~~

SomeLiteral::SomeLiteral() { nodeKind = SOME_LITERAL; }

bool SomeLiteral::isConstant() { return true; }

void SomeLiteral::analyze(bool force) {
    HANDLE_FORCE();

    getRight()->analyze();

    setType(getRight()->getType()->getMaybe());

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * SomeLiteral::clone() { return ExpressionClone(this); }
//

// ~~~~~ NothingLiteral ~~~~~

NothingLiteral::NothingLiteral() { nodeKind = NOTHING_LITERAL; }

bool NothingLiteral::isConstant() { return true; }

void NothingLiteral::analyze(bool force) {
    HANDLE_FORCE();

    std::stack<const Type *> & lValStack = compilation->frontEnd.lValStack;

    if (lValStack.empty())
        errorl(getContext(),
               "No l-val to deduce maybe type 'nothing' literal.");
    else if (!lValStack.top()->isMaybe())
        errorl(getContext(), "Relevant l-val does not describe a maybe type. "
                             "Type of 'nothing' literal could not be deduced.");

    setType(compilation->frontEnd.lValStack.top());

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * NothingLiteral::clone() { return ExpressionClone(this); }
//

// ~~~~~ TupleLiteral ~~~~~

TupleLiteral::TupleLiteral() { nodeKind = TUPLE_LITERAL; }

bool TupleLiteral::isConstant() {
    bool c = true;
    for (ASTNode * _expr : getSubExpressions()) {
        Expression * expr = (Expression *)_expr;
        expr->analyze();
        c &= expr->isConstant();
    }
    return c;
}

std::vector<ASTNode *> & TupleLiteral::getSubExpressions() {
    return subExpressions;
}
void TupleLiteral::setSubExpressions(std::vector<ASTNode *> _subExpressions) {
    subExpressions = _subExpressions;
}
void TupleLiteral::addSubExpression(ASTNode * _subExpression) {
    _subExpression->parent = this;
    _subExpression->replace =
        rpget<replacementPolicy_TupleLiteral_subExpression>();
    subExpressions.push_back(_subExpression);
}

void TupleLiteral::addSymbols(Scope * _scope) {
    Expression::addSymbols(_scope);
    for (ASTNode * sub : getSubExpressions())
        sub->addSymbols(_scope);
}

void TupleLiteral::analyze(bool force) {
    HANDLE_FORCE();

    std::vector<const Type *> types;
    for (ASTNode * expr : getSubExpressions()) {
        expr->analyze();
        types.push_back(expr->getType());
    }

    setType(TupleType::get(types));

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * TupleLiteral::clone() {
    TupleLiteral * t = ExpressionClone(this);
    std::vector<ASTNode *> & my_subExpressions = getSubExpressions();
    std::vector<ASTNode *> expressions = my_subExpressions;
    my_subExpressions.clear();
    for (ASTNode * e : expressions)
        t->addSubExpression(e->clone());
    return t;
}

void TupleLiteral::desugar() {
    for (ASTNode * expr : getSubExpressions())
        expr->desugar();
}
//

// ~~~~~ Declarator ~~~~~

Declarator::Declarator()
    : identifier(nullptr), templateInst(nullptr), typeSpecifiers({}),
      createdFromType(false) {
    nodeKind = DECLARATOR;
    setFlag(IMPLIES_COMPLETE, true);
}

ASTNode * Declarator::getIdentifier() const { return identifier; }
void Declarator::setIdentifier(ASTNode * _identifier) {
    identifier = _identifier;
    identifier->parent = this;
    identifier->replace = rpget<replacementPolicy_Declarator_Identifier>();
}

ASTNode * Declarator::getTemplateInst() const { return templateInst; }
void Declarator::setTemplateInst(ASTNode * _templateInst) {
    templateInst = _templateInst;
    templateInst->parent = this;
    templateInst->replace = rpget<replacementPolicy_Declarator_TemplateInst>();
}

std::vector<std::string> & Declarator::getTypeSpecifiers() {
    return typeSpecifiers;
}
void Declarator::setTypeSpecifiers(std::vector<std::string> _specifiers) {
    typeSpecifiers = _specifiers;
}
void Declarator::addTypeSpecifier(std::string _specifier) {
    typeSpecifiers.push_back(_specifier);
}

// Node interface
void Declarator::analyze(bool force) {
    HANDLE_FORCE();

    std::string mangled = mangleSymbol();

    // Maybe<Symbol*> m_sym = getScope()->getSymbol(getScope(), getIdentifier(),
    // &getContext(), /* traverse = */true, /* fail = */false);
    Maybe<Symbol *> m_sym =
        getScope()->getSymbol(getScope(), mangled, &getContext(),
                              /* traverse = */ true, /* fail = */ false);
    Symbol * sym = nullptr;

    if (m_sym.assignTo(sym)) {
        if (!sym->isAlias()) {
            const Type * t = sym->node()->getType();
            BJOU_DEBUG_ASSERT(t);

            if (t->isStruct()) {
                if (getTemplateInst())
                    errorl(getTemplateInst()->getContext(),
                           "'" + sym->demangledString() +
                               "' is not a template type.");
            } else if (sym->isTemplateType()) {
                if (!getTemplateInst())
                    errorl(getContext(),
                           "Missing template instantiation arguments.");
                TemplateStruct * ttype = (TemplateStruct *)sym->node();
                Declarator * new_decl = makeTemplateStruct(ttype, getTemplateInst())
                                            ->getType()
                                            ->getGenericDeclarator();
                new_decl->setScope(getScope());
                new_decl->setContext(getContext());
                (*replace)(parent, this, new_decl);
                new_decl->templateInst = nullptr;
                new_decl->analyze(true);
                return;
            } else if (!t->isPrimative()) {
                errorl(getContext(),
                       "'" + sym->demangledString() + "' is not a type.");
            }
        } 
    } else if (compilation->frontEnd.typeTable.count(mangled) == 0 ||
               !compilation->frontEnd.typeTable[mangled]->isPrimative())
        getScope()->getSymbol(getScope(), getIdentifier(),
                              &getContext()); // should just produce an error

    setFlag(ANALYZED, true);
}

void Declarator::unwrap(std::vector<ASTNode *> & terminals) {
    if (getTemplateInst())
        getTemplateInst()->unwrap(terminals);
    if (getIdentifier())
        getIdentifier()->unwrap(terminals);
    terminals.push_back(this);
}

ASTNode * Declarator::clone() { return DeclaratorClone(this); }

void Declarator::desugar() {
    getIdentifier()->desugar();
    if (getTemplateInst())
        getTemplateInst()->desugar();
}

const Type * Declarator::getType() {
    analyze();

    if (compilation->frontEnd.primativeTypeTable.count(mangleSymbol()) > 0)
        return compilation->frontEnd.primativeTypeTable[mangleSymbol()];

    // Maybe<Symbol*> m_sym = getScope()->getSymbol(getScope(), identifier,
    // &getContext());
    std::string mangled = mangleSymbol();
    Maybe<Symbol *> m_sym =
        getScope()->getSymbol(getScope(), mangled, &getContext());
    Symbol * sym = nullptr;
    m_sym.assignTo(sym);
    BJOU_DEBUG_ASSERT(sym);

    if (sym->isTemplateType())
        return PlaceholderType::get(); // @bad -- not intended use of
                                       // PlaceholderType
    return sym->node()->getType();
}

void Declarator::addSymbols(Scope * _scope) {
    setScope(_scope);
    if (getTemplateInst())
        getTemplateInst()->addSymbols(_scope);
}

Declarator::~Declarator() {
    if (!createdFromType) {
        if (identifier)
            delete identifier;
        if (templateInst)
            delete templateInst;
    }
}
//

// Declarator interface
std::string Declarator::mangleSymbol() {
    return mangledIdentifier((Identifier *)getIdentifier());
    /*
    if (shouldAnalyze)
        analyze();

    Identifier * identifier = (Identifier*)getIdentifier();
    if (identifier->namespaces.empty() &&
        compilation->frontEnd.typeTable.count(identifier->getUnqualified()) &&
        compilation->frontEnd.typeTable[identifier->getUnqualified()]->isPrimative())
        return identifier->getUnqualified();

    Maybe<Symbol*> m_sym = getScope()->getSymbol(identifier, &getContext());
    Symbol * sym = nullptr;
    m_sym.assignTo(sym);
    BJOU_DEBUG_ASSERT(sym);
    return sym->name;
     */
}

std::string Declarator::mangleAndPrefixSymbol() {
    std::string mangled = mangleSymbol();
    return "T" + std::to_string(mangled.size()) + mangled;
}

const ASTNode * Declarator::getBase() const { return this; }
ASTNode * Declarator::under() const { return (ASTNode *)this; }

void Declarator::propagateScope(Scope * _scope) { scope = _scope; }
//

// ~~~~~ ArrayDeclarator ~~~~~

ArrayDeclarator::ArrayDeclarator()
    : arrayOf(nullptr), expression(nullptr), size(-2) {
    nodeKind = ARRAY_DECLARATOR;
}

ArrayDeclarator::ArrayDeclarator(ASTNode * _arrayOf)
    : arrayOf(_arrayOf), expression(nullptr), size(-2) {
    nodeKind = ARRAY_DECLARATOR;
    setArrayOf(_arrayOf);
}

ArrayDeclarator::ArrayDeclarator(ASTNode * _arrayOf, ASTNode * _expression)
    : arrayOf(_arrayOf), expression(_expression), size(-2) {
    nodeKind = ARRAY_DECLARATOR;
}

ASTNode * ArrayDeclarator::getArrayOf() const { return arrayOf; }
void ArrayDeclarator::setArrayOf(ASTNode * _arrayOf) {
    arrayOf = _arrayOf;
    arrayOf->parent = this;
    arrayOf->replace = rpget<replacementPolicy_ArrayDeclarator_ArrayOf>();
}

ASTNode * ArrayDeclarator::getExpression() const { return expression; }
void ArrayDeclarator::setExpression(ASTNode * _expression) {
    expression = _expression;
    expression->parent = this;
    expression->replace = rpget<replacementPolicy_ArrayDeclarator_Expression>();
}

// Node interface
void ArrayDeclarator::analyze(bool force) {
    HANDLE_FORCE();
    getArrayOf()->analyze();
    if (getExpression()) {
        Expression * expr = (Expression *)getExpression();
        expr->analyze();
        expr = (Expression *)getExpression(); // refresh
        if (expr->isConstant()) {
            Val v = expr->eval();
            size = (int)v.as_i64;
        } else
            size = -1;
    } else if (size == -2)
        size = -1;
    // more to be done here
    setFlag(ANALYZED, true);
}

ArrayDeclarator::~ArrayDeclarator() {
    BJOU_DEBUG_ASSERT(arrayOf);
    delete arrayOf;
    if (expression && !createdFromType)
        delete expression;
}

void ArrayDeclarator::addSymbols(Scope * _scope) {
    setScope(_scope);
    getArrayOf()->addSymbols(_scope);
    if (expression)
        expression->addSymbols(scope);
}

void ArrayDeclarator::unwrap(std::vector<ASTNode *> & terminals) {
    getArrayOf()->unwrap(terminals);
    if (getExpression())
        getExpression()->unwrap(terminals);
}

ASTNode * ArrayDeclarator::clone() {
    ArrayDeclarator * c = DeclaratorClone(this);

    // if (c->getArrayOf())
    c->setArrayOf(c->getArrayOf()->clone());
    if (c->getExpression())
        c->setExpression(c->getExpression()->clone());

    return c;
}

void ArrayDeclarator::desugar() {
    getArrayOf()->desugar();
    getExpression()->desugar();
}

const Type * ArrayDeclarator::getType() {
    ASTNode * expression = getExpression();
    Declarator * arrayOf = (Declarator *)getArrayOf();
    const Type * array_t = nullptr;
    BJOU_DEBUG_ASSERT(expression);
    Expression * expr = (Expression *)getExpression();
    expr->analyze();
    int width = -1;
    if (expr->isConstant()) {
        expr = (Expression *)getExpression(); // refresh
        width = (int)expr->eval().as_i64;
    }
    array_t = ArrayType::get(arrayOf->getType(), width);
    return array_t;
}

//

// Declarator interface
std::string ArrayDeclarator::mangleSymbol() {
    return "a" + ((Declarator *)getArrayOf())->mangleSymbol();
}

std::string ArrayDeclarator::mangleAndPrefixSymbol() {
    std::string baseMangled =
        ((Declarator *)getArrayOf())->mangleAndPrefixSymbol();
    return baseMangled.substr(0, strlen("T")) + "a" +
           baseMangled.substr(strlen("T") /* to end */);
}

const ASTNode * ArrayDeclarator::getBase() const {
    return ((Declarator *)getArrayOf())->getBase();
}

ASTNode * ArrayDeclarator::under() const { return (Declarator *)getArrayOf(); }

void ArrayDeclarator::propagateScope(Scope * _scope) {
    scope = _scope;
    ((Declarator *)getArrayOf())->propagateScope(_scope);
}

//

// ~~~~~ SliceDeclarator ~~~~~

SliceDeclarator::SliceDeclarator() : sliceOf(nullptr) {
    nodeKind = SLICE_DECLARATOR;
}

SliceDeclarator::SliceDeclarator(ASTNode * _sliceOf) : sliceOf(_sliceOf) {
    nodeKind = SLICE_DECLARATOR;
    setSliceOf(_sliceOf);
}

ASTNode * SliceDeclarator::getSliceOf() const { return sliceOf; }
void SliceDeclarator::setSliceOf(ASTNode * _sliceOf) {
    sliceOf = _sliceOf;
    sliceOf->parent = this;
    sliceOf->replace = rpget<replacementPolicy_SliceDeclarator_SliceOf>();
}

// Node interface
void SliceDeclarator::analyze(bool force) {
    HANDLE_FORCE();

    getSliceOf()->analyze();

    setFlag(ANALYZED, true);

    desugar();
}

SliceDeclarator::~SliceDeclarator() {
    BJOU_DEBUG_ASSERT(sliceOf);
    delete sliceOf;
}

void SliceDeclarator::addSymbols(Scope * _scope) {
    setScope(_scope);
    getSliceOf()->addSymbols(_scope);
}

void SliceDeclarator::unwrap(std::vector<ASTNode *> & terminals) {
    getSliceOf()->unwrap(terminals);
}

ASTNode * SliceDeclarator::clone() {
    SliceDeclarator * c = DeclaratorClone(this);

    c->setSliceOf(c->getSliceOf()->clone());

    return c;
}

void SliceDeclarator::desugar() {
    /*
    getSliceOf()->desugar();

    SliceType * slice_t = (SliceType*)getType();

    Declarator * new_decl = slice_t->getRealType()->getGenericDeclarator();

    (*replace)(parent, this, new_decl);

    new_decl->addSymbols(getScope());
    new_decl->analyze();
    */
}

const Type * SliceDeclarator::getType() {
    return SliceType::get(getSliceOf()->getType());
}

//

// Declarator interface
std::string SliceDeclarator::mangleSymbol() {
    return "s" + ((Declarator *)getSliceOf())->mangleSymbol();
}

std::string SliceDeclarator::mangleAndPrefixSymbol() {
    std::string baseMangled =
        ((Declarator *)getSliceOf())->mangleAndPrefixSymbol();
    return baseMangled.substr(0, strlen("T")) + "s" +
           baseMangled.substr(strlen("T") /* to end */);
}

const ASTNode * SliceDeclarator::getBase() const {
    return ((Declarator *)getSliceOf())->getBase();
}

ASTNode * SliceDeclarator::under() const { return (Declarator *)getSliceOf(); }

void SliceDeclarator::propagateScope(Scope * _scope) {
    scope = _scope;
    ((Declarator *)getSliceOf())->propagateScope(_scope);
}

//

// ~~~~~ DynamicArrayDeclarator ~~~~~

DynamicArrayDeclarator::DynamicArrayDeclarator() : arrayOf(nullptr) {
    nodeKind = DYNAMIC_ARRAY_DECLARATOR;
}

DynamicArrayDeclarator::DynamicArrayDeclarator(ASTNode * _arrayOf)
    : arrayOf(_arrayOf) {
    nodeKind = DYNAMIC_ARRAY_DECLARATOR;
    setArrayOf(_arrayOf);
}

ASTNode * DynamicArrayDeclarator::getArrayOf() const { return arrayOf; }
void DynamicArrayDeclarator::setArrayOf(ASTNode * _arrayOf) {
    arrayOf = _arrayOf;
    arrayOf->parent = this;
    arrayOf->replace =
        rpget<replacementPolicy_DynamicArrayDeclarator_ArrayOf>();
}

// Node interface
void DynamicArrayDeclarator::analyze(bool force) {
    HANDLE_FORCE();

    getArrayOf()->analyze();

    setFlag(ANALYZED, true);

    desugar();
}

DynamicArrayDeclarator::~DynamicArrayDeclarator() {
    BJOU_DEBUG_ASSERT(arrayOf);
    delete arrayOf;
}

void DynamicArrayDeclarator::addSymbols(Scope * _scope) {
    setScope(_scope);
    getArrayOf()->addSymbols(_scope);
}

void DynamicArrayDeclarator::unwrap(std::vector<ASTNode *> & terminals) {
    getArrayOf()->unwrap(terminals);
}

ASTNode * DynamicArrayDeclarator::clone() {
    DynamicArrayDeclarator * c = DeclaratorClone(this);

    c->setArrayOf(c->getArrayOf()->clone());

    return c;
}

void DynamicArrayDeclarator::desugar() {
    /*
    getArrayOf()->desugar();

    DynamicArrayType * dyn_t = (DynamicArrayType*)getType();

    Declarator * new_decl = dyn_t->getRealType()->getGenericDeclarator();

    (*replace)(parent, this, new_decl);

    new_decl->addSymbols(getScope());
    new_decl->analyze();
    */
}

const Type * DynamicArrayDeclarator::getType() {
    return DynamicArrayType::get(getArrayOf()->getType());
}

//

// Declarator interface
std::string DynamicArrayDeclarator::mangleSymbol() {
    return "a" + ((Declarator *)getArrayOf())->mangleSymbol();
}

std::string DynamicArrayDeclarator::mangleAndPrefixSymbol() {
    std::string baseMangled =
        ((Declarator *)getArrayOf())->mangleAndPrefixSymbol();
    return baseMangled.substr(0, strlen("T")) + "a" +
           baseMangled.substr(strlen("T") /* to end */);
}

const ASTNode * DynamicArrayDeclarator::getBase() const {
    return ((Declarator *)getArrayOf())->getBase();
}

ASTNode * DynamicArrayDeclarator::under() const {
    return (Declarator *)getArrayOf();
}

void DynamicArrayDeclarator::propagateScope(Scope * _scope) {
    scope = _scope;
    ((Declarator *)getArrayOf())->propagateScope(_scope);
}

//

// ~~~~~ PointerDeclarator ~~~~~

PointerDeclarator::PointerDeclarator() : pointerOf(nullptr) {
    nodeKind = POINTER_DECLARATOR;
}

PointerDeclarator::PointerDeclarator(ASTNode * _pointerOf)
    : pointerOf(_pointerOf) {
    nodeKind = POINTER_DECLARATOR;
    // a pointer anywhere in the chain of different declarator types implies
    // that the base type does not need to be complete in order for the total
    // declarator to be valid in any context
    setPointerOf(_pointerOf);
    ((Declarator *)getBase())->setFlag(IMPLIES_COMPLETE, false);
}

ASTNode * PointerDeclarator::getPointerOf() const { return pointerOf; }
void PointerDeclarator::setPointerOf(ASTNode * _pointerOf) {
    pointerOf = _pointerOf;
    pointerOf->parent = this;
    pointerOf->replace = rpget<replacementPolicy_PointerDeclarator_PointerOf>();
}

// Node interface
void PointerDeclarator::analyze(bool force) {
    HANDLE_FORCE();
    getPointerOf()->analyze();
    setFlag(ANALYZED, true);
}

PointerDeclarator::~PointerDeclarator() {
    BJOU_DEBUG_ASSERT(pointerOf);
    delete pointerOf;
}

void PointerDeclarator::addSymbols(Scope * _scope) {
    setScope(_scope);
    getPointerOf()->addSymbols(_scope);
}

void PointerDeclarator::unwrap(std::vector<ASTNode *> & terminals) {
    getPointerOf()->unwrap(terminals);
}

ASTNode * PointerDeclarator::clone() {
    PointerDeclarator * c = DeclaratorClone(this);

    // if (c->getPointerOf())
    c->setPointerOf(c->getPointerOf()->clone());

    return c;
}

void PointerDeclarator::desugar() { getPointerOf()->desugar(); }

const Type * PointerDeclarator::getType() {
    Declarator * pointerOf = (Declarator *)getPointerOf();
    return PointerType::get(pointerOf->getType());
}

// Declarator interface
std::string PointerDeclarator::mangleSymbol() {
    return "p" + ((Declarator *)getPointerOf())->mangleSymbol();
}

std::string PointerDeclarator::mangleAndPrefixSymbol() {
    std::string baseMangled =
        ((Declarator *)getPointerOf())->mangleAndPrefixSymbol();
    return baseMangled.substr(0, strlen("T")) + "p" +
           baseMangled.substr(strlen("T") /* to end */);
}

const ASTNode * PointerDeclarator::getBase() const {
    return ((Declarator *)getPointerOf())->getBase();
}

ASTNode * PointerDeclarator::under() const {
    return (Declarator *)getPointerOf();
}

void PointerDeclarator::propagateScope(Scope * _scope) {
    scope = _scope;
    ((Declarator *)getPointerOf())->propagateScope(_scope);
}
//
//

// ~~~~~ RefDeclarator ~~~~~

RefDeclarator::RefDeclarator() : refOf(nullptr) { nodeKind = REF_DECLARATOR; }

RefDeclarator::RefDeclarator(ASTNode * _refOf) : refOf(_refOf) {
    nodeKind = REF_DECLARATOR;
    // a ref anywhere in the chain of different declarator types implies
    // that the base type does not need to be complete in order for the total
    // declarator to be valid in any context
    setRefOf(_refOf);
    ((Declarator *)getBase())->setFlag(IMPLIES_COMPLETE, false);
}

ASTNode * RefDeclarator::getRefOf() const { return refOf; }
void RefDeclarator::setRefOf(ASTNode * _refOf) {
    refOf = _refOf;
    refOf->parent = this;
    refOf->replace = rpget<replacementPolicy_RefDeclarator_RefOf>();
}

// Node interface
void RefDeclarator::analyze(bool force) {
    HANDLE_FORCE();
    getRefOf()->analyze();
    setFlag(ANALYZED, true);
}

RefDeclarator::~RefDeclarator() {
    BJOU_DEBUG_ASSERT(refOf);
    delete refOf;
}

void RefDeclarator::addSymbols(Scope * _scope) {
    setScope(_scope);
    getRefOf()->addSymbols(_scope);
}

void RefDeclarator::unwrap(std::vector<ASTNode *> & terminals) {
    getRefOf()->unwrap(terminals);
}

ASTNode * RefDeclarator::clone() {
    RefDeclarator * c = DeclaratorClone(this);

    // if (c->getRefOf())
    c->setRefOf(c->getRefOf()->clone());

    return c;
}

void RefDeclarator::desugar() { getRefOf()->desugar(); }

const Type * RefDeclarator::getType() {
    Declarator * refOf = (Declarator *)getRefOf();
    return RefType::get(refOf->getType());
}
//

// Declarator interface
std::string RefDeclarator::mangleSymbol() {
    return "r" + ((Declarator *)getRefOf())->mangleSymbol();
}

std::string RefDeclarator::mangleAndPrefixSymbol() {
    std::string baseMangled =
        ((Declarator *)getRefOf())->mangleAndPrefixSymbol();
    return baseMangled.substr(0, strlen("T")) + "r" +
           baseMangled.substr(strlen("T") /* to end */);
}

const ASTNode * RefDeclarator::getBase() const {
    return ((Declarator *)getRefOf())->getBase();
}

ASTNode * RefDeclarator::under() const { return (Declarator *)getRefOf(); }

void RefDeclarator::propagateScope(Scope * _scope) {
    scope = _scope;
    ((Declarator *)getRefOf())->propagateScope(_scope);
}
//

// ~~~~~ MaybeDeclarator ~~~~~

MaybeDeclarator::MaybeDeclarator() : maybeOf(nullptr) {
    nodeKind = MAYBE_DECLARATOR;
}

MaybeDeclarator::MaybeDeclarator(ASTNode * _maybeOf) : maybeOf(_maybeOf) {
    nodeKind = MAYBE_DECLARATOR;
    setMaybeOf(_maybeOf);
}

ASTNode * MaybeDeclarator::getMaybeOf() const { return maybeOf; }
void MaybeDeclarator::setMaybeOf(ASTNode * _maybeOf) {
    maybeOf = _maybeOf;
    maybeOf->parent = this;
    maybeOf->replace = rpget<replacementPolicy_MaybeDeclarator_MaybeOf>();
}

// Node interface
void MaybeDeclarator::analyze(bool force) {
    HANDLE_FORCE();
    getMaybeOf()->analyze();
    setFlag(ANALYZED, true);
}

MaybeDeclarator::~MaybeDeclarator() {
    BJOU_DEBUG_ASSERT(maybeOf);
    delete maybeOf;
}

void MaybeDeclarator::addSymbols(Scope * _scope) {
    setScope(_scope);
    getMaybeOf()->addSymbols(_scope);
}

void MaybeDeclarator::unwrap(std::vector<ASTNode *> & terminals) {
    getMaybeOf()->unwrap(terminals);
}

ASTNode * MaybeDeclarator::clone() {
    MaybeDeclarator * c = DeclaratorClone(this);

    // if (c->maybeOfOf())
    c->setMaybeOf(c->getMaybeOf()->clone());

    return c;
}

void MaybeDeclarator::desugar() { getMaybeOf()->desugar(); }

const Type * MaybeDeclarator::getType() {
    Declarator * maybeOf = (Declarator *)getMaybeOf();
    BJOU_DEBUG_ASSERT(false);
    // return new MaybeType(maybeOf->getType());
    return nullptr;
}
//

// Declarator interface
std::string MaybeDeclarator::mangleSymbol() {
    return "m" + ((Declarator *)getMaybeOf())->mangleSymbol();
}

std::string MaybeDeclarator::mangleAndPrefixSymbol() {
    std::string baseMangled =
        ((Declarator *)getMaybeOf())->mangleAndPrefixSymbol();
    return baseMangled.substr(0, strlen("T")) + "m" +
           baseMangled.substr(strlen("T") /* to end */);
}

const ASTNode * MaybeDeclarator::getBase() const {
    return ((Declarator *)getMaybeOf())->getBase();
}

ASTNode * MaybeDeclarator::under() const { return (Declarator *)getMaybeOf(); }

void MaybeDeclarator::propagateScope(Scope * _scope) {
    scope = _scope;
    ((Declarator *)getMaybeOf())->propagateScope(_scope);
}
//

// ~~~~~ TupleDeclarator ~~~~~

TupleDeclarator::TupleDeclarator() : subDeclarators({}) {
    nodeKind = TUPLE_DECLARATOR;
}

std::vector<ASTNode *> & TupleDeclarator::getSubDeclarators() {
    return subDeclarators;
}
void TupleDeclarator::setSubDeclarators(
    std::vector<ASTNode *> _subDeclarators) {
    subDeclarators = _subDeclarators;
}
void TupleDeclarator::addSubDeclarator(ASTNode * _subDeclarator) {
    _subDeclarator->parent = this;
    _subDeclarator->replace =
        rpget<replacementPolicy_TupleDeclarator_subDeclarator>();
    subDeclarators.push_back(_subDeclarator);
}

// Node interface
void TupleDeclarator::analyze(bool force) {
    HANDLE_FORCE();
    for (ASTNode * sd : getSubDeclarators())
        ((Declarator *)sd)->analyze();
    setFlag(ANALYZED, true);
}

TupleDeclarator::~TupleDeclarator() {
    for (ASTNode * sd : getSubDeclarators())
        delete sd;
}

void TupleDeclarator::addSymbols(Scope * _scope) {
    setScope(_scope);
    for (ASTNode * sd : getSubDeclarators())
        sd->addSymbols(_scope);
}

void TupleDeclarator::unwrap(std::vector<ASTNode *> & terminals) {
    for (ASTNode * sd : getSubDeclarators())
        sd->unwrap(terminals);
}

ASTNode * TupleDeclarator::clone() {
    TupleDeclarator * c = DeclaratorClone(this);
    std::vector<ASTNode *> & my_subDeclarators = c->getSubDeclarators();
    std::vector<ASTNode *> declarators = my_subDeclarators;

    my_subDeclarators.clear();
    for (ASTNode * d : declarators)
        c->addSubDeclarator(d->clone());

    return c;
}

void TupleDeclarator::desugar() {
    for (ASTNode * decl : getSubDeclarators())
        decl->desugar();
}

const Type * TupleDeclarator::getType() {
    std::vector<ASTNode *> & subDeclarators = getSubDeclarators();
    std::vector<const Type *> types;

    std::transform(subDeclarators.begin(), subDeclarators.end(),
                   std::back_inserter(types), [](ASTNode * declarator) {
                       return ((Declarator *)declarator)->getType();
                   });

    return TupleType::get(types);
}

//

// Declarator interface
std::string TupleDeclarator::mangleSymbol() {
    std::string mangled = "__bjou_tuple_";
    std::vector<ASTNode *> & subDeclarators = getSubDeclarators();
    mangled += std::to_string(subDeclarators.size());
    for (ASTNode * sd : subDeclarators)
        mangled += ((Declarator *)sd)->mangleAndPrefixSymbol();

    return mangled;
}

std::string TupleDeclarator::mangleAndPrefixSymbol() { return mangleSymbol(); }

const ASTNode * TupleDeclarator::getBase() const { return this; }

void TupleDeclarator::propagateScope(Scope * _scope) {
    scope = _scope;
    for (ASTNode * sd : getSubDeclarators())
        ((Declarator *)sd)->propagateScope(_scope);
}
//

// ~~~~~ ProcedureDeclarator ~~~~~

ProcedureDeclarator::ProcedureDeclarator()
    : paramDeclarators({}), retDeclarator(nullptr) {
    nodeKind = PROCEDURE_DECLARATOR;
}

std::vector<ASTNode *> & ProcedureDeclarator::getParamDeclarators() {
    return paramDeclarators;
}
void ProcedureDeclarator::setParamDeclarators(
    std::vector<ASTNode *> _paramDeclarators) {
    paramDeclarators = _paramDeclarators;
}
void ProcedureDeclarator::addParamDeclarator(ASTNode * _paramDeclarator) {
    _paramDeclarator->parent = this;
    _paramDeclarator->replace =
        rpget<replacementPolicy_ProcedureDeclarator_ParamDeclarators>();
    paramDeclarators.push_back(_paramDeclarator);
}

ASTNode * ProcedureDeclarator::getRetDeclarator() const {
    return retDeclarator;
}
void ProcedureDeclarator::setRetDeclarator(ASTNode * _retDeclarator) {
    retDeclarator = _retDeclarator;
    retDeclarator->parent = this;
    retDeclarator->replace =
        rpget<replacementPolicy_ProcedureDeclarator_RetDeclarator>();
}

// Node interface
void ProcedureDeclarator::analyze(bool force) {
    HANDLE_FORCE();
    for (ASTNode * pd : getParamDeclarators()) {
        ((Declarator *)pd)->analyze();
        const Type * pd_t = pd->getType();
        if (pd_t->isStruct() && ((StructType *)pd_t)->isAbstract)
            errorl(pd->getContext(),
                   "Can't use '" + pd_t->getDemangledName() +
                       "' as a parameter type because it is an abstract type.");
    }
    ((Declarator *)getRetDeclarator())->analyze();
    const Type * rd_t = getRetDeclarator()->getType();
    if (rd_t->isStruct() && ((StructType *)rd_t)->isAbstract)
        errorl(getRetDeclarator()->getContext(),
               "Can't use '" + rd_t->getDemangledName() +
                   "' as a return type because it is an abstract type.");
    setFlag(ANALYZED, true);
}

ProcedureDeclarator::~ProcedureDeclarator() {
    for (ASTNode * pd : paramDeclarators)
        delete pd;
    BJOU_DEBUG_ASSERT(retDeclarator);
    delete retDeclarator;
}

void ProcedureDeclarator::addSymbols(Scope * _scope) {
    setScope(_scope);
    for (ASTNode * pd : paramDeclarators)
        pd->addSymbols(_scope);
    BJOU_DEBUG_ASSERT(retDeclarator);
    retDeclarator->addSymbols(_scope);
}

void ProcedureDeclarator::unwrap(std::vector<ASTNode *> & terminals) {
    for (ASTNode * pd : getParamDeclarators())
        pd->unwrap(terminals);
    getRetDeclarator()->unwrap(terminals);
}

ASTNode * ProcedureDeclarator::clone() {
    ProcedureDeclarator * c = DeclaratorClone(this);
    std::vector<ASTNode *> & my_paramDeclarators = c->getParamDeclarators();
    std::vector<ASTNode *> declarators = my_paramDeclarators;

    my_paramDeclarators.clear();
    for (ASTNode * d : declarators)
        c->addParamDeclarator(d->clone());

    // if (c->getRetDeclarator())
    c->setRetDeclarator(c->getRetDeclarator()->clone());
    return c;
}

void ProcedureDeclarator::desugar() {
    for (ASTNode * p : getParamDeclarators())
        p->desugar();
    getRetDeclarator()->desugar();
}

const Type * ProcedureDeclarator::getType() {
    std::vector<ASTNode *> & paramDeclarators = getParamDeclarators();
    std::vector<const Type *> paramTypes;
    std::transform(paramDeclarators.begin(), paramDeclarators.end(),
                   std::back_inserter(paramTypes), [](ASTNode * declarator) {
                       return ((Declarator *)declarator)->getType();
                   });
    const Type * retType = ((Declarator *)getRetDeclarator())->getType();
    bool isVararg = getFlag(IS_VARARG);

    return ProcedureType::get(paramTypes, retType, isVararg);
}
//

// Declarator interface
std::string ProcedureDeclarator::mangleSymbol() {
    std::string mangled = "PA";
    std::vector<ASTNode *> & subDeclarators = getParamDeclarators();
    mangled += std::to_string(subDeclarators.size());
    for (ASTNode * sd : subDeclarators)
        mangled += ((Declarator *)sd)->mangleAndPrefixSymbol();
    ASTNode * retDeclarator = getRetDeclarator();
    mangled += "R" + ((Declarator *)retDeclarator)->mangleAndPrefixSymbol();

    return mangled;
}

std::string ProcedureDeclarator::mangleAndPrefixSymbol() {
    return mangleSymbol();
}

const ASTNode * ProcedureDeclarator::getBase() const { return this; }

void ProcedureDeclarator::propagateScope(Scope * _scope) {
    scope = _scope;
    for (ASTNode * pd : getParamDeclarators())
        ((Declarator *)pd)->propagateScope(_scope);
    ((Declarator *)getRetDeclarator())->propagateScope(_scope);
}
//

// ~~~~~ PlaceholderDeclarator ~~~~~

PlaceholderDeclarator::PlaceholderDeclarator() {
    nodeKind = PLACEHOLDER_DECLARATOR;
}

// Node interface
void PlaceholderDeclarator::analyze(bool force) {
    HANDLE_FORCE();

    BJOU_DEBUG_ASSERT(false && "PlaceholderDeclarator::analyze()");

    setFlag(ANALYZED, true);
}

PlaceholderDeclarator::~PlaceholderDeclarator() {}

void PlaceholderDeclarator::addSymbols(Scope * _scope) { setScope(_scope); }

void PlaceholderDeclarator::unwrap(std::vector<ASTNode *> & terminals) {
    terminals.push_back(this);
}

ASTNode * PlaceholderDeclarator::clone() { return DeclaratorClone(this); }

void PlaceholderDeclarator::desugar() {}

const Type * PlaceholderDeclarator::getType() { return PlaceholderType::get(); }
//

// Declarator interface
std::string PlaceholderDeclarator::mangleSymbol() { return "_"; }

std::string PlaceholderDeclarator::mangleAndPrefixSymbol() { return "_"; }

const ASTNode * PlaceholderDeclarator::getBase() const { return this; }
//

// ~~~~~ Constant ~~~~~

Constant::Constant()
    : name({}), mangledName({}), typeDeclarator(nullptr),
      initialization(nullptr) {
    nodeKind = CONSTANT;
}

std::string & Constant::getName() { return name; }
void Constant::setName(std::string _name) { name = _name; }

std::string & Constant::getMangledName() { return mangledName; }
void Constant::setMangledName(std::string _mangledName) {
    mangledName = _mangledName;
}

ASTNode * Constant::getTypeDeclarator() const { return typeDeclarator; }
void Constant::setTypeDeclarator(ASTNode * _declarator) {
    typeDeclarator = _declarator;
    typeDeclarator->parent = this;
    typeDeclarator->replace =
        rpget<replacementPolicy_Constant_TypeDeclarator>();
}

ASTNode * Constant::getInitialization() const { return initialization; }
void Constant::setInitialization(ASTNode * _initialization) {
    initialization = _initialization;
    initialization->parent = this;
    initialization->replace =
        rpget<replacementPolicy_Constant_Initialization>();
}

// Node interface
void Constant::analyze(bool force) {
    HANDLE_FORCE();

    if (getTypeDeclarator()) {
        std::stack<const Type *> & lValStack = compilation->frontEnd.lValStack;

        const Type * t = getTypeDeclarator()->getType(); // @leak
        lValStack.push(t);
        if (!conv(t, getInitialization()->getType()))
            errorl(
                getInitialization()->getContext(),
                "Can't create '" + t->getDemangledName() + "'" + " constant " +
                    "'" + getName() + "' with expression of type '" +
                    getInitialization()->getType()->getDemangledName() + "'.");
        lValStack.pop();
    } else {
        setTypeDeclarator(
            getInitialization()->getType()->getGenericDeclarator());
        getTypeDeclarator()->addSymbols(getScope());
    }

    if (!((Expression *)getInitialization())->isConstant())
        errorl(getInitialization()->getContext(),
               "Value for '" + getName() + "' is not constant.");
    if (!getInitialization()->getType()->isProcedure()) {
        ASTNode * folded = ((Expression *)getInitialization())->eval().toExpr();
        folded->addSymbols(getScope());
        folded->analyze();
        // @leak?
        setInitialization(folded);
    }

    setFlag(ANALYZED, true);
}

void Constant::addSymbols(Scope * _scope) {
    setScope(_scope);
    if (!getFlag(IS_TYPE_MEMBER)) {
        _Symbol<Constant> * symbol = new _Symbol<Constant>(getName(), this);
        setMangledName(symbol->mangledString(_scope));
        _scope->addSymbol(symbol, &getNameContext());
    }
    if (getTypeDeclarator())
        getTypeDeclarator()->addSymbols(_scope);
    getInitialization()->addSymbols(_scope);
}

void Constant::unwrap(std::vector<ASTNode *> & terminals) {
    getInitialization()->unwrap(terminals);
}

ASTNode * Constant::clone() {
    Constant * c = new Constant(*this);

    if (c->getTypeDeclarator())
        c->setTypeDeclarator(c->getTypeDeclarator()->clone());
    // if (c->getInitialization())
    c->setInitialization(c->getInitialization()->clone());

    return c;
}

void Constant::desugar() {
    if (getTypeDeclarator())
        getTypeDeclarator()->desugar();
    if (getInitialization())
        getInitialization()->desugar();
}

bool Constant::isStatement() const { return true; }

Constant::~Constant() {
    if (typeDeclarator)
        delete typeDeclarator;
    if (initialization)
        delete initialization;
}
//

const Type * Constant::getType() {
    analyze();
    return ((Expression *)getInitialization())->getType();
}

// ~~~~~ VariableDeclaration ~~~~~

VariableDeclaration::VariableDeclaration()
    : name({}), mangledName({}), typeDeclarator(nullptr),
      initialization(nullptr) {
    nodeKind = VARIABLE_DECLARATION;
}

std::string & VariableDeclaration::getName() { return name; }
void VariableDeclaration::setName(std::string _name) { name = _name; }

std::string & VariableDeclaration::getMangledName() { return mangledName; }
void VariableDeclaration::setMangledName(std::string _mangledName) {
    mangledName = _mangledName;
}

ASTNode * VariableDeclaration::getTypeDeclarator() const {
    return typeDeclarator;
}
void VariableDeclaration::setTypeDeclarator(ASTNode * _declarator) {
    typeDeclarator = _declarator;
    typeDeclarator->parent = this;
    typeDeclarator->replace =
        rpget<replacementPolicy_VariableDeclaration_TypeDeclarator>();
}

ASTNode * VariableDeclaration::getInitialization() const {
    return initialization;
}
void VariableDeclaration::setInitialization(ASTNode * _initialization) {
    initialization = _initialization;
    initialization->parent = this;
    initialization->replace =
        rpget<replacementPolicy_VariableDeclaration_Initialization>();
}

// Node interface
void VariableDeclaration::analyze(bool force) {
    HANDLE_FORCE();
    setFlag(ANALYZED, true); // prevents endless recursion if initialization
                             // references self (i.e. 'f : f32 = f')

    const Type * my_t = nullptr;
    Symbol * sym = nullptr;

    if (!getFlag(IS_TYPE_MEMBER)) {
        Maybe<Symbol *> m_sym = getScope()->getSymbol(
            getScope(), getMangledName(), &getNameContext(), true, true, false);
        m_sym.assignTo(sym);
        BJOU_DEBUG_ASSERT(sym);
    }

    if (getInitialization()) {
        const Type * init_t = nullptr;

        sym->initializedInScopes.insert(getScope());

        if (getFlag(IS_TYPE_MEMBER))
            errorl(getInitialization()->getContext(),
                   "Type member variables cannot be initialized.", true,
                   "did you mean to make '" + getName() + "' a constant?");

        if (getTypeDeclarator()) {
            const Type * decl_t = getTypeDeclarator()->getType();

            my_t = decl_t;

            decl_t = decl_t->unRef();

            std::stack<const Type *> & lValStack =
                compilation->frontEnd.lValStack;

            lValStack.push(decl_t);

            init_t = getInitialization()->getType();

            if (my_t->isRef() &&
                !((Expression *)getInitialization())->canBeLVal())
                errorl(getInitialization()->getContext(),
                       "Reference initializer can't be used as a reference.");

            if (!conv(decl_t, init_t))
                errorl(getInitialization()->getContext(),
                       "Can't initialize " + decl_t->getDemangledName() + " '" +
                           getName() + "' with expression of type '" +
                           init_t->getDemangledName() + "'.");

            if (!equal(decl_t, init_t))
                emplaceConversion((Expression *)getInitialization(), decl_t);

            lValStack.pop();
        } else {
            my_t = init_t = getInitialization()->getType();
            setTypeDeclarator(init_t->getGenericDeclarator());
            getTypeDeclarator()->addSymbols(getScope());
        }
        if ((getScope()->nspace || !getScope()->parent) &&
            !((Expression *)getInitialization())->isConstant())
            errorl(getInitialization()->getContext(),
                   "Global variable initializations must be compile time "
                   "constants.");
    } else {
        my_t = getTypeDeclarator()->getType();

        if (!getFlag(IS_PROC_PARAM) && !getFlag(IS_TYPE_MEMBER) &&
            getTypeDeclarator()->getType()->isRef()) {

            errorl(getContext(),
                   "'" + getName() +
                       "' is a reference and must be initialized.");
        }
    }

    BJOU_DEBUG_ASSERT(getTypeDeclarator() &&
                      "No typeDeclarator in VariableDeclaration");

    if (getTypeDeclarator()->nodeKind == ARRAY_DECLARATOR) {
        if (sym) {
            // arrays don't require initialization before reference
            sym->initializedInScopes.insert(getScope());
        }

        ArrayDeclarator * array_decl = (ArrayDeclarator *)getTypeDeclarator();
        if (array_decl->size == -1) {
            Context errContext;
            std::string errMsg;
            if (array_decl->getExpression()) {
                errContext = array_decl->getExpression()->getContext();
                errMsg = "Array lengths must be compile-time constants.";
            } else {
                errContext = array_decl->getContext();
                errMsg = "Static array declarations must specify their length.";
            }
            errorl(errContext, errMsg, true,
                   "Note: For a dynamic array type, use '[...]'");
        }
    }

    BJOU_DEBUG_ASSERT(my_t);

    if (my_t->isStruct() && ((StructType *)my_t)->isAbstract)
        errorl(getTypeDeclarator()->getContext(),
               "Can't instantiate '" + my_t->getDemangledName() +
                   "', which is an abstract type.");
}

void VariableDeclaration::addSymbols(Scope * _scope) {
    setScope(_scope);
    if (!getFlag(IS_TYPE_MEMBER)) {
        _Symbol<VariableDeclaration> * symbol =
            new _Symbol<VariableDeclaration>(getName(), this);
        setMangledName(symbol->mangledString(_scope));
        _scope->addSymbol(symbol, &getNameContext());
    }
    if (getTypeDeclarator())
        getTypeDeclarator()->addSymbols(_scope);
    if (getInitialization())
        getInitialization()->addSymbols(_scope);
}

void VariableDeclaration::unwrap(std::vector<ASTNode *> & terminals) {
    if (getTypeDeclarator())
        getTypeDeclarator()->unwrap(terminals);
    if (getInitialization())
        getInitialization()->unwrap(terminals);
}

ASTNode * VariableDeclaration::clone() {
    VariableDeclaration * c = new VariableDeclaration(*this);

    if (c->getTypeDeclarator())
        c->setTypeDeclarator(c->getTypeDeclarator()->clone());
    if (c->getInitialization())
        c->setInitialization(c->getInitialization()->clone());

    return c;
}

void VariableDeclaration::desugar() {
    if (getTypeDeclarator())
        getTypeDeclarator()->desugar();
    if (getInitialization())
        getInitialization()->desugar();
}

VariableDeclaration::~VariableDeclaration() {
    if (typeDeclarator)
        delete typeDeclarator;
    if (initialization)
        delete initialization;
}
//

const Type * VariableDeclaration::getType() {
    if (!getTypeDeclarator())
        analyze();
    return getTypeDeclarator()->getType();
}

bool VariableDeclaration::isStatement() const { return true; }

// ~~~~~ Alias ~~~~~

Alias::Alias() : name({}), mangledName({}), declarator(nullptr) {
    nodeKind = ALIAS;
    setFlag(SYMBOL_OVERWRITE, true);
}

std::string & Alias::getName() { return name; }
void Alias::setName(std::string _name) { name = _name; }

std::string & Alias::getMangledName() { return mangledName; }
void Alias::setMangledName(std::string _mangledName) {
    mangledName = _mangledName;
}

ASTNode * Alias::getDeclarator() const { return declarator; }
void Alias::setDeclarator(ASTNode * _declarator) {
    declarator = _declarator;
    declarator->parent = this;
    declarator->replace = rpget<replacementPolicy_Alias_Declarator>();
}

// Node interface
void Alias::analyze(bool force) {
    HANDLE_FORCE();

    getDeclarator()->analyze();

    setFlag(ANALYZED, true);
}

void Alias::addSymbols(Scope * _scope) {
    setScope(_scope);
    
    getDeclarator()->addSymbols(_scope);

    setScope(_scope);
    _Symbol<Alias> * symbol = new _Symbol<Alias>(getName(), this);
    setMangledName(symbol->mangledString(_scope));

    // do this before adding symbol so that it can't self reference
    getDeclarator()->analyze();
    Type::alias(getMangledName(), getDeclarator()->getType());
    
    _scope->addSymbol(symbol, &getNameContext());
}

const Type * Alias::getType() {
    analyze();

    const Type * t = getTypeFromTable(getMangledName());

    BJOU_DEBUG_ASSERT(t);

    return t;
}

void Alias::unwrap(std::vector<ASTNode *> & terminals) {
    getDeclarator()->unwrap(terminals);
}

ASTNode * Alias::clone() {
    Alias * c = new Alias(*this);

    // if (c->getDeclarator())
    c->setDeclarator(c->getDeclarator()->clone());

    return c;
}

void Alias::desugar() { getDeclarator()->desugar(); }

Alias::~Alias() {
    BJOU_DEBUG_ASSERT(declarator);
    delete declarator;
}
//

// ~~~~~ Struct ~~~~~

Struct::Struct()
    : name({}), mangledName({}), extends(nullptr), memberVarDecls({}),
      interfaceImpls({}), inst(nullptr), n_interface_procs(0) {
    nodeKind = STRUCT;
}

std::string & Struct::getName() { return name; }
void Struct::setName(std::string _name) { name = _name; }

std::string & Struct::getMangledName() { return mangledName; }
void Struct::setMangledName(std::string _mangledName) {
    mangledName = _mangledName;
}

ASTNode * Struct::getExtends() const { return extends; }
void Struct::setExtends(ASTNode * _extends) {
    extends = _extends;
    extends->parent = this;
    extends->replace = rpget<replacementPolicy_Struct_Extends>();
}

std::vector<ASTNode *> & Struct::getMemberVarDecls() { return memberVarDecls; }
void Struct::setMemberVarDecls(std::vector<ASTNode *> _memberVarDecls) {
    memberVarDecls = _memberVarDecls;
}
void Struct::addMemberVarDecl(ASTNode * _memberVarDecl) {
    _memberVarDecl->setFlag(VariableDeclaration::IS_TYPE_MEMBER, true);
    _memberVarDecl->parent = this;
    _memberVarDecl->replace = rpget<replacementPolicy_Struct_MemberVarDecl>();
    memberVarDecls.push_back(_memberVarDecl);
}

std::vector<ASTNode *> & Struct::getConstantDecls() { return constantDecls; }
void Struct::setConstantDecls(std::vector<ASTNode *> _constantDecls) {
    constantDecls = _constantDecls;
}
void Struct::addConstantDecl(ASTNode * _constantDecl) {
    _constantDecl->setFlag(Constant::IS_TYPE_MEMBER, true);
    _constantDecl->parent = this;
    _constantDecl->replace = rpget<replacementPolicy_Struct_ConstantDecl>();
    constantDecls.push_back(_constantDecl);
}

std::vector<ASTNode *> & Struct::getMemberProcs() { return memberProcs; }
void Struct::setMemberProcs(std::vector<ASTNode *> _memberProcs) {
    memberProcs = _memberProcs;
}
void Struct::addMemberProc(ASTNode * memberProc) {
    memberProc->parent = this;
    memberProc->setFlag(Procedure::IS_TYPE_MEMBER, true);
    memberProc->parent = this;
    memberProc->replace = rpget<replacementPolicy_Struct_MemberProc>();
    memberProcs.push_back(memberProc);
}

std::vector<ASTNode *> & Struct::getMemberTemplateProcs() {
    return memberTemplateProcs;
}
void Struct::setMemberTemplateProcs(
    std::vector<ASTNode *> _memberTemplateProcs) {
    memberTemplateProcs = _memberTemplateProcs;
}
void Struct::addMemberTemplateProc(ASTNode * memberTemplateProc) {
    memberTemplateProc->parent = this;
    memberTemplateProc->setFlag(TemplateProc::IS_TYPE_MEMBER, true);
    memberTemplateProc->parent = this;
    memberTemplateProc->replace =
        rpget<replacementPolicy_Struct_MemberTemplateProc>();
    memberTemplateProcs.push_back(memberTemplateProc);
}

std::vector<ASTNode *> & Struct::getInterfaceImpls() { return interfaceImpls; }
void Struct::setInterfaceImpls(std::vector<ASTNode *> _interfaceImpls) {
    interfaceImpls = _interfaceImpls;
}
void Struct::addInterfaceImpl(ASTNode * _interfaceImpl) {
    _interfaceImpl->parent = this;
    _interfaceImpl->replace = rpget<replacementPolicy_Struct_InterfaceImpl>();
    interfaceImpls.push_back(_interfaceImpl);
}

std::vector<ASTNode *> Struct::getAllInterfaceImplsSorted() {
    // std::map<std::string, ASTNode*> sorted_map;
    std::map<unsigned int, ASTNode *> sorted_map;
    std::vector<ASTNode *> sorted_vec;

    sorted_vec.reserve(n_interface_procs);

    if (getExtends()) {
        StructType * extends_t = (StructType *)getExtends()->getType();
        for (ASTNode * _impl :
             extends_t->_struct->getAllInterfaceImplsSorted()) {
            InterfaceImplementation * impl = (InterfaceImplementation *)_impl;
            Identifier * ident = (Identifier *)impl->getIdentifier();
            // sorted_map[mangledIdentifier(ident)] = _impl;
            sorted_map[compilation->frontEnd.getInterfaceSortKey(
                mangledIdentifier(ident))] = _impl;
        }
    }

    for (ASTNode * _impl : getInterfaceImpls()) {
        InterfaceImplementation * impl = (InterfaceImplementation *)_impl;
        Identifier * ident = (Identifier *)impl->getIdentifier();
        // sorted_map[mangledIdentifier(ident)] = _impl;
        sorted_map[compilation->frontEnd.getInterfaceSortKey(
            mangledIdentifier(ident))] = _impl;
    }

    for (auto & impl : sorted_map)
        sorted_vec.push_back(impl.second);

    return sorted_vec;
}

// Node interface
void Struct::analyze(bool force) {
    HANDLE_FORCE();

    std::unordered_map<std::string, ASTNode *> declaredMembers;

    if (getExtends()) {
        Struct * e = ((StructType *)getExtends()->getType())->_struct;
        e->analyze(force);
        while (e) {
            for (ASTNode * _v : e->getMemberVarDecls())
                declaredMembers[((VariableDeclaration *)_v)->getName()] = _v;
            for (ASTNode * _c : e->getConstantDecls())
                declaredMembers[((Constant *)_c)->getName()] = _c;

            if (e->getExtends())
                e = ((StructType *)e->getExtends()->getType())->_struct;
            else
                e = nullptr;
        }
    }

    for (ASTNode * _mem : getMemberVarDecls()) {
        VariableDeclaration * mem = (VariableDeclaration *)_mem;

        if (declaredMembers.find(mem->getName()) != declaredMembers.end()) {
            errorl(mem->getNameContext(),
                   "Redefinition of member '" + mem->getName() + "'.", false);
            errorl(declaredMembers[mem->getName()]->getNameContext(),
                   "'" + mem->getName() + "' previously defined here");
        }

        mem->analyze(force);
        if (mem->getInitialization())
            errorl(mem->getInitialization()->getContext(),
                   "Type member variables may not be default initialized.");

        declaredMembers[mem->getName()] = _mem;
    }

    for (ASTNode * _constant : getConstantDecls()) {
        Constant * constant = (Constant *)_constant;

        if (declaredMembers.find(constant->getName()) !=
            declaredMembers.end()) {
            errorl(constant->getNameContext(),
                   "Redefinition of member '" + constant->getName() + "'.",
                   false);
            errorl(declaredMembers[constant->getName()]->getNameContext(),
                   "'" + constant->getName() + "' previously defined here");
        }

        constant->analyze(force);

        declaredMembers[constant->getName()] = _constant;
    }

    for (ASTNode * _proc : getMemberProcs()) {
        Procedure * proc = (Procedure *)_proc;

        if (declaredMembers.find(proc->getName()) != declaredMembers.end()) {
            errorl(proc->getNameContext(),
                   "Redefinition of member '" + proc->getName() + "'.", false);
            errorl(declaredMembers[proc->getName()]->getNameContext(),
                   "'" + proc->getName() + "' previously defined here");
        }

        proc->analyze(force);
        // compilation->frontEnd.deferredAST.push_back(proc);
    }

    for (ASTNode * _template_proc : getMemberTemplateProcs()) {
        TemplateProc * template_proc = (TemplateProc *)_template_proc;
        Procedure * proc = (Procedure *)template_proc->getTemplate();

        if (declaredMembers.find(proc->getName()) != declaredMembers.end()) {
            errorl(proc->getNameContext(),
                   "Redefinition of member '" + proc->getName() + "'.", false);
            errorl(declaredMembers[proc->getName()]->getNameContext(),
                   "'" + proc->getName() + "' previously defined here");
        }
    }

    if (getExtends()) {
        ASTNode * extends = getExtends();
        const Type * e_t = extends->getType();
        if (e_t->isStruct()) {
            const StructType * e_s_t = (const StructType *)e_t;
            Struct * extendsStruct = (Struct *)e_s_t->_struct;
            std::vector<std::string> implNames;
            for (ASTNode * _impl : getInterfaceImpls()) {
                InterfaceImplementation * impl =
                    (InterfaceImplementation *)_impl;
                implNames.push_back(
                    mangledIdentifier((Identifier *)impl->getIdentifier()));
            }
            for (ASTNode * _e_impl : extendsStruct->getInterfaceImpls()) {
                InterfaceImplementation * e_impl =
                    (InterfaceImplementation *)_e_impl;
                if (e_impl->getFlag(
                        InterfaceImplementation::PUNT_TO_EXTENSION)) {
                    if (std::find(implNames.begin(), implNames.end(),
                                  mangledIdentifier(
                                      (Identifier *)e_impl->getIdentifier())) ==
                        implNames.end()) {
                        errorl(this->getNameContext(),
                               "Type '" + demangledString(getMangledName()) +
                                   "' extends abstract type '" +
                                   e_s_t->getDemangledName() +
                                   "', but does not implement required "
                                   "interface '" +
                                   demangledString(mangledIdentifier(
                                       (Identifier *)e_impl->getIdentifier())) +
                                   "'.",
                               false);
                        errorl(e_impl->getContext(), "Required:");
                    }
                }
            }
        } else
            errorl(extends->getContext(),
                   "Can't extend a type that is not a struct."); // @bad error
                                                                 // message
    }

    for (ASTNode * _impl : getInterfaceImpls()) {
        InterfaceImplementation * impl = (InterfaceImplementation *)_impl;
        impl->analyze(force);
    }

    for (ASTNode * _impl : getAllInterfaceImplsSorted()) {
        InterfaceImplementation * impl = (InterfaceImplementation *)_impl;
        n_interface_procs += impl->getInterfaceDef()->totalProcs();
    }

    if (n_interface_procs > compilation->max_interface_procs)
        compilation->max_interface_procs = n_interface_procs;

    setFlag(ANALYZED, true);
}

void Struct::preDeclare(Scope * _scope) {
    setScope(_scope);
    _Symbol<Struct> * symbol = new _Symbol<Struct>(getName(), this, inst);
    setMangledName(symbol->mangledString(_scope));
    _scope->addSymbol(symbol, &getNameContext());

    if (getMangledName() == "typeinfo")
        compilation->frontEnd.typeinfo_struct = this;

    // @refactor? should this really be here?
    StructType::get(getMangledName(), this, (TemplateInstantiation *)inst);
}

void Struct::addSymbols(Scope * _scope) {
    /*
    setScope(_scope);
    _Symbol<Struct>* symbol = new _Symbol<Struct>(getName(), this, inst);
    setMangledName(symbol->mangledString(_scope));
    _scope->addSymbol(symbol, &getNameContext());

    // @refactor? should this really be here?
    compilation->frontEnd.typeTable[getMangledName()] = new
    StructType(getMangledName(), this, (TemplateInstantiation*)inst);
    */

    if (extends)
        extends->addSymbols(_scope);
    for (ASTNode * mem : getMemberVarDecls())
        mem->addSymbols(_scope);
    for (ASTNode * constant : getConstantDecls())
        constant->addSymbols(_scope);
    for (ASTNode * proc : getMemberProcs())
        proc->addSymbols(_scope);
    for (ASTNode * tproc : getMemberTemplateProcs())
        tproc->addSymbols(_scope);
    for (ASTNode * _impl : getAllInterfaceImplsSorted()) {
        InterfaceImplementation * impl = (InterfaceImplementation *)_impl;
        if (impl->parent == this) {
            if (impl->getFlag(InterfaceImplementation::PUNT_TO_EXTENSION))
                createProcSetsForPuntedInterfaceImpl(this, impl);
            impl->addSymbols(_scope);
        } else {
            createProcSetForInheritedInterfaceImpl(this, impl);
        }
    }
}

void Struct::unwrap(std::vector<ASTNode *> & terminals) {
    if (getExtends())
        getExtends()->unwrap(terminals);
    for (ASTNode * mem : getMemberVarDecls())
        mem->unwrap(terminals);
    for (ASTNode * constant : getConstantDecls())
        constant->unwrap(terminals);
    for (ASTNode * proc : getMemberProcs())
        proc->unwrap(terminals);
    for (ASTNode * impl : getInterfaceImpls())
        impl->unwrap(terminals);
}

ASTNode * Struct::clone() {
    Struct * c = new Struct(*this);

    std::vector<ASTNode *> & my_memberVarDecls = c->getMemberVarDecls();
    std::vector<ASTNode *> vars = my_memberVarDecls;
    std::vector<ASTNode *> & my_constantDecls = c->getConstantDecls();
    std::vector<ASTNode *> constants = my_constantDecls;
    std::vector<ASTNode *> & my_memberProcs = c->getMemberProcs();
    std::vector<ASTNode *> procs = my_memberProcs;
    std::vector<ASTNode *> & my_interfaceImpls = c->getInterfaceImpls();
    std::vector<ASTNode *> impls = my_interfaceImpls;

    if (c->getExtends())
        c->setExtends(c->getExtends()->clone());

    my_memberVarDecls.clear();
    my_constantDecls.clear();
    my_memberProcs.clear();
    my_interfaceImpls.clear();

    for (ASTNode * v : vars)
        c->addMemberVarDecl(v->clone());
    for (ASTNode * co : constants)
        c->addConstantDecl(co->clone());
    for (ASTNode * p : procs)
        c->addMemberProc(p->clone());
    for (ASTNode * i : impls)
        c->addInterfaceImpl(i->clone());

    return c;
}

void Struct::desugar() {
    if (getExtends())
        getExtends()->desugar();
    for (ASTNode * mem : getMemberVarDecls())
        mem->desugar();
    for (ASTNode * constant : getConstantDecls())
        constant->desugar();
    for (ASTNode * proc : getMemberProcs())
        proc->desugar();
    for (ASTNode * impl : getInterfaceImpls())
        impl->desugar();
}

Struct::~Struct() {
    for (ASTNode * mvd : memberVarDecls)
        delete mvd;
    for (ASTNode * constant : constantDecls)
        delete constant;
    // ownership transferred to top of AST
    // for (ASTNode * proc : memberProcs)
    // delete proc;
    for (ASTNode * ii : interfaceImpls)
        delete ii;
}
//

const Type * Struct::getType() {
    // analyze(); // should not be necessary
    std::string mangled = getMangledName();

    return StructType::get(mangled);
}

// ~~~~~ InterfaceDef ~~~~~

InterfaceDef::InterfaceDef() : name({}), mangledName({}), procs({}) {
    nodeKind = INTERFACE_DEF;
}

std::string & InterfaceDef::getName() { return name; }
void InterfaceDef::setName(std::string _name) { name = _name; }

std::string & InterfaceDef::getMangledName() { return mangledName; }
void InterfaceDef::setMangledName(std::string _mangledName) {
    mangledName = _mangledName;
}

std::map<std::string, std::vector<ASTNode *>> & InterfaceDef::getProcs() {
    return procs;
}
void InterfaceDef::setProcs(
    std::map<std::string, std::vector<ASTNode *>> _procs) {
    procs = _procs;
}
void InterfaceDef::addProcs(std::string key, std::vector<ASTNode *> vals) {
    procs[key] = vals;
}
void InterfaceDef::addProc(std::string key, ASTNode * val) {
    val->parent = this;
    val->replace = rpget<replacementPolicy_InterfaceDef_Proc>();
    procs[key].push_back(val);
}

unsigned int InterfaceDef::totalProcs() {
    unsigned int total = 0;

    for (auto & procs : getProcs())
        total += (int)procs.second.size();

    return total;
}

// Node interface
void InterfaceDef::analyze(bool force) {
    HANDLE_FORCE();

    for (auto & procs : getProcs()) {
        std::unordered_map<Procedure *, const Type *> proc_types;
        for (ASTNode * _proc : procs.second) {
            Procedure * proc = (Procedure *)_proc;

            if (proc->getParamVarDeclarations().empty())
                errorl(proc->getNameContext(), "The first parameter of an "
                                               "interface procedure "
                                               "declaration must be 'this'.");
            else if (proc->getParamVarDeclarations()[0]->nodeKind !=
                     ASTNode::THIS)
                errorl(proc->getParamVarDeclarations()[0]->getContext(),
                       "The first parameter of an interface procedure "
                       "declaration must be 'this'.");

            proc_types[proc] = proc->Procedure::getType();
        }
        for (ASTNode * _proc : procs.second) {
            Procedure * proc = (Procedure *)_proc;
            for (auto & pt : proc_types) {
                if (proc != pt.first) {
                    ProcedureType * proc_type =
                        (ProcedureType *)proc_types[proc];
                    if (argMatch(proc_type, pt.second)) {
                        errorl(pt.first->getNameContext(),
                               "Interface procedure declaration matches "
                               "previous declaration.",
                               false);
                        errorl(proc->getNameContext(), "First declared here.");
                    }
                }
            }
        }
    }

    setFlag(ANALYZED, true);
}

void InterfaceDef::preDeclare(Scope * _scope) {
    setScope(_scope);
    _Symbol<InterfaceDef> * symbol = new _Symbol<InterfaceDef>(getName(), this);
    setMangledName(symbol->mangledString(_scope));
    _scope->addSymbol(symbol, &getNameContext());
}

void InterfaceDef::addSymbols(Scope * _scope) {
    /*
            setScope(_scope);
    _Symbol<InterfaceDef>* symbol = new _Symbol<InterfaceDef>(getName(), this);
    setMangledName(symbol->mangledString(_scope));
    _scope->addSymbol(symbol, &getNameContext());
            */

    for (auto & procs : getProcs()) {
        for (ASTNode * _proc : procs.second) {
            Procedure * proc = (Procedure *)_proc;

            // @bad
            for (ASTNode * _param : proc->getParamVarDeclarations()) {
                _param->setScope(_scope);

                if (_param->nodeKind == THIS)
                    continue;

                BJOU_DEBUG_ASSERT(_param->nodeKind == VARIABLE_DECLARATION);

                VariableDeclaration * param = (VariableDeclaration *)_param;

                if (param->getInitialization())
                    param->getInitialization()->addSymbols(_scope);
                if (param->getTypeDeclarator())
                    param->getTypeDeclarator()->addSymbols(_scope);
            }

            proc->getRetDeclarator()->addSymbols(_scope);
        }
    }
}

void InterfaceDef::unwrap(std::vector<ASTNode *> & terminals) {
    for (auto & procs : getProcs())
        for (ASTNode * proc : procs.second)
            proc->unwrap(terminals);
}

ASTNode * InterfaceDef::clone() {
    InterfaceDef * c = new InterfaceDef(*this);

    std::map<std::string, std::vector<ASTNode *>> & my_procs = c->getProcs();
    std::map<std::string, std::vector<ASTNode *>> _procs = my_procs;

    my_procs.clear();

    for (auto & it : _procs)
        for (ASTNode * p : it.second)
            c->addProc(it.first, p->clone());

    return c;
}

void InterfaceDef::desugar() {
    for (auto & procs : getProcs())
        for (ASTNode * proc : procs.second)
            proc->desugar();
}

InterfaceDef::~InterfaceDef() {
    for (auto & procs : getProcs())
        for (ASTNode * proc : procs.second)
            delete proc;
}
//

// ~~~~~ InterfaceImplementation ~~~~~

InterfaceImplementation::InterfaceImplementation()
    : identifier(nullptr), procs({}) {
    nodeKind = INTERFACE_IMPLEMENTATION;
}

ASTNode * InterfaceImplementation::getIdentifier() const { return identifier; }
void InterfaceImplementation::setIdentifier(ASTNode * _identifier) {
    identifier = _identifier;
    identifier->parent = this;
    identifier->replace =
        rpget<replacementPolicy_InterfaceImplementation_Identifier>();
}

std::map<std::string, std::vector<ASTNode *>> &
InterfaceImplementation::getProcs() {
    return procs;
}
void InterfaceImplementation::setProcs(
    std::map<std::string, std::vector<ASTNode *>> _procs) {
    BJOU_DEBUG_ASSERT(false);
    procs = _procs;
}
void InterfaceImplementation::addProcs(std::string key,
                                       std::vector<ASTNode *> vals) {
    BJOU_DEBUG_ASSERT(false);
    procs[key] = vals;
}
void InterfaceImplementation::addProc(std::string key, ASTNode * val) {
    val->parent = this;
    val->replace = rpget<replacementPolicy_InterfaceImplementation_Proc>();
    val->setFlag(Procedure::IS_TYPE_MEMBER, true);
    procs[key].push_back(val);
}

InterfaceDef * InterfaceImplementation::getInterfaceDef() {
    Identifier * id = (Identifier *)getIdentifier();
    Maybe<Symbol *> m_defSym =
        getScope()->getSymbol(getScope(), id, &getContext());
    Symbol * defSym = nullptr;
    m_defSym.assignTo(defSym);
    BJOU_DEBUG_ASSERT(defSym);

    std::string demangledIdentifier = demangledString(mangledIdentifier(id));

    if (!defSym->isInterface())
        errorl(id->getContext(),
               "'" + demangledIdentifier + "' is not an interface.");

    InterfaceDef * ifaceDef = (InterfaceDef *)defSym->node();
    BJOU_DEBUG_ASSERT(ifaceDef);
    return ifaceDef;
}

// Node interface
void InterfaceImplementation::analyze(bool force) {
    HANDLE_FORCE();

    InterfaceDef * ifaceDef = getInterfaceDef();
    Identifier * id = (Identifier *)getIdentifier();
    std::string demangledIdentifier = demangledString(mangledIdentifier(id));

    if (!getFlag(PUNT_TO_EXTENSION)) {
        for (auto & _defs : ifaceDef->getProcs()) {
            const std::string & procName = _defs.first;
            std::vector<ASTNode *> & defs = _defs.second;

            if (getProcs().count(procName) == 0) {
                std::vector<std::string> reqs = {
                    "'" + procName + "' requires implementations of type(s):"};
                for (ASTNode * _def : defs) {
                    Procedure * def = (Procedure *)_def;

                    const Type * parent_t = parent->getType();

                    const Type * placeholder_def_t =
                        (const Type *)def->getType();
                    const Type * def_t =
                        placeholder_def_t->replacePlaceholders(parent_t);

                    reqs.push_back(def_t->getDemangledName());
                    errorl(id->getContext(),
                           "'" +
                               demangledString(
                                   ((Struct *)parent)->getMangledName()) +
                               "' implementation for interface '" +
                               demangledIdentifier + "' does not satisfy '" +
                               procName + "'.",
                           true, reqs);
                }
            }

            std::set<ASTNode *> used;

            for (ASTNode * _def : defs) {
                Procedure * def = (Procedure *)_def;

                const Type * parent_t = parent->getType(); // @leak?

                const Type * placeholder_def_t = (const Type *)def->getType();
                const Type * def_t =
                    placeholder_def_t->replacePlaceholders(parent_t);

                bool found = false;
                std::vector<ASTNode *> & procImpls = getProcs()[procName];
                for (ASTNode * proc : procImpls) {
                    if (equal(def_t, proc->getType())) {
                        found = true;
                        used.insert(proc);

                        if (ifaceDef->getMangledName() == "idestroy" &&
                            procName == "destroy")
                            ((StructType *)parent_t)->idestroy_link =
                                (Procedure *)proc;

                        break;
                    }
                }
                if (!found)
                    errorl(id->getContext(),
                           "'" +
                               demangledString(
                                   ((Struct *)parent)->getMangledName()) +
                               "' implementation for interface '" +
                               demangledIdentifier + "' does not satisfy '" +
                               procName + "'.",
                           true,
                           "Missing implementation: " + procName + " " +
                               def_t->getDemangledName()); // @leak
            }

            for (auto & procs : getProcs()) {
                for (ASTNode * _proc : procs.second) {
                    if (!used.count(_proc)) {
                        Procedure * proc = (Procedure *)_proc;
                        errorl(proc->getNameContext(),
                               "Interface '" + demangledIdentifier +
                                   "' does not require a procedure named '" +
                                   proc->getName() + "' of type " +
                                   proc->getType()->getDemangledName() + ".");
                    }
                }
            }
        }

        for (auto & procs : getProcs())
            for (ASTNode * proc : procs.second)
                proc->analyze(force);
    }
    setFlag(ANALYZED, true);
}

void InterfaceImplementation::addSymbols(Scope * _scope) {
    setScope(_scope);

    // If the parent struct is not abstract, we will unset the punt flag so that
    // analysis will give errors about incompletion
    if (!((Struct *)parent)->getFlag(Struct::IS_ABSTRACT))
        setFlag(PUNT_TO_EXTENSION, false);

    if (getFlag(PUNT_TO_EXTENSION)) {
        InterfaceDef * ifaceDef = getInterfaceDef();

        for (auto & procs : ifaceDef->getProcs()) {
            for (ASTNode * _proc : procs.second) {
                Procedure * proc = (Procedure *)_proc;
                proc = (Procedure *)proc->clone();

                addProc(procs.first, proc);

                proc->desugarThis();

                for (ASTNode * _param : proc->getParamVarDeclarations()) {
                    VariableDeclaration * param = (VariableDeclaration *)_param;
                    param->setScope(ifaceDef->getScope());
                    param->getTypeDeclarator()->setScope(ifaceDef->getScope());

                    BJOU_DEBUG_ASSERT(param->getTypeDeclarator());

                    Declarator * new_decl =
                        param->getTypeDeclarator()
                            ->getType()
                            ->replacePlaceholders(parent->getType())
                            ->getGenericDeclarator();
                    delete param->getTypeDeclarator();
                    param->setTypeDeclarator(new_decl);
                }

                Declarator * ret_decl = (Declarator *)proc->getRetDeclarator();

                BJOU_DEBUG_ASSERT(ret_decl);

                Declarator * new_decl =
                    ret_decl->getType()
                        ->replacePlaceholders(parent->getType())
                        ->getGenericDeclarator();
                delete ret_decl;
                proc->setRetDeclarator(new_decl);
            }
        }
    }

    for (auto & procs : getProcs())
        for (ASTNode * proc : procs.second)
            proc->addSymbols(_scope);
}

void InterfaceImplementation::unwrap(std::vector<ASTNode *> & terminals) {
    for (auto & procs : getProcs())
        for (ASTNode * proc : procs.second)
            proc->unwrap(terminals);
}

ASTNode * InterfaceImplementation::clone() {
    InterfaceImplementation * c = new InterfaceImplementation(*this);

    std::map<std::string, std::vector<ASTNode *>> & my_procs = c->getProcs();
    std::map<std::string, std::vector<ASTNode *>> _procs = my_procs;

    my_procs.clear();

    c->setIdentifier(getIdentifier()->clone());

    for (auto & it : _procs)
        for (ASTNode * p : it.second)
            c->addProc(it.first, p->clone());

    return c;
}

void InterfaceImplementation::desugar() {
    for (auto & procs : getProcs())
        for (ASTNode * proc : procs.second)
            proc->desugar();
}

InterfaceImplementation::~InterfaceImplementation() {
    BJOU_DEBUG_ASSERT(identifier);
    delete identifier;
    // @bad FIX THIS WHEN YOU FIGURE OUT WHAT THE MAP IS SUPPOSED TO LOOK LIKE
}
//

// ~~~~~ Enum ~~~~~

Enum::Enum() : name({}), mangledName({}), identifiers({}) { nodeKind = ENUM; }

std::string & Enum::getName() { return name; }
void Enum::setName(std::string _name) { name = _name; }

std::string & Enum::getMangledName() { return mangledName; }
void Enum::setMangledName(std::string _mangledName) {
    mangledName = _mangledName;
}

std::vector<std::string> & Enum::getIdentifiers() { return identifiers; }
void Enum::setIdentifiers(std::vector<std::string> _identifiers) {
    identifiers = _identifiers;
}
void Enum::addIdentifier(std::string _identifier) {
    identifiers.push_back(_identifier);
}

// Node interface
void Enum::analyze(bool force) {
    HANDLE_FORCE();
    setFlag(ANALYZED, true);
}

ASTNode * Enum::clone() { return new Enum(*this); }

void Enum::addSymbols(Scope * _scope) {
    setScope(_scope);
    _Symbol<Enum> * symbol = new _Symbol<Enum>(getName(), this);
    setMangledName(symbol->mangledString(_scope));
    _scope->addSymbol(symbol, &getNameContext());
}

Enum::~Enum() {}
//

const Type * Enum::getType() {
    analyze();
    std::string mangled = getMangledName();

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

// ~~~~~ ArgList ~~~~~

ArgList::ArgList() : expressions({}) { nodeKind = ARG_LIST; }

std::vector<ASTNode *> & ArgList::getExpressions() { return expressions; }
void ArgList::setExpressions(std::vector<ASTNode *> _expressions) {
    expressions = _expressions;
}
void ArgList::addExpression(ASTNode * _expression) {
    _expression->parent = this;
    _expression->replace = rpget<replacementPolicy_ArgList_Expressions>();
    expressions.push_back(_expression);
}
void ArgList::addExpressionToFront(ASTNode * _expression) {
    _expression->parent = this;
    _expression->replace = rpget<replacementPolicy_ArgList_Expressions>();
    expressions.insert(expressions.begin(), _expression);
}

// Node interface
void ArgList::analyze(bool force) {
    HANDLE_FORCE();

    for (ASTNode * expr : getExpressions())
        expr->analyze(force);

    setFlag(ANALYZED, true);
}

void ArgList::unwrap(std::vector<ASTNode *> & terminals) {
    for (ASTNode * e : getExpressions())
        e->unwrap(terminals);
}

ASTNode * ArgList::clone() {
    ArgList * c = new ArgList(*this);

    std::vector<ASTNode *> & my_expressions = c->getExpressions();
    std::vector<ASTNode *> exprs = my_expressions;

    my_expressions.clear();

    for (ASTNode * e : exprs)
        c->addExpression(e->clone());

    return c;
}

void ArgList::desugar() {
    for (ASTNode * expr : getExpressions())
        expr->desugar();
}

void ArgList::addSymbols(Scope * _scope) {
    setScope(_scope);
    for (ASTNode * expr : getExpressions())
        expr->addSymbols(_scope);
}

ArgList::~ArgList() {
    for (ASTNode * e : expressions)
        delete e;
}
//

static void handleTerminators(ASTNode * statement,
                              std::vector<ASTNode *> & statements,
                              ASTNode *& node) {
    if (node->nodeKind == ASTNode::RETURN || node->nodeKind == ASTNode::BREAK ||
        node->nodeKind == ASTNode::CONTINUE) {

        statement->setFlag(ASTNode::HAS_TOP_LEVEL_RETURN, true);

        auto search = std::find(statements.begin(), statements.end(), node);

        BJOU_DEBUG_ASSERT(search != statements.end());

        search++;

        for (; search != statements.end(); search++) {
            if ((*search)->nodeKind != ASTNode::MACRO_USE &&
                (*search)->nodeKind != ASTNode::IGNORE) {

                std::string err_str;

                if (node->nodeKind == ASTNode::RETURN)
                    err_str = "return";
                else if (node->nodeKind == ASTNode::BREAK)
                    err_str = "break";
                else if (node->nodeKind == ASTNode::CONTINUE)
                    err_str = "continue";

                errorl(node->getContext(),
                       "Code below this " + err_str + " will never execute.");
            }
        }
    }
}

// ~~~~~ This ~~~~~

This::This() { nodeKind = THIS; }

// Node interface
const Type * This::getType() { return PlaceholderType::get()->getRef(); }

void This::analyze(bool force) {
    HANDLE_FORCE();
    setFlag(ANALYZED, true);
}

ASTNode * This::clone() { return new This(*this); }

void This::addSymbols(Scope * _scope) {
    setScope(_scope);
    // and...
}

This::~This() {}
//

// ~~~~~ Procedure ~~~~~

Procedure::Procedure()
    : name({}), mangledName({}), paramVarDeclarations({}),
      retDeclarator(nullptr), procDeclarator(nullptr), statements({}),
      inst(nullptr) {
    nodeKind = PROCEDURE;
}

std::string & Procedure::getName() { return name; }
void Procedure::setName(std::string _name) { name = _name; }

std::string & Procedure::getMangledName() { return mangledName; }
void Procedure::setMangledName(std::string _mangledName) {
    mangledName = _mangledName;
}

std::vector<ASTNode *> & Procedure::getParamVarDeclarations() {
    return paramVarDeclarations;
}
void Procedure::setParamVarDeclarations(
    std::vector<ASTNode *> _paramVarDeclarations) {
    paramVarDeclarations = _paramVarDeclarations;
}
void Procedure::addParamVarDeclaration(ASTNode * _paramVarDeclaration) {
    _paramVarDeclaration->parent = this;
    _paramVarDeclaration->replace =
        rpget<replacementPolicy_Procedure_ParamVarDeclaration>();
    paramVarDeclarations.push_back(_paramVarDeclaration);
}

ASTNode * Procedure::getRetDeclarator() const { return retDeclarator; }
void Procedure::setRetDeclarator(ASTNode * _retDeclarator) {
    retDeclarator = _retDeclarator;
    retDeclarator->parent = this;
    retDeclarator->replace = rpget<replacementPolicy_Procedure_RetDeclarator>();
}

ASTNode * Procedure::getProcDeclarator() const { return procDeclarator; }
void Procedure::setProcDeclarator(ASTNode * _procDeclarator) {
    procDeclarator = _procDeclarator;
    procDeclarator->parent = this;
    procDeclarator->replace =
        rpget<replacementPolicy_Procedure_ProcDeclarator>();
}

std::vector<ASTNode *> & Procedure::getStatements() { return statements; }
void Procedure::setStatements(std::vector<ASTNode *> _statements) {
    statements = _statements;
}
void Procedure::addStatement(ASTNode * _statement) {
    _statement->parent = this;
    _statement->replace = rpget<replacementPolicy_Procedure_Statement>();
    statements.push_back(_statement);
}

Struct * Procedure::getParentStruct() {
    Struct * s = nullptr;

    BJOU_DEBUG_ASSERT(parent);

    if (parent->nodeKind == ASTNode::STRUCT) {
        s = (Struct *)parent;
    } else if (parent->nodeKind == ASTNode::TEMPLATE_PROC &&
               parent->parent->nodeKind == ASTNode::STRUCT) {
        s = (Struct *)parent->parent;
    } else if (parent->nodeKind == ASTNode::INTERFACE_IMPLEMENTATION) {
        BJOU_DEBUG_ASSERT(parent->parent);
        s = (Struct *)parent->parent;
    } else if (parent->nodeKind == ASTNode::TEMPLATE_PROC &&
               parent->parent->nodeKind == ASTNode::INTERFACE_IMPLEMENTATION) {
        BJOU_DEBUG_ASSERT(parent->parent);
        BJOU_DEBUG_ASSERT(parent->parent->parent);
        s = (Struct *)parent->parent->parent;
    }

    BJOU_DEBUG_ASSERT(s);

    return s;
}

void Procedure::desugarThis() {
    This * seen = nullptr;
    for (ASTNode * node : getParamVarDeclarations()) {
        if (node->nodeKind == ASTNode::THIS) {
            if (!parent)
                errorl(node->getContext(),
                       "'this' not allowed here."); // @bad error message

            Struct * s = getParentStruct();

            if (seen) {
                errorl(node->getContext(), "'this' already declared.", false);
                errorl(seen->getContext(), "'this' originally declared here.");
            }

            VariableDeclaration * param = new VariableDeclaration;
            param->setFlag(VariableDeclaration::IS_PROC_PARAM, true);
            param->setContext(node->getContext());
            param->setNameContext(node->getContext());
            param->setName("this");

            Declarator * base = s->getType()->getGenericDeclarator();

            param->setTypeDeclarator(new RefDeclarator(base));
            param->getTypeDeclarator()->addSymbols(
                node->getScope()); // @bad. does this make sense?

            (*node->replace)(node->parent, node, param);

            seen = (This *)node;
        }
    }
    delete seen;
}

// Node interface
void Procedure::analyze(bool force) {
    HANDLE_FORCE();

    compilation->frontEnd.procStack.push(this);

    for (ASTNode * param : getParamVarDeclarations()) {
        param->analyze(force);
        const Type * p_t = param->getType();
        if (p_t->isStruct() && ((StructType *)p_t)->isAbstract)
            errorl(param->getContext(),
                   "Can't use '" + p_t->getDemangledName() +
                       "' as a parameter type because it is an abstract type.");
    }
    getRetDeclarator()->analyze(force);
    const Type * r_t = getRetDeclarator()->getType();
    if (r_t->isStruct() && ((StructType *)r_t)->isAbstract)
        errorl(getRetDeclarator()->getContext(),
               "Can't use '" + r_t->getDemangledName() +
                   "' as a return type because it is an abstract type.");

    for (ASTNode *& statement : getStatements()) {
        statement->analyze(force);
        handleTerminators(this, getStatements(), statement);
    }

    if (!getFlag(HAS_TOP_LEVEL_RETURN) && !getFlag(IS_EXTERN) &&
        !conv(getRetDeclarator()->getType(), VoidType::get())) {

        errorl(getContext().lastchar(),
               "'" + getName() + "' must explicitly return a value of type '" +
                   retDeclarator->getType()->getDemangledName() + "'");
    }

    compilation->frontEnd.procStack.pop();

    setFlag(ANALYZED, true);
}

void Procedure::addSymbols(Scope * _scope) {
    setScope(_scope);

    Scope * myScope = new Scope("", _scope);

    // open scope normally unless it is a local proc
    // if so, put it in the nearest namespace listing (or global if none)
    if (_scope->nspace || !_scope->parent) {
        _scope->scopes.push_back(myScope);
    } else {
        Scope * walk = _scope;
        while (walk->parent && !walk->nspace)
            walk = walk->parent;
        walk->scopes.push_back(myScope);
    }

    desugarThis();

    for (ASTNode * param : getParamVarDeclarations())
        param->addSymbols(myScope);
    getRetDeclarator()->addSymbols(myScope);
    for (ASTNode * statement : getStatements())
        statement->addSymbols(myScope);

    // necessary if proc was imported
    if (procDeclarator)
        procDeclarator->addSymbols(myScope);

    // this has to be done after params and ret so that name mangling works in
    // symbols
    _Symbol<Procedure> * symbol =
        new _Symbol<Procedure>(getName(), (Procedure *)this, inst);
    setMangledName(symbol->mangledString(_scope));

    myScope->description =
        "scope opened by " + demangledString(getMangledName());

    _scope->addSymbol(symbol, &getNameContext());

    if (getMangledName() == "printf" && getFlag(IS_EXTERN))
        compilation->frontEnd.printf_decl = this;
    if (getMangledName() == "malloc" && getFlag(IS_EXTERN))
        compilation->frontEnd.malloc_decl = this;
    if (getMangledName() == "free" && getFlag(IS_EXTERN))
        compilation->frontEnd.free_decl = this;
}

void Procedure::unwrap(std::vector<ASTNode *> & terminals) {
    for (ASTNode * param : getParamVarDeclarations())
        param->unwrap(terminals);
    getRetDeclarator()->unwrap(terminals);
    for (ASTNode * statement : getStatements())
        statement->unwrap(terminals);
}

ASTNode * Procedure::clone() {
    Procedure * c = new Procedure(*this);

    std::vector<ASTNode *> & my_paramVarDeclarations =
        c->getParamVarDeclarations();
    std::vector<ASTNode *> params = my_paramVarDeclarations;

    my_paramVarDeclarations.clear();

    for (ASTNode * p : params)
        c->addParamVarDeclaration(p->clone());

    // if (c->getRetDeclarator())
    c->setRetDeclarator(c->getRetDeclarator()->clone());

    if (c->getProcDeclarator())
        c->setProcDeclarator(c->getProcDeclarator()->clone());

    std::vector<ASTNode *> & my_statements = c->getStatements();
    std::vector<ASTNode *> stats = my_statements;

    my_statements.clear();

    for (ASTNode * s : stats)
        c->addStatement(s->clone());

    return c;
}

void Procedure::desugar() {
    if (getProcDeclarator())
        getProcDeclarator()->desugar();
    for (ASTNode * p : getParamVarDeclarations())
        p->desugar();
    getRetDeclarator()->desugar();
    for (ASTNode * s : getStatements())
        s->desugar();
}

Procedure::~Procedure() {
    for (ASTNode * pvd : paramVarDeclarations)
        delete pvd;
    BJOU_DEBUG_ASSERT(retDeclarator);
    delete retDeclarator;
    if (procDeclarator)
        delete procDeclarator;
    for (ASTNode * s : statements)
        delete s;
}
//

const Type * Procedure::getType() {
    ProcedureDeclarator * procDeclarator =
        (ProcedureDeclarator *)getProcDeclarator();
    if (procDeclarator)
        return procDeclarator->getType();
    procDeclarator = new ProcedureDeclarator();

    for (ASTNode * p : getParamVarDeclarations()) {
        Declarator * d = p->getType()->getGenericDeclarator();
        d->addSymbols(p->getScope());
        procDeclarator->addParamDeclarator(d);
    }

    procDeclarator->setRetDeclarator(
        getRetDeclarator()->getType()->getGenericDeclarator());
    procDeclarator->addSymbols(getRetDeclarator()->getScope());
    procDeclarator->setFlag(ProcedureDeclarator::eBitFlags::IS_VARARG,
                            getFlag(IS_VARARG));
    setProcDeclarator(procDeclarator);

    return procDeclarator->getType();
}

// ~~~~~ Namespace ~~~~~

Namespace::Namespace() : name({}), nodes({}), thisNamespaceScope(nullptr) {
    nodeKind = NAMESPACE;
}

std::string & Namespace::getName() { return name; }
void Namespace::setName(std::string _name) { name = _name; }

std::vector<ASTNode *> & Namespace::getNodes() { return nodes; }
void Namespace::setNodes(std::vector<ASTNode *> _nodes) { nodes = _nodes; }
void Namespace::addNode(ASTNode * _node) {
    _node->parent = this;
    _node->replace = rpget<replacementPolicy_Namespace_Node>();
    nodes.push_back(_node);
}

std::vector<ASTNode *> Namespace::gather() {
    std::vector<ASTNode *> gathered;
    gathered.reserve(nodes.size());

    for (ASTNode * node : getNodes()) {
        if (node->nodeKind == NAMESPACE) {
            std::vector<ASTNode *> sub = ((Namespace *)node)->gather();
            gathered.insert(gathered.end(), sub.begin(), sub.end());
        } else
            gathered.push_back(node);
    }

    return gathered;
}

// Node interface
void Namespace::analyze(bool force) {
    HANDLE_FORCE();

    for (ASTNode * node : getNodes())
        node->analyze(force);

    setFlag(ANALYZED, true);
}

void Namespace::preDeclare(Scope * _scope) {
    setScope(_scope);

    if (scope->namespaces.count(getName()) == 0) {
        NamespaceScope * myScope = new NamespaceScope(getName(), _scope);
        _scope->scopes.push_back(myScope);
        _scope->namespaces[getName()] = myScope;
    }

    thisNamespaceScope = scope->namespaces[getName()];

    for (ASTNode * node : nodes) {
        if (node->nodeKind == ASTNode::STRUCT)
            ((Struct *)node)->preDeclare(thisNamespaceScope);
        else if (node->nodeKind == ASTNode::NAMESPACE)
            ((Namespace *)node)->preDeclare(thisNamespaceScope);
    }
}

void Namespace::addSymbols(Scope * _scope) {
    for (ASTNode * node : getNodes())
        node->addSymbols(thisNamespaceScope);
}

void Namespace::unwrap(std::vector<ASTNode *> & terminals) {
    for (ASTNode * node : getNodes())
        node->unwrap(terminals);
}

ASTNode * Namespace::clone() {
    Namespace * c = new Namespace(*this);

    std::vector<ASTNode *> & my_nodes = c->getNodes();
    std::vector<ASTNode *> _nodes = my_nodes;

    my_nodes.clear();

    for (ASTNode * n : _nodes)
        c->addNode(n->clone());

    return c;
}

void Namespace::desugar() {
    for (ASTNode * node : getNodes())
        node->desugar();
}

Namespace::~Namespace() {
    for (ASTNode * n : nodes)
        delete n;
}
//

// ~~~~~ Import ~~~~~

Import::Import() : module({}) { nodeKind = IMPORT; }

std::string & Import::getModule() { return module; }
void Import::setModule(std::string _module) { module = _module; }

// Node interface
void Import::unwrap(std::vector<ASTNode *> & terminals) {
    terminals.push_back(this);
}

void Import::analyze(bool force) {
    HANDLE_FORCE();
    setFlag(ANALYZED, true);
}

ASTNode * Import::clone() { return new Import(*this); }

void Import::addSymbols(Scope * _scope) {
    setScope(_scope);
    // and...
}

Import::~Import() {}
//

// ~~~~~ Print ~~~~~

Print::Print() : args(nullptr) { nodeKind = PRINT; }

ASTNode * Print::getArgs() const { return args; }
void Print::setArgs(ASTNode * _args) {
    args = _args;
    args->parent = this;
    args->replace = rpget<replacementPolicy_Print_Args>();
}

// Node interface
void Print::analyze(bool force) {
    HANDLE_FORCE();

    getArgs()->analyze(force);
    ArgList * args = (ArgList *)getArgs();
    Expression * formatArg = (Expression *)args->getExpressions()[0];

    if (!conv(formatArg->getType(), CharType::get()->getPointer()) ||
        !formatArg->isConstant())
        errorl(formatArg->getContext(),
               "First argument to print must be a constant string.");
    if (args->getExpressions().size() - 1 !=
        (size_t)std::count(formatArg->getContents().begin(),
                           formatArg->getContents().end(), '%'))
        errorl(formatArg->getContext(), "Number of arguments after format "
                                        "string must match the number of '%' "
                                        "characters in format string.");

    setFlag(ANALYZED, true);
}

void Print::addSymbols(Scope * _scope) {
    setScope(_scope);
    BJOU_DEBUG_ASSERT(getArgs());
    getArgs()->addSymbols(_scope);
}

void Print::unwrap(std::vector<ASTNode *> & terminals) {
    args->unwrap(terminals);
}

ASTNode * Print::clone() {
    Print * c = new Print(*this);

    // if (c->getArgs())
    c->setArgs(c->getArgs()->clone());

    return c;
}

void Print::desugar() { getArgs()->desugar(); }

bool Print::isStatement() const { return true; }

Print::~Print() {
    BJOU_DEBUG_ASSERT(args);
    delete args;
}
//

// ~~~~~ Return ~~~~~

Return::Return() : expression(nullptr) { nodeKind = RETURN; }

ASTNode * Return::getExpression() const { return expression; }
void Return::setExpression(ASTNode * _expression) {
    _expression->parent = this;
    _expression->replace = rpget<replacementPolicy_Return_Expression>();
    expression = _expression;
}

// Node interface
void Return::analyze(bool force) {
    HANDLE_FORCE();

    Procedure * proc =
        (Procedure *)(compilation->frontEnd.procStack.empty()
                          ? nullptr
                          : compilation->frontEnd.procStack.top());

    if (proc) {
        const Type * retLVal = proc->getRetDeclarator()->getType(); // @leak
        compilation->frontEnd.lValStack.push(retLVal);

        if (getExpression()) {
            Expression * expr = (Expression *)getExpression();
            expr->analyze(force);
            // @leaks abound
            if (retLVal == VoidType::get()) {
                errorl(expr->getContext(),
                       "'" + proc->getName() + "' does not return a value.");
            } else {
                const Type * expr_t = expr->getType();
                if (!conv(expr_t, retLVal))
                    errorl(expr->getContext(),
                           "return statement does not match the return type "
                           "for procedure '" +
                               proc->getName() + "'.",
                           true,
                           "expected '" + retLVal->getDemangledName() + "'",
                           "got '" + expr_t->getDemangledName() + "'");
                if (retLVal->isPrimative() && expr_t->isPrimative())
                    if (!equal(expr_t, retLVal))
                        emplaceConversion((Expression *)getExpression(),
                                          retLVal);
            }
        } else if (retLVal != VoidType::get()) {
            errorl(getContext(), "'" + proc->getName() +
                                     "' must return an expression of type '" +
                                     retLVal->getDemangledName());
        }

        compilation->frontEnd.lValStack.pop();
    } else
        errorl(getContext(),
               "Unexpected return statement outside of procedure.");

    setFlag(ANALYZED, true);
}

void Return::addSymbols(Scope * _scope) {
    setScope(_scope);
    if (expression)
        expression->addSymbols(_scope);
}

void Return::unwrap(std::vector<ASTNode *> & terminals) {
    if (getExpression())
        getExpression()->unwrap(terminals);
}

ASTNode * Return::clone() {
    Return * c = new Return(*this);

    if (c->getExpression())
        c->setExpression(c->getExpression()->clone());

    return c;
}

void Return::desugar() {
    if (getExpression())
        getExpression()->desugar();
}

bool Return::isStatement() const { return true; }

Return::~Return() {
    if (expression)
        delete expression;
}
//

// ~~~~~ Break ~~~~~

Break::Break() { nodeKind = BREAK; }

// Node interface
void Break::analyze(bool force) {
    HANDLE_FORCE();

    ASTNode * p = parent;

    while (p && p->isStatement()) {
        if (p->nodeKind == ASTNode::WHILE || p->nodeKind == ASTNode::DO_WHILE ||
            p->nodeKind == ASTNode::FOR) {
            break;
        }
        p = p->parent;
    }

    if (!p || !p->isStatement())
        errorl(getContext(), "no loop to break out of");

    setFlag(ANALYZED, true);
}

ASTNode * Break::clone() { return new Break(*this); }

void Break::addSymbols(Scope * _scope) {
    setScope(_scope);
    // and...
}

bool Break::isStatement() const { return true; }

Break::~Break() {}
//

// ~~~~~ Continue ~~~~~

Continue::Continue() { nodeKind = CONTINUE; }

// Node interface
void Continue::analyze(bool force) {
    HANDLE_FORCE();

    ASTNode * p = parent;

    while (p && p->isStatement()) {
        if (p->nodeKind == ASTNode::WHILE || p->nodeKind == ASTNode::DO_WHILE ||
            p->nodeKind == ASTNode::FOR) {
            break;
        }
        p = p->parent;
    }

    if (!p || !p->isStatement())
        errorl(getContext(), "no loop to continue");

    setFlag(ANALYZED, true);
}

ASTNode * Continue::clone() { return new Continue(*this); }

void Continue::addSymbols(Scope * _scope) {
    setScope(_scope);
    // and...
}

bool Continue::isStatement() const { return true; }

Continue::~Continue() {}
//

// ~~~~~ If ~~~~~

If::If() : conditional(nullptr), statements({}), _else(nullptr) {
    nodeKind = IF;
}

ASTNode * If::getConditional() const { return conditional; }
void If::setConditional(ASTNode * _conditional) {
    conditional = _conditional;
    conditional->parent = this;
    conditional->replace = rpget<replacementPolicy_If_Conditional>();
}

std::vector<ASTNode *> & If::getStatements() { return statements; }
void If::setStatements(std::vector<ASTNode *> _statements) {
    statements = _statements;
}
void If::addStatement(ASTNode * _statement) {
    _statement->parent = this;
    _statement->replace = rpget<replacementPolicy_If_Statement>();
    statements.push_back(_statement);
}

ASTNode * If::getElse() const { return _else; }
void If::setElse(ASTNode * __else) {
    _else = __else;
    _else->parent = this;
    _else->replace = rpget<replacementPolicy_If_Else>();
}

// Node interface
void If::analyze(bool force) {
    HANDLE_FORCE();

    getConditional()->analyze(force);

    if (!equal(getConditional()->getType(), BoolType::get()))
        errorl(
            getConditional()->getContext(),
            "Expression resulting in type 'bool' is required for conditionals.",
            true,
            "'" + getConditional()->getType()->getDemangledName() +
                "' is invalid");
    for (ASTNode *& statement : getStatements()) {
        statement->analyze(force);
        handleTerminators(this, getStatements(), statement);
    }

    if (getElse())
        getElse()->analyze(force);

    setFlag(ANALYZED, true);
}

void If::addSymbols(Scope * _scope) {
    setScope(_scope);
    Scope * myScope = new Scope(
        "line " + std::to_string(getContext().begin.line) + " if", _scope);
    _scope->scopes.push_back(myScope);
    conditional->addSymbols(_scope);
    for (ASTNode * statement : getStatements())
        statement->addSymbols(myScope);
    if (_else)
        _else->addSymbols(_scope);
}

void If::unwrap(std::vector<ASTNode *> & terminals) {
    getConditional()->unwrap(terminals);
    for (ASTNode * statement : getStatements())
        statement->unwrap(terminals);
    if (getElse())
        getElse()->unwrap(terminals);
}

ASTNode * If::clone() {
    If * c = new If(*this);

    // if (c->getConditional())
    c->setConditional(c->getConditional()->clone());

    c->getStatements().clear();

    for (ASTNode * s : getStatements())
        c->addStatement(s->clone());

    if (c->getElse())
        c->setElse(c->getElse()->clone());

    return c;
}

void If::desugar() {
    getConditional()->desugar();
    for (ASTNode * s : getStatements())
        s->desugar();
    if (getElse())
        getElse()->desugar();
}

bool If::isStatement() const { return true; }

If::~If() {
    BJOU_DEBUG_ASSERT(conditional);
    delete conditional;
    for (ASTNode * s : statements)
        delete s;
    if (_else)
        delete _else;
}
//

// ~~~~~ Else ~~~~~

Else::Else() : statements({}) { nodeKind = ELSE; }

std::vector<ASTNode *> & Else::getStatements() { return statements; }
void Else::setStatements(std::vector<ASTNode *> _statements) {
    statements = _statements;
}
void Else::addStatement(ASTNode * _statement) {
    _statement->parent = this;
    _statement->replace = rpget<replacementPolicy_Else_Statement>();
    statements.push_back(_statement);
}

// Node interface
void Else::analyze(bool force) {
    HANDLE_FORCE();

    for (ASTNode *& statement : getStatements()) {
        statement->analyze(force);
        handleTerminators(this, getStatements(), statement);
    }

    setFlag(ANALYZED, true);
}

void Else::addSymbols(Scope * _scope) {
    setScope(_scope);
    Scope * myScope = new Scope(
        "line " + std::to_string(getContext().begin.line) + " else", _scope);
    _scope->scopes.push_back(myScope);
    for (ASTNode *& statement : getStatements())
        statement->addSymbols(myScope);
}

void Else::unwrap(std::vector<ASTNode *> & terminals) {
    for (ASTNode * statement : getStatements())
        statement->unwrap(terminals);
}

ASTNode * Else::clone() {
    Else * c = new Else(*this);

    c->getStatements().clear();

    for (ASTNode * s : getStatements())
        c->addStatement(s->clone());

    return c;
}

void Else::desugar() {
    for (ASTNode * s : getStatements())
        s->desugar();
}

bool Else::isStatement() const { return true; }

Else::~Else() {
    for (ASTNode * s : statements)
        delete s;
}
//

// ~~~~~ For ~~~~~

For::For()
    : initializations({}), conditional(nullptr), afterthoughts({}),
      statements({}) {
    nodeKind = FOR;
}

std::vector<ASTNode *> & For::getInitializations() { return initializations; }
void For::setInitializations(std::vector<ASTNode *> _initializations) {
    initializations = _initializations;
}
void For::addInitialization(ASTNode * _initialization) {
    _initialization->parent = this;
    _initialization->replace = rpget<replacementPolicy_For_Initialization>();
    initializations.push_back(_initialization);
}

ASTNode * For::getConditional() const { return conditional; }
void For::setConditional(ASTNode * _conditional) {
    conditional = _conditional;
    conditional->parent = this;
    conditional->replace = rpget<replacementPolicy_For_Conditional>();
}

std::vector<ASTNode *> & For::getAfterthoughts() { return afterthoughts; }
void For::setAfterthoughts(std::vector<ASTNode *> _afterthoughts) {
    afterthoughts = _afterthoughts;
}
void For::addAfterthought(ASTNode * _afterthought) {
    _afterthought->parent = this;
    _afterthought->replace = rpget<replacementPolicy_For_Afterthought>();
    afterthoughts.push_back(_afterthought);
}

std::vector<ASTNode *> & For::getStatements() { return statements; }
void For::setStatements(std::vector<ASTNode *> _statements) {
    statements = _statements;
}
void For::addStatement(ASTNode * _statement) {
    _statement->parent = this;
    _statement->replace = rpget<replacementPolicy_For_Statement>();
    statements.push_back(_statement);
}

// Node interface
void For::analyze(bool force) {
    HANDLE_FORCE();

    for (ASTNode * init : getInitializations())
        init->analyze(force);

    getConditional()->analyze(force);

    if (!equal(getConditional()->getType(), BoolType::get()))
        errorl(
            getConditional()->getContext(),
            "Expression resulting in type 'bool' is required for conditionals.",
            true,
            "'" + getConditional()->getType()->getDemangledName() +
                "' is invalid");

    for (ASTNode * at : getAfterthoughts())
        at->analyze(force);

    for (ASTNode *& statement : getStatements()) {
        statement->analyze(force);
        handleTerminators(this, getStatements(), statement);
    }

    setFlag(ANALYZED, true);
}

void For::addSymbols(Scope * _scope) {
    setScope(_scope);
    Scope * myScope = new Scope(
        "line " + std::to_string(getContext().begin.line) + " for", _scope);
    _scope->scopes.push_back(myScope);
    for (ASTNode * initialization : getInitializations())
        initialization->addSymbols(myScope);
    conditional->addSymbols(myScope);
    for (ASTNode * afterthought : getAfterthoughts())
        afterthought->addSymbols(myScope);
    for (ASTNode * statement : getStatements())
        statement->addSymbols(myScope);
}

void For::unwrap(std::vector<ASTNode *> & terminals) {
    for (ASTNode * initialization : getInitializations())
        initialization->unwrap(terminals);
    getConditional()->unwrap(terminals);
    for (ASTNode * afterthought : getAfterthoughts())
        afterthought->unwrap(terminals);
    for (ASTNode * statement : getStatements())
        statement->unwrap(terminals);
}

ASTNode * For::clone() {
    For * c = new For(*this);

    c->getInitializations().clear();
    for (ASTNode * s : getInitializations())
        c->addInitialization(s->clone());

    // if (c->getConditional())
    c->setConditional(c->getConditional()->clone());

    c->getAfterthoughts().clear();
    for (ASTNode * s : getAfterthoughts())
        c->addAfterthought(s->clone());

    c->getStatements().clear();
    for (ASTNode * s : getStatements())
        c->addStatement(s->clone());

    return c;
}

void For::desugar() {
    for (ASTNode * i : getInitializations())
        i->desugar();
    getConditional()->desugar();
    for (ASTNode * at : getAfterthoughts())
        at->desugar();
    for (ASTNode * s : getStatements())
        s->desugar();
}

bool For::isStatement() const { return true; }

For::~For() {
    for (ASTNode * i : initializations)
        delete i;
    ASTNode * conditional = getConditional();
    BJOU_DEBUG_ASSERT(conditional);
    delete conditional;
    for (ASTNode * a : afterthoughts)
        delete a;
    for (ASTNode * s : statements)
        delete s;
}
//

// ~~~~~ Foreach ~~~~~

Foreach::Foreach() : expression(nullptr), statements({}) { nodeKind = FOREACH; }

Context & Foreach::getIdentContext() { return identContext; }
void Foreach::setIdentContext(Context & _context) { identContext = _context; }

std::string & Foreach::getIdent() { return ident; }
void Foreach::setIdent(std::string & _ident) { ident = _ident; }

ASTNode * Foreach::getExpression() { return expression; }
void Foreach::setExpression(ASTNode * _expr) {
    expression = _expr;
    expression->parent = this;
    expression->replace = rpget<replacementPolicy_Foreach_Expression>();
}

std::vector<ASTNode *> & Foreach::getStatements() { return statements; }
void Foreach::setStatements(std::vector<ASTNode *> _statements) {
    statements = _statements;
}
void Foreach::addStatement(ASTNode * _statement) {
    _statement->parent = this;
    _statement->replace = rpget<replacementPolicy_Foreach_Statement>();
    statements.push_back(_statement);
}

// Node interface
void Foreach::analyze(bool force) {
    HANDLE_FORCE();

    const Type * t = getExpression()->getType()->unRef();

    if (!t->isArray() && !t->isSlice() && !t->isDynamicArray())
        errorl(getExpression()->getContext(),
               "Can't loop over '" + t->getDemangledName() + "'.", true,
               "array, slice, or dynamic array is required");

    if (t->isSlice() && getFlag(TAKE_REF))
        errorl(getExpression()->getContext(),
               "Can't take references from slice.");

    desugar();

    setFlag(ANALYZED, true);
}

void Foreach::addSymbols(Scope * _scope) {
    setScope(_scope);
    getExpression()->addSymbols(_scope);
}

void Foreach::unwrap(std::vector<ASTNode *> & terminals) {
    getExpression()->unwrap(terminals);
}

ASTNode * Foreach::clone() {
    Foreach * c = new Foreach(*this);

    c->setExpression(getExpression()->clone());

    return c;
}

void Foreach::desugar() {
    const Type * t = getExpression()->getType()->unRef();
    const Type * elem_t = t->under();

    For * _for = new For;
    _for->setContext(getContext());

    VariableDeclaration * it = new VariableDeclaration;
    it->setName(compilation->frontEnd.makeUID("__bjou_foreach_it"));

    IntegerLiteral * zero = new IntegerLiteral;
    zero->setContents("0");

    it->setInitialization(zero);

    _for->addInitialization(it);

    LssExpression * lss = new LssExpression;

    Identifier * i_it = new Identifier;
    i_it->setUnqualified(it->getName());

    LenExpression * len = new LenExpression;
    len->setExpr(getExpression());

    lss->setLeft(i_it);
    lss->setRight(len);

    _for->setConditional(lss);

    AddAssignExpression * inc = new AddAssignExpression;

    Identifier * _i_it = new Identifier;
    _i_it->setUnqualified(it->getName());

    IntegerLiteral * one = new IntegerLiteral;
    one->setContents("1");

    inc->setLeft(_i_it);
    inc->setRight(one);

    _for->addAfterthought(inc);

    VariableDeclaration * elem = new VariableDeclaration;
    elem->setContext(getIdentContext());
    elem->setNameContext(getIdentContext());
    elem->setName(getIdent());
    if (getFlag(TAKE_REF))
        elem->setTypeDeclarator(elem_t->getRef()->getGenericDeclarator());
    else
        elem->setTypeDeclarator(elem_t->getGenericDeclarator());

    SubscriptExpression * sub = new SubscriptExpression;

    Identifier * __i_it = new Identifier;
    __i_it->setUnqualified(it->getName());

    sub->setLeft(getExpression()->clone());
    sub->setRight(__i_it);

    elem->setInitialization(sub);

    _for->addStatement(elem);
    for (ASTNode * s : getStatements())
        _for->addStatement(s);

    _for->addSymbols(getScope());

    (*replace)(parent, this, _for);

    _for->analyze();
}

bool Foreach::isStatement() const { return true; }

Foreach::~Foreach() { delete getExpression(); }
//

// ~~~~~ While ~~~~~

While::While() : conditional(nullptr), statements({}) { nodeKind = WHILE; }

ASTNode * While::getConditional() const { return conditional; }
void While::setConditional(ASTNode * _conditional) {
    conditional = _conditional;
    conditional->parent = this;
    conditional->replace = rpget<replacementPolicy_While_Conditional>();
}

std::vector<ASTNode *> & While::getStatements() { return statements; }
void While::setStatements(std::vector<ASTNode *> _statements) {
    statements = _statements;
}
void While::addStatement(ASTNode * _statement) {
    _statement->parent = this;
    _statement->replace = rpget<replacementPolicy_While_Statement>();
    statements.push_back(_statement);
}

// Node interface
void While::analyze(bool force) {
    HANDLE_FORCE();

    getConditional()->analyze(force);

    if (!equal(getConditional()->getType(), BoolType::get()))
        errorl(
            getConditional()->getContext(),
            "Expression resulting in type 'bool' is required for conditionals.",
            true,
            "'" + getConditional()->getType()->getDemangledName() +
                "' is invalid");

    for (ASTNode *& statement : getStatements()) {
        statement->analyze(force);
        handleTerminators(this, getStatements(), statement);
    }

    setFlag(ANALYZED, true);
}

void While::addSymbols(Scope * _scope) {
    setScope(_scope);
    Scope * myScope = new Scope(
        "line " + std::to_string(getContext().begin.line) + " while", _scope);
    _scope->scopes.push_back(myScope);
    conditional->addSymbols(_scope);
    for (ASTNode * statement : getStatements())
        statement->addSymbols(myScope);
}

void While::unwrap(std::vector<ASTNode *> & terminals) {
    getConditional()->unwrap(terminals);
    for (ASTNode * statement : getStatements())
        statement->unwrap(terminals);
}

ASTNode * While::clone() {
    While * c = new While(*this);

    // if (c->getConditional())
    c->setConditional(c->getConditional()->clone());

    c->getStatements().clear();
    for (ASTNode * s : getStatements())
        c->addStatement(s->clone());

    return c;
}

void While::desugar() {
    getConditional()->desugar();
    for (ASTNode * s : getStatements())
        s->desugar();
}

bool While::isStatement() const { return true; }

While::~While() {
    BJOU_DEBUG_ASSERT(conditional);
    delete conditional;
    for (ASTNode * s : statements)
        delete s;
}
//

// ~~~~~ DoWhile ~~~~~

DoWhile::DoWhile() : conditional(nullptr), statements({}) {
    nodeKind = DO_WHILE;
}

ASTNode * DoWhile::getConditional() const { return conditional; }
void DoWhile::setConditional(ASTNode * _conditional) {
    conditional = _conditional;
    conditional->parent = this;
    conditional->replace = rpget<replacementPolicy_DoWhile_Conditional>();
}

std::vector<ASTNode *> & DoWhile::getStatements() { return statements; }
void DoWhile::setStatements(std::vector<ASTNode *> _statements) {
    statements = _statements;
}
void DoWhile::addStatement(ASTNode * _statement) {
    _statement->parent = this;
    _statement->replace = rpget<replacementPolicy_DoWhile_Statement>();
    statements.push_back(_statement);
}

// Node interface
void DoWhile::analyze(bool force) {
    HANDLE_FORCE();

    getConditional()->analyze(force);

    if (!equal(getConditional()->getType(), BoolType::get()))
        errorl(
            getConditional()->getContext(),
            "Expression resulting in type 'bool' is required for conditionals.",
            true,
            "'" + getConditional()->getType()->getDemangledName() +
                "' is invalid");

    for (ASTNode *& statement : getStatements()) {
        statement->analyze(force);
        handleTerminators(this, getStatements(), statement);
    }

    setFlag(ANALYZED, true);
}

void DoWhile::addSymbols(Scope * _scope) {
    setScope(_scope);
    Scope * myScope = new Scope(
        "line " + std::to_string(getContext().begin.line) + " if", _scope);
    _scope->scopes.push_back(myScope);
    conditional->addSymbols(_scope);
    for (ASTNode * statement : getStatements())
        statement->addSymbols(myScope);
}

void DoWhile::unwrap(std::vector<ASTNode *> & terminals) {
    getConditional()->unwrap(terminals);
    for (ASTNode * statement : getStatements())
        statement->unwrap(terminals);
}

ASTNode * DoWhile::clone() {
    DoWhile * c = new DoWhile(*this);

    // if (c->getConditional())
    c->setConditional(c->getConditional()->clone());

    c->getStatements().clear();
    for (ASTNode * s : getStatements())
        c->addStatement(s->clone());

    return c;
}

void DoWhile::desugar() {
    getConditional()->desugar();
    for (ASTNode * s : getStatements())
        s->desugar();
}

bool DoWhile::isStatement() const { return true; }

DoWhile::~DoWhile() {
    BJOU_DEBUG_ASSERT(conditional);
    delete conditional;
    for (ASTNode * s : statements)
        delete s;
}
//

// ~~~~~ Match ~~~~~

Match::Match() : expression(nullptr), withs({}) { nodeKind = MATCH; }

ASTNode * Match::getExpression() const { return expression; }
void Match::setExpression(ASTNode * _expression) {
    expression = _expression;
    expression->parent = this;
    expression->replace = rpget<replacementPolicy_Match_Expression>();
}

std::vector<ASTNode *> & Match::getWiths() { return withs; }
void Match::setWiths(std::vector<ASTNode *> _withs) { withs = _withs; }
void Match::addWith(ASTNode * _with) {
    _with->parent = this;
    _with->replace = rpget<replacementPolicy_Match_With>();
    withs.push_back(_with);
}

// Node interface
void Match::analyze(bool force) {
    HANDLE_FORCE();

    getExpression()->analyze(force);

    for (ASTNode * _with : getWiths()) {
        With * with = (With *)_with;
        with->analyze(force);
        Expression * w_expr = (Expression *)with->getExpression();
        if (!conv(w_expr->getType(), getExpression()->getType()))
            errorl(w_expr->getContext(),
                   "Expression of type '" +
                       w_expr->getType()->getDemangledName() +
                       "' does not match 'match' type '" +
                       getExpression()->getType()->getDemangledName() + "'.");
    }

    setFlag(ANALYZED, true);
}

void Match::addSymbols(Scope * _scope) {
    setScope(_scope);
    getExpression()->addSymbols(_scope);
    for (ASTNode * with : getWiths())
        with->addSymbols(_scope);
}

void Match::unwrap(std::vector<ASTNode *> & terminals) {
    getExpression()->unwrap(terminals);
    for (ASTNode * with : getWiths())
        with->unwrap(terminals);
}

ASTNode * Match::clone() {
    Match * c = new Match(*this);

    // if (c->getExpression())
    c->setExpression(c->getExpression()->clone());

    c->getWiths().clear();
    for (ASTNode * w : getWiths())
        c->addWith(w->clone());

    return c;
}

void Match::desugar() {
    getExpression()->desugar();
    for (ASTNode * w : getWiths())
        w->desugar();
}

bool Match::isStatement() const { return true; }

Match::~Match() {
    BJOU_DEBUG_ASSERT(expression);
    delete expression;
    for (ASTNode * w : withs)
        delete w;
}
//

// ~~~~~ With ~~~~~

With::With() : expression(nullptr), statements({}) { nodeKind = WITH; }

ASTNode * With::getExpression() const { return expression; }
void With::setExpression(ASTNode * _expression) {
    expression = _expression;
    expression->parent = this;
    expression->replace = rpget<replacementPolicy_With_Expression>();
}

std::vector<ASTNode *> & With::getStatements() { return statements; }
void With::setStatements(std::vector<ASTNode *> _statements) {
    statements = _statements;
}
void With::addStatement(ASTNode * _statement) {
    _statement->parent = this;
    _statement->replace = rpget<replacementPolicy_With_Statement>();
    statements.push_back(_statement);
}

// Node interface
void With::analyze(bool force) {
    HANDLE_FORCE();

    getExpression()->analyze(force);

    for (ASTNode *& statement : getStatements()) {
        statement->analyze(force);
        handleTerminators(this, getStatements(), statement);
    }

    setFlag(ANALYZED, true);
}

void With::addSymbols(Scope * _scope) {
    setScope(_scope);
    Scope * myScope = new Scope(
        "line " + std::to_string(getContext().begin.line) + " with", _scope);
    _scope->scopes.push_back(myScope);
    getExpression()->addSymbols(_scope);
    for (ASTNode * statement : getStatements())
        statement->addSymbols(myScope);
}

void With::unwrap(std::vector<ASTNode *> & terminals) {
    getExpression()->unwrap(terminals);
    for (ASTNode * statement : getStatements())
        statement->unwrap(terminals);
}

ASTNode * With::clone() {
    With * c = new With(*this);

    if (c->getExpression())
        c->setExpression(c->getExpression()->clone());

    c->getStatements().clear();
    for (ASTNode * s : getStatements())
        c->addStatement(s->clone());

    return c;
}

void With::desugar() {
    getExpression()->desugar();
    for (ASTNode * s : getStatements())
        s->desugar();
}

bool With::isStatement() const { return true; }

With::~With() {
    BJOU_DEBUG_ASSERT(expression);
    delete expression;
    for (ASTNode * s : statements)
        delete s;
}
//

// ~~~~~ TemplateDefineList ~~~~~

TemplateDefineList::TemplateDefineList() : elements({}) {
    nodeKind = TEMPLATE_DEFINE_LIST;
}

std::vector<ASTNode *> & TemplateDefineList::getElements() { return elements; }
void TemplateDefineList::setElements(std::vector<ASTNode *> _elements) {
    elements = _elements;
}
void TemplateDefineList::addElement(ASTNode * _element) {
    _element->parent = this;
    _element->replace = rpget<replacementPolicy_TemplateDefineList_Element>();
    elements.push_back(_element);
}

// Node interface
void TemplateDefineList::analyze(bool force) {
    HANDLE_FORCE();

    // @incomplete

    setFlag(ANALYZED, true);
}

void TemplateDefineList::addSymbols(Scope * _scope) { setScope(_scope); }

void TemplateDefineList::unwrap(std::vector<ASTNode *> & terminals) {
    for (ASTNode * element : getElements())
        element->unwrap(terminals);
}

ASTNode * TemplateDefineList::clone() {
    TemplateDefineList * c = new TemplateDefineList(*this);

    c->getElements().clear();
    for (ASTNode * e : getElements())
        c->addElement(e->clone());

    return c;
}

void TemplateDefineList::desugar() {
    for (ASTNode * e : getElements())
        e->desugar();
}

TemplateDefineList::~TemplateDefineList() {
    for (ASTNode * e : elements)
        delete e;
}
//

// ~~~~~ TemplateDefineElement ~~~

TemplateDefineElement::TemplateDefineElement() : name({}) {
    nodeKind = TEMPLATE_DEFINE_ELEMENT;
}

std::string & TemplateDefineElement::getName() { return name; }
void TemplateDefineElement::setName(std::string _name) { name = _name; }

// ~~~~~ TemplateDefineTypeDescriptor ~~~~~

TemplateDefineTypeDescriptor::TemplateDefineTypeDescriptor() : bounds({}) {
    nodeKind = TEMPLATE_DEFINE_TYPE_DESCRIPTOR;
}

std::vector<ASTNode *> & TemplateDefineTypeDescriptor::getBounds() {
    return bounds;
}
void TemplateDefineTypeDescriptor::setBounds(std::vector<ASTNode *> _bounds) {
    bounds = _bounds;
}
void TemplateDefineTypeDescriptor::addBound(ASTNode * _bound) {
    _bound->parent = this;
    _bound->replace =
        rpget<replacementPolicy_TemplateDefineTypeDescriptor_Bound>();
    bounds.push_back(_bound);
}

// Node interface
void TemplateDefineTypeDescriptor::analyze(bool force) {
    HANDLE_FORCE();
    setFlag(ANALYZED, true);
}

void TemplateDefineTypeDescriptor::addSymbols(Scope * _scope) {
    setScope(_scope);
}

void TemplateDefineTypeDescriptor::unwrap(std::vector<ASTNode *> & terminals) {
    for (ASTNode * bound : getBounds())
        bound->unwrap(terminals);
}

ASTNode * TemplateDefineTypeDescriptor::clone() {
    TemplateDefineTypeDescriptor * c = new TemplateDefineTypeDescriptor(*this);

    c->getBounds().clear();
    for (ASTNode * b : getBounds())
        c->addBound(b->clone());

    return c;
}

void TemplateDefineTypeDescriptor::desugar() {
    for (ASTNode * b : getBounds())
        b->desugar();
}

TemplateDefineTypeDescriptor::~TemplateDefineTypeDescriptor() {
    for (ASTNode * b : bounds)
        delete b;
}
//

// ~~~~~ TemplateDefineVariadicTypeArgs ~~~~~

TemplateDefineVariadicTypeArgs::TemplateDefineVariadicTypeArgs() {
    nodeKind = TEMPLATE_DEFINE_VARIADIC_TYPE_ARGS;
}

// Node interface
void TemplateDefineVariadicTypeArgs::analyze(bool force) {
    HANDLE_FORCE();
    setFlag(ANALYZED, true);
}

ASTNode * TemplateDefineVariadicTypeArgs::clone() {
    return new TemplateDefineVariadicTypeArgs(*this);
}

void TemplateDefineVariadicTypeArgs::addSymbols(Scope * _scope) {
    setScope(_scope);
}

TemplateDefineVariadicTypeArgs::~TemplateDefineVariadicTypeArgs() {}
//

// ~~~~~ TemplateDefineExpression ~~~~~

TemplateDefineExpression::TemplateDefineExpression() : varDecl(nullptr) {
    nodeKind = TEMPLATE_DEFINE_EXPRESSION;
}

ASTNode * TemplateDefineExpression::getVarDecl() const { return varDecl; }
void TemplateDefineExpression::setVarDecl(ASTNode * _varDecl) {
    varDecl = _varDecl;
    varDecl->parent = this;
    varDecl->replace =
        rpget<replacementPolicy_TemplateDefineExpression_VarDecl>();
}

// Node interface
void TemplateDefineExpression::analyze(bool force) {
    HANDLE_FORCE();
    setFlag(ANALYZED, true);
}

void TemplateDefineExpression::addSymbols(Scope * _scope) { setScope(_scope); }

void TemplateDefineExpression::unwrap(std::vector<ASTNode *> & terminals) {
    getVarDecl()->unwrap(terminals);
}

ASTNode * TemplateDefineExpression::clone() {
    TemplateDefineExpression * c = new TemplateDefineExpression(*this);

    // if (c->getVarDecl())
    c->setVarDecl(c->getVarDecl()->clone());

    return c;
}

void TemplateDefineExpression::desugar() { getVarDecl()->desugar(); }

TemplateDefineExpression::~TemplateDefineExpression() {
    BJOU_DEBUG_ASSERT(varDecl);
    delete varDecl;
}
//

// ~~~~~ TemplateInstantiation ~~~~~

TemplateInstantiation::TemplateInstantiation() : elements({}) {
    nodeKind = TEMPLATE_INSTANTIATION;
}

std::vector<ASTNode *> & TemplateInstantiation::getElements() {
    return elements;
}
void TemplateInstantiation::setElements(std::vector<ASTNode *> _elements) {
    elements = _elements;
}
void TemplateInstantiation::addElement(ASTNode * _element) {
    _element->parent = this;
    _element->replace =
        rpget<replacementPolicy_TemplateInstantiation_Element>();
    elements.push_back(_element);
}

// Node interface
void TemplateInstantiation::analyze(bool force) {
    HANDLE_FORCE();
    setFlag(ANALYZED, true);
}

void TemplateInstantiation::addSymbols(Scope * _scope) {
    setScope(_scope);
    for (ASTNode * element : getElements())
        element->addSymbols(_scope);
}

void TemplateInstantiation::unwrap(std::vector<ASTNode *> & terminals) {
    for (ASTNode * element : getElements())
        element->unwrap(terminals);
}

ASTNode * TemplateInstantiation::clone() {
    TemplateInstantiation * c = new TemplateInstantiation(*this);

    c->getElements().clear();
    for (ASTNode * e : getElements())
        c->addElement(e->clone());

    return c;
}

void TemplateInstantiation::desugar() {
    for (ASTNode * e : getElements())
        e->desugar();
}

TemplateInstantiation::~TemplateInstantiation() {
    for (ASTNode * e : elements)
        delete e;
}
//

// ~~~~~ TemplateAlias ~~~~~

TemplateAlias::TemplateAlias() : _template(nullptr), templateDef(nullptr) {
    nodeKind = TEMPLATE_ALIAS;
    setFlag(IS_TEMPLATE, true);
}

ASTNode * TemplateAlias::getTemplate() const { return _template; }
void TemplateAlias::setTemplate(ASTNode * __template) {
    _template = __template;
    _template->parent = this;
    _template->replace = rpget<replacementPolicy_TemplateAlias_Template>();
}

ASTNode * TemplateAlias::getTemplateDef() const { return templateDef; }
void TemplateAlias::setTemplateDef(ASTNode * _templateDef) {
    templateDef = _templateDef;
    templateDef->parent = this;
    templateDef->replace = rpget<replacementPolicy_TemplateAlias_TemplateDef>();
}

// Node interface
void TemplateAlias::analyze(bool force) {
    HANDLE_FORCE();
    setFlag(ANALYZED, true);
}

ASTNode * TemplateAlias::clone() {
    TemplateAlias * c = new TemplateAlias(*this);

    // if (c->getTemplate())
    c->setTemplate(c->getTemplate()->clone());

    // if (c->getTemplateDef())
    c->setTemplateDef(c->getTemplateDef()->clone());

    return c;
}

void TemplateAlias::addSymbols(Scope * _scope) {
    setScope(_scope);
    Alias * typeTemplate = (Alias *)getTemplate();
    _Symbol<TemplateAlias> * symbol =
        new _Symbol<TemplateAlias>(typeTemplate->getName(), this);
    _scope->addSymbol(symbol, &typeTemplate->getNameContext());

    // @refactor? should this really be here?
    std::string mangledName = symbol->mangledString(_scope);

    BJOU_DEBUG_ASSERT(false);

    /*
    compilation->frontEnd.typeTable[mangledName] =
        new TemplateAliasType(mangledName);
    */
}

TemplateAlias::~TemplateAlias() {
    if (_template)
        delete _template;
    if (templateDef)
        delete templateDef;
}
//

// ~~~~~ TemplateStruct ~~~~~

TemplateStruct::TemplateStruct() : _template(nullptr), templateDef(nullptr) {
    nodeKind = TEMPLATE_STRUCT;
    setFlag(IS_TEMPLATE, true);
}

ASTNode * TemplateStruct::getTemplate() const { return _template; }
void TemplateStruct::setTemplate(ASTNode * __template) {
    _template = __template;
    _template->parent = this;
    _template->replace = rpget<replacementPolicy_TemplateStruct_Template>();
}

ASTNode * TemplateStruct::getTemplateDef() const { return templateDef; }
void TemplateStruct::setTemplateDef(ASTNode * _templateDef) {
    templateDef = _templateDef;
    templateDef->parent = this;
    templateDef->replace =
        rpget<replacementPolicy_TemplateStruct_TemplateDef>();
}

// Node interface
const Type * TemplateStruct::getType() {
    std::string mangled = ((Struct *)_template)->getName(); // @incomplete

    return PlaceholderType::get(); // @bad -- not intended use of
                                   // PlaceholderType
}

void TemplateStruct::analyze(bool force) {
    HANDLE_FORCE();
    setFlag(ANALYZED, true);
}

ASTNode * TemplateStruct::clone() {
    TemplateStruct * c = new TemplateStruct(*this);

    // if (c->getTemplate())
    c->setTemplate(c->getTemplate()->clone());

    // if (c->getTemplateDef())
    c->setTemplateDef(c->getTemplateDef()->clone());

    return c;
}

void TemplateStruct::addSymbols(Scope * _scope) {
    setScope(_scope);
    Struct * typeTemplate = (Struct *)getTemplate();
    _Symbol<TemplateStruct> * symbol =
        new _Symbol<TemplateStruct>(typeTemplate->getName(), this);
    _scope->addSymbol(symbol, &typeTemplate->getNameContext());

    // @refactor? should this really be here?
    std::string mangledName = symbol->mangledString(_scope);

    /*
    compilation->frontEnd.typeTable[mangledName] =
        new TemplateStructType(mangledName);
    */
}

TemplateStruct::~TemplateStruct() {
    if (_template)
        delete _template;
    if (templateDef)
        delete templateDef;
}
//

// ~~~~~ TemplateProc ~~~~~

TemplateProc::TemplateProc() : _template(nullptr), templateDef(nullptr) {
    nodeKind = TEMPLATE_PROC;
    setFlag(IS_TEMPLATE, true);
}

std::string & TemplateProc::getMangledName() { return mangledName; }
void TemplateProc::setMangledName(std::string _mangledName) {
    mangledName = _mangledName;
}

ASTNode * TemplateProc::getTemplate() const { return _template; }
void TemplateProc::setTemplate(ASTNode * __template) {
    _template = __template;
    _template->parent = this;
    _template->replace = rpget<replacementPolicy_TemplateProc_Template>();
}

ASTNode * TemplateProc::getTemplateDef() const { return templateDef; }
void TemplateProc::setTemplateDef(ASTNode * _templateDef) {
    templateDef = _templateDef;
    templateDef->parent = this;
    templateDef->replace = rpget<replacementPolicy_TemplateProc_TemplateDef>();
}

// Node interface
void TemplateProc::analyze(bool force) {
    HANDLE_FORCE();
    setFlag(ANALYZED, true);
}

ASTNode * TemplateProc::clone() {
    TemplateProc * c = new TemplateProc(*this);

    // if (c->getTemplate())
    c->setTemplate(c->getTemplate()->clone());

    // if (c->getTemplateDef())
    c->setTemplateDef(c->getTemplateDef()->clone());

    return c;
}

void TemplateProc::addSymbols(Scope * _scope) {
    setScope(_scope);
    Procedure * procedureTemplate = (Procedure *)getTemplate();
    // procedureTemplate->addSymbols(_scope);

    _Symbol<TemplateProc> * symbol =
        new _Symbol<TemplateProc>(procedureTemplate->getName(), this);
    setMangledName(symbol->mangled(_scope)->name);
    _scope->addSymbol(symbol, &procedureTemplate->getNameContext());
}

TemplateProc::~TemplateProc() {
    if (_template)
        delete _template;
    if (templateDef)
        delete templateDef;
}
//

// ~~~~~ SLComment ~~~~~

SLComment::SLComment() : contents({}) { nodeKind = SL_COMMENT; }

std::string & SLComment::getContents() { return contents; }
void SLComment::setContents(std::string _contents) { contents = _contents; }

// Node interface
void SLComment::analyze(bool force) {
    HANDLE_FORCE();
    setFlag(ANALYZED, true);
}

ASTNode * SLComment::clone() { return new SLComment(*this); }

void SLComment::addSymbols(Scope * _scope) { setScope(_scope); }

SLComment::~SLComment() {}
//

// ~~~~~ ModuleDeclaration ~~~~~

ModuleDeclaration::ModuleDeclaration() : identifier({}) {
    nodeKind = MODULE_DECL;
}

std::string & ModuleDeclaration::getIdentifier() { return identifier; }
void ModuleDeclaration::setIdentifier(std::string _identifier) {
    identifier = _identifier;
}

// Node interface
void ModuleDeclaration::analyze(bool force) {
    HANDLE_FORCE();

    compilation->mode = Compilation::Mode::MODULE;

    setFlag(ANALYZED, true);
}

ASTNode * ModuleDeclaration::clone() { return new ModuleDeclaration(*this); }

void ModuleDeclaration::addSymbols(Scope * _scope) { setScope(_scope); }

ModuleDeclaration::~ModuleDeclaration() {}
//

// ~~~~~ IgnoreNode ~~~~~

IgnoreNode::IgnoreNode() { nodeKind = IGNORE; }

// Node interface
void IgnoreNode::analyze(bool force) {
    HANDLE_FORCE();

    setFlag(ANALYZED, true);
}

ASTNode * IgnoreNode::clone() { return new IgnoreNode(*this); }

void IgnoreNode::addSymbols(Scope * _scope) { setScope(_scope); }

void * IgnoreNode::generate(BackEnd & backEnd, bool flag) { return nullptr; }

IgnoreNode::~IgnoreNode() {}
//

//

// ~~~~~ MacroUse ~~~~~

MacroUse::MacroUse() : macroName({}) {
    nodeKind = MACRO_USE;
    // setFlag(ASTNode::CT, true);
}

std::string & MacroUse::getMacroName() { return macroName; }
void MacroUse::setMacroName(std::string _macroName) { macroName = _macroName; }

std::vector<ASTNode *> & MacroUse::getArgs() { return args; }
void MacroUse::addArg(ASTNode * arg) {
    arg->parent = this;
    arg->replace = rpget<replacementPolicy_MacroUse_Arg>();
    args.push_back(arg);
}

// Node interface
const Type * MacroUse::getType() {
    analyze();
    return result_t;
}

void MacroUse::analyze(bool force) {
    HANDLE_FORCE();

    ASTNode * r = compilation->frontEnd.macroManager.invoke(this);

    if (!r) {
        r = new IgnoreNode();
        r->setContext(getContext());
    }

    if (parent && !parent->nodeKind == MACRO_USE && !replace->canReplace(r)) {
        std::string expected;
        for (int & k : replace->allowed_nodeKinds) {
            expected += compilation->frontEnd.kind2string[(ASTNode::NodeKind)k];
            if (k != replace->allowed_nodeKinds.back())
                expected += ", ";
        }
        errorl(getContext(),
               "macro does not substitute to the correct AST node kind", true,
               "expected: " + expected,
               "got: " + compilation->frontEnd.kind2string[r->nodeKind]);
    }

    (*replace)(parent, this, r);

    result_t = r->getType();
    r->analyze();

    setFlag(ANALYZED, true);
}

ASTNode * MacroUse::clone() {
    MacroUse * c = new MacroUse(*this);
    c->args.clear();
    for (ASTNode * arg : getArgs())
        c->addArg(arg->clone());
    return c;
}

void MacroUse::unwrap(std::vector<ASTNode *> & terminals) {
    for (ASTNode * arg : getArgs())
        arg->unwrap(terminals);
}

void MacroUse::addSymbols(Scope * _scope) {
    setScope(_scope);
    for (ASTNode * arg : getArgs())
        if (arg->nodeKind != STRUCT && arg->nodeKind != INTERFACE_DEF)
            arg->addSymbols(_scope);
}

MacroUse::~MacroUse() {}
//

} // namespace bjou
