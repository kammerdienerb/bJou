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
#include <sstream>
#include <iomanip>
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

static std::string identStripMod(std::string _s) {
    size_t p = 0;
    const char * s = _s.c_str();
    char c;
    while ((c = *s) && (isalnum(c) || c == '_' || c == '\''))    s++;
    if (*s     && (*s     == ':')
    &&  *(s+1) && (*(s+1) == ':')) {
        return std::string(s); 
    }
    return _s;
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

ASTNode * ASTNode::getParent() const {
    if (parent) {
        if (parent->nodeKind == ASTNode::MULTINODE)
            return parent->getParent();
        return parent;
    }

    return nullptr;
}

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

void ASTNode::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    BJOU_DEBUG_ASSERT(false);
}

const Type * ASTNode::getType() { return nullptr; }
bool ASTNode::isStatement() const { return false; }

#define HANDLE_FORCE()                                                         \
    if (getFlag(ANALYZED) && !force)                                           \
    return

MultiNode::MultiNode() : isModuleContainer(false) { nodeKind = MULTINODE; }

MultiNode::MultiNode(std::vector<ASTNode *> & _nodes)
    : isModuleContainer(false), nodes(_nodes) {
    nodeKind = MULTINODE;
    for (ASTNode * node : nodes) {
        node->parent = this;
        node->replace = rpget<replacementPolicy_MultiNode_Node>();
    }
}

void MultiNode::take(std::vector<ASTNode *> & _nodes) {
    nodes = std::move(_nodes);

    for (ASTNode * node : nodes) {
        node->parent = this;
        node->replace = rpget<replacementPolicy_MultiNode_Node>();
    }
}

void MultiNode::analyze(bool force) {
    for (ASTNode * node : nodes)
        node->analyze(force);
}

void MultiNode::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);

    if (!isModuleContainer) {
        for (ASTNode * node : nodes) {
            if (node->nodeKind != STRUCT)
                node->addSymbols(mod, _scope);
        }
    }
}

void MultiNode::unwrap(std::vector<ASTNode *> & terminals) {
    for (ASTNode * node : nodes)
        node->unwrap(terminals);
}

void MultiNode::flatten(std::vector<ASTNode *> & out) {
    for (ASTNode * node : nodes) {
        if (node->nodeKind == MULTINODE) {
            MultiNode * multi = (MultiNode *)node;
            multi->flatten(out);
        } else {
            out.push_back(node);
        }
    }
}

ASTNode * MultiNode::clone() {
    MultiNode * clone = new MultiNode(*this);
    return clone;
}

void MultiNode::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    for (ASTNode * node : nodes)
        node->dump(stream, level, dumpCT);
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
        Procedure * proc = set->get(getScope(), args, nullptr, nullptr, false);
        if (proc) {
            // need to check args here since there may only be one
            // overload that is returned without checking
            std::vector<const Type *> arg_types;
            for (ASTNode * expr : args->getExpressions())
                arg_types.push_back(expr->getType());

            const Type * compare_type =
                ProcedureType::get(arg_types, VoidType::get());

            if (argMatch(proc->getType(), compare_type)) {
                CallExpression * call = new CallExpression;

                call->setContext(getContext());
                call->setLeft(
                    stringToIdentifier(proc->getLookupName()));
                call->setRight(args);

                (*replace)(parent, this, call);

                call->addSymbols(mod, getScope());
                call->analyze();

                setType(call->getType());

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

    if (nodeKind == EXPR_BLOCK) {
        return false;
    }

    if (getFlag(Expression::IDENT)) {
        Maybe<Symbol *> m_sym =
            getScope()->getSymbol(getScope(), (Identifier *)this, &getContext(),
                                  true, true, false, false);
        Symbol * sym = nullptr;
        m_sym.assignTo(sym);
        BJOU_DEBUG_ASSERT(sym);

        if (sym->isVar())
            return true;
    } else {
        if (getFlag(Expression::TERMINAL))
            return false;

        // const char * assignableOps[] = {"[]", ".", "=", "??", "@"};

        if (nodeKind == SUBSCRIPT_EXPRESSION) {
            if (!getLeft()->getType()->isSlice())
                return true;
        } else if (nodeKind == ACCESS_EXPRESSION) {
            if (getLeft()->nodeKind == IDENTIFIER) {
                getLeft()->analyze();
                Identifier * ident = (Identifier *)getLeft();
                if (ident->resolved) {
                    if (ident->resolved->nodeKind == VARIABLE_DECLARATION)
                        return true;
                }
            }

            // @bad @incomplete
            // what about T.create = 12345?
            return true;
        } else if (nodeKind == ASSIGNMENT_EXPRESSION) {
            return true;
        } else if (nodeKind == DEREF_EXPRESSION) {
            return true;
        }
    }

    return false;
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

void Expression::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    ASTNode * left = getLeft();
    ASTNode * right = getRight();
    if (left)
        left->addSymbols(mod, scope);
    if (right)
        right->addSymbols(mod, scope);
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
    if ((dest_t->isRef() && conv(dest_t, expr->getType()))
    ||  (expr->getType()->isRef() && equal(dest_t, expr->getType()->unRef())))
        return;

    ASTNode * p = expr->getParent();

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

void AddExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " + ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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
        } else if (rt->isPointer() && equal(lt, rt)) {
            setType(IntType::get(Type::Sign::SIGNED, 64));
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

void SubExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " - ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
//

// ~~~~~ BSHLExpression ~~~~~

BSHLExpression::BSHLExpression() {
    contents = "bshl";

    nodeKind = BSHL_EXPRESSION;
}

bool BSHLExpression::isConstant() { return BinaryExpression::isConstant(); }

Val BSHLExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalBSHL(a, b, getType());
}

// Node interface
void BSHLExpression::analyze(bool force) {
    HANDLE_FORCE();

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    if (!lt->isInt() || !rt->isInt()) {
        errorl(getContext(),
               "Could not match '" + lt->getDemangledName() + "' with '" +
                   rt->getDemangledName() + "' using the operator '" +
                   contents + "'.",
               true, "operands of bitwise operations must be integer types");
    }

    const Type * t = conv(lt, rt);
    BJOU_DEBUG_ASSERT(t && t->isInt());

    t = IntType::get(Type::Sign::UNSIGNED, ((IntType *)t)->width);

    convertOperands(this, t);

    setType(t);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * BSHLExpression::clone() { return ExpressionClone(this); }

void BSHLExpression::dump(std::ostream & stream, unsigned int level,
                          bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " bshl ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
//

// ~~~~~ BSHRExpression ~~~~~

BSHRExpression::BSHRExpression() {
    contents = "bshr";

    nodeKind = BSHR_EXPRESSION;
}

bool BSHRExpression::isConstant() { return BinaryExpression::isConstant(); }

Val BSHRExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalBSHR(a, b, getType());
}

// Node interface
void BSHRExpression::analyze(bool force) {
    HANDLE_FORCE();

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    if (!lt->isInt() || !rt->isInt()) {
        errorl(getContext(),
               "Could not match '" + lt->getDemangledName() + "' with '" +
                   rt->getDemangledName() + "' using the operator '" +
                   contents + "'.",
               true, "operands of bitwise operations must be integer types");
    }

    const Type * t = conv(lt, rt);
    BJOU_DEBUG_ASSERT(t && t->isInt());

    t = IntType::get(Type::Sign::UNSIGNED, ((IntType *)t)->width);

    convertOperands(this, t);

    setType(t);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * BSHRExpression::clone() { return ExpressionClone(this); }

void BSHRExpression::dump(std::ostream & stream, unsigned int level,
                          bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " bshr ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void MultExpression::dump(std::ostream & stream, unsigned int level,
                          bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " * ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void DivExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " * ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void ModExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " % ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
//

static void convertAssignmentOperand(BinaryExpression * assign) {
    const Type * lt = assign->getLeft()->getType()->unRef();
    const Type * rt = assign->getRight()->getType()->unRef();

    if (!equal(lt, rt) && conv(lt, rt))
        emplaceConversion((Expression *)assign->getRight(), lt);
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
        BJOU_DEBUG_ASSERT(sym);

        if (!sym->isVar()) {
            errorl(expr->getLeft()->getContext(),
                   "Operand left of '" + contents +
                       "' operator must be assignable.");
        }

        sym->initializedInScopes.insert(expr->getScope());
    }

    expr->getLeft()->analyze();

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

void AssignmentExpression::dump(std::ostream & stream, unsigned int level,
                                bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " = ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void AddAssignExpression::dump(std::ostream & stream, unsigned int level,
                               bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " += ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void SubAssignExpression::dump(std::ostream & stream, unsigned int level,
                               bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " -= ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void MultAssignExpression::dump(std::ostream & stream, unsigned int level,
                                bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " *= ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void DivAssignExpression::dump(std::ostream & stream, unsigned int level,
                               bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " /= ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void ModAssignExpression::dump(std::ostream & stream, unsigned int level,
                               bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " %= ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void MaybeAssignExpression::dump(std::ostream & stream, unsigned int level,
                                 bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " ?? ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

    if (getContents() == "<" && opOverload())
        return;

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    comparisonSignWarn(getContext(), lt, rt);

    if (lt->isInt()) {
        if (rt->isInt() || rt->isFloat() || rt->isChar() || rt->isBool()) {
            const Type * dest_t = conv(lt, rt);
            convertOperands(this, dest_t);
        } else
            goto err;
    } else if (lt->isBool()) {
        if (rt->isInt() || rt->isFloat() || rt->isChar() || rt->isBool()) {
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
            // @bad: is this the right way to handle
            // comparison of pointer types?
            const Type * dest_t = conv(lt, rt);
            if (!dest_t)
                dest_t = conv(rt, lt);
            if (!dest_t)
                goto err;
            convertOperands(this, dest_t);
        } else 
            goto err;
    } else if (lt->isEnum() && lt == rt) {
        /* good */
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

void LssExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " < ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void LeqExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " <= ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

    if (opOverload())
        return;

    ((LssExpression *)this)->LssExpression::analyze(force);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * GtrExpression::clone() { return ExpressionClone(this); }

void GtrExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " > ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void GeqExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " >= ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

    if (opOverload())
        return;

    ((LssExpression *)this)->LssExpression::analyze(force);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * EquExpression::clone() { return ExpressionClone(this); }

void EquExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " == ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

    if (opOverload())
        return;

    ((LssExpression *)this)->LssExpression::analyze(force);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * NeqExpression::clone() { return ExpressionClone(this); }

void NeqExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " != ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
//

// ~~~~~ LogAndExpression ~~~~~

LogAndExpression::LogAndExpression() {
    contents = "and";

    nodeKind = LOG_AND_EXPRESSION;
}

Val LogAndExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalLogAnd(a, b, getType());
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

    if (!conv(_bool, lt))
        badop = getLeft();
    if (!conv(_bool, rt))
        badop = getRight();

    if (!equal(_bool, lt))
        emplaceConversion((Expression *)getLeft(), _bool);
    if (!equal(_bool, rt))
        emplaceConversion((Expression *)getRight(), _bool);

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

void LogAndExpression::dump(std::ostream & stream, unsigned int level,
                            bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " and ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
//

// ~~~~~ LogOrExpression ~~~~~

LogOrExpression::LogOrExpression() {
    contents = "or";

    nodeKind = LOG_OR_EXPRESSION;
}

Val LogOrExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalLogOr(a, b, getType());
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

void LogOrExpression::dump(std::ostream & stream, unsigned int level,
                           bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " or ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
//

// ~~~~~ BANDExpression ~~~~~

BANDExpression::BANDExpression() {
    contents = "band";

    nodeKind = BAND_EXPRESSION;
}

bool BANDExpression::isConstant() { return BinaryExpression::isConstant(); }

Val BANDExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalBAND(a, b, getType());
}

// Node interface
void BANDExpression::analyze(bool force) {
    HANDLE_FORCE();

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    if (!lt->isInt() || !rt->isInt()) {
        errorl(getContext(),
               "Could not match '" + lt->getDemangledName() + "' with '" +
                   rt->getDemangledName() + "' using the operator '" +
                   contents + "'.",
               true, "operands of bitwise operations must be integer types");
    }

    const Type * t = conv(lt, rt);
    BJOU_DEBUG_ASSERT(t && t->isInt());

    t = IntType::get(Type::Sign::UNSIGNED, ((IntType *)t)->width);

    convertOperands(this, t);

    setType(t);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * BANDExpression::clone() { return ExpressionClone(this); }

void BANDExpression::dump(std::ostream & stream, unsigned int level,
                          bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " band ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
//

// ~~~~~ BORExpression ~~~~~

BORExpression::BORExpression() {
    contents = "bor";

    nodeKind = BOR_EXPRESSION;
}

bool BORExpression::isConstant() { return BinaryExpression::isConstant(); }

Val BORExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalBOR(a, b, getType());
}

// Node interface
void BORExpression::analyze(bool force) {
    HANDLE_FORCE();

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    if (!lt->isInt() || !rt->isInt()) {
        errorl(getContext(),
               "Could not match '" + lt->getDemangledName() + "' with '" +
                   rt->getDemangledName() + "' using the operator '" +
                   contents + "'.",
               true, "operands of bitwise operations must be integer types");
    }

    const Type * t = conv(lt, rt);
    BJOU_DEBUG_ASSERT(t && t->isInt());

    t = IntType::get(Type::Sign::UNSIGNED, ((IntType *)t)->width);

    convertOperands(this, t);

    setType(t);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * BORExpression::clone() { return ExpressionClone(this); }

void BORExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " bor ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
//

// ~~~~~ BXORExpression ~~~~~

BXORExpression::BXORExpression() {
    contents = "bxor";

    nodeKind = BXOR_EXPRESSION;
}

bool BXORExpression::isConstant() { return BinaryExpression::isConstant(); }

Val BXORExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a, b;
    a = ((Expression *)getLeft())->eval();
    b = ((Expression *)getRight())->eval();
    return evalBXOR(a, b, getType());
}

// Node interface
void BXORExpression::analyze(bool force) {
    HANDLE_FORCE();

    getLeft()->analyze(force);
    getRight()->analyze(force);

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType()->unRef();

    if (!lt->isInt() || !rt->isInt()) {
        errorl(getContext(),
               "Could not match '" + lt->getDemangledName() + "' with '" +
                   rt->getDemangledName() + "' using the operator '" +
                   contents + "'.",
               true, "operands of bitwise operations must be integer types");
    }

    const Type * t = conv(lt, rt);
    BJOU_DEBUG_ASSERT(t && t->isInt());

    t = IntType::get(Type::Sign::UNSIGNED, ((IntType *)t)->width);

    convertOperands(this, t);

    setType(t);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * BXORExpression::clone() { return ExpressionClone(this); }

void BXORExpression::dump(std::ostream & stream, unsigned int level,
                          bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " bxor ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

    if (opOverload())
        return;

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

    if (lt->isSlice()) {
        if (!((Expression*)getLeft())->canBeLVal()) {
            errorl(getContext(), "Indexing into slice literal is invalid.");
        }
    }

    if (lt->isDynamicArray()) {
        if (!((Expression*)getLeft())->canBeLVal()) {
            errorl(getContext(), "Indexing into dynamic array literal is invalid.");
        }
    }

    if (lt->isPointer() || lt->isSlice() || lt->isDynamicArray()) {
        setType(lt->under());
    } else if (lt->isArray()) {
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

void SubscriptExpression::dump(std::ostream & stream, unsigned int level,
                               bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << "[";
    getRight()->dump(stream, level, dumpCT);
    stream << "]";
    stream << ")";
}

void SubscriptExpression::desugar() {
    if (getLeft()->getType()->unRef()->isSlice()) {
        CallExpression * call = new CallExpression;
        call->setContext(getContext());

        // __bjou_slice_subscript
        Identifier * __bjou_slice_subscript = new Identifier;
        __bjou_slice_subscript->setSymMod("__slice");
        __bjou_slice_subscript->setSymName("__bjou_slice_subscript");

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

        (*replace)(parent, this, call);

        call->addSymbols(mod, getScope());
        call->analyze();
    } else if (getLeft()->getType()->unRef()->isDynamicArray()) {
        CallExpression * call = new CallExpression;
        call->setContext(getContext());

        // __bjou_dynamic_array_subscript
        Identifier * __bjou_dynamic_array_subscript = new Identifier;
        __bjou_dynamic_array_subscript->setSymMod("__dynamic_array");
        __bjou_dynamic_array_subscript->setSymName(
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

        (*replace)(parent, this, call);

        call->addSymbols(mod, getScope());
        call->analyze();
    } else if (getLeft()->getType()->unRef()->isArray() &&
               compilation->frontEnd.abc) {
        const ArrayType * array_t = (const ArrayType*)getLeft()->getType()->unRef();

        CallExpression * call = new CallExpression;
        call->setContext(getContext());

        // __bjou_array_subscript
        Identifier * __bjou_array_subscript = new Identifier;
        __bjou_array_subscript->setSymMod("__array");
        __bjou_array_subscript->setSymName("__bjou_array_subscript");

        ArgList * r = new ArgList;
        // (array, len, index, filename, line, col)
        r->addExpression(getLeft());
        IntegerLiteral * len = new IntegerLiteral;
        len->setContents(std::to_string(array_t->width));
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

        (*replace)(parent, this, call);

        call->addSymbols(mod, getScope());
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
            proc = procSet->get(getScope(), args, l->getRight(), &args->getContext());
            /* ((Identifier *)getLeft())->qualified = proc->getLookupName(); */
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
                       demangledIdentifier((Identifier *)getLeft()) +
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

void CallExpression::dump(std::ostream & stream, unsigned int level,
                          bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << "(";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
    stream << ")";
}

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
                                               const Type * t,
                                               Identifier * ident) {
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
    proc_ident->setSymMod(struct_t->_struct->mod);
    std::string t_name = string_sans_mod(struct_t->_struct->getLookupName());
    proc_ident->setSymType(t_name);
    proc_ident->setSymName(ident->getSymName());

    if (ident->getRight()) {
        proc_ident->setRight(ident->getRight());
    }

    return proc_ident;
}

CallExpression * AccessExpression::nextCall() {
    if (getParent() && getParent()->nodeKind == CALL_EXPRESSION)
        return (CallExpression *)getParent();
    return nullptr;
}

int AccessExpression::handleThroughTemplate() {
    // Special case
    // Consider:
    //
    //          type Type$T {
    //              proc create(arg : T) : Type$T { ... }
    //          }
    //
    // We would like to be able to say Type.create(12345) which would treat
    // Type.create() as
    //
    //          proc create$T(arg : T) : Type$T { ... }
    //
    // thus using template deduction from arguments to return the correct type
    // Type$int.
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
        Maybe<Symbol *> m_sym = decl->getScope()->getSymbol(
            decl->getScope(), decl->getIdentifier(), &decl->getContext(),
            /* traverse = */ true, /* fail = */ false);
        Symbol * sym = nullptr;

        if (m_sym.assignTo(sym)) {
            if (sym->isTemplateType()) {
                Identifier * proc_ident = stringToIdentifier(r_id->getSymName());
                proc_ident->setSymType(decl->asString());
                    /* = createIdentifierFromAccess( */
                        /* this, (Declarator *)getLeft(), r_id); */

                (*this->replace)(parent, this, proc_ident);

                proc_ident->addSymbols(mod, getScope());
                proc_ident->analyze();
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
            if (getRight()->nodeKind == ASTNode::IDENTIFIER) {
                if (struct_t->constantMap.count(r_id->getSymName()) > 0) {
                    Expression * const_expr =
                        (Expression *)struct_t
                            ->constantMap[r_id->getSymName()]
                            ->getInitialization();
                    const_expr = (Expression *)const_expr->clone();
                    const_expr->setScope(getScope());
                    const_expr->setContext(getContext());
                    (*this->replace)(parent, this, const_expr);
                    setType(const_expr->getType());
                    return -1;
                } else if (struct_t->memberProcs.count(r_id->getSymName()) >
                           0) {
                    Identifier * proc_ident = createIdentifierFromAccess(
                        this, struct_t, r_id);
                    (*this->replace)(parent, this, proc_ident);
                    proc_ident->addSymbols(mod, getScope());
                    proc_ident->setFlag(ASTNode::CT, isCT(this));
                    proc_ident->analyze();

                    setType(proc_ident->type);
                    if (!type && !nextCall())
                        proc_ident->getType(); // identifier error
                    return -1;
                } else {
                    if (struct_t->memberIndices.count(r_id->getSymName()) >
                        0)
                        errorl(getRight()->getContext(),
                               "Type '" + lt->getDemangledName() +
                                   "' does not define a constant or procedure "
                                   "member named '" +
                                   r_id->getSymName() + "'.",
                               true,
                               "'" + r_id->getSymName() +
                                   "' is a member variable and can only be "
                                   "accessed by an instance of '" +
                                   lt->getDemangledName() + "'");
                    else
                        errorl(getRight()->getContext(),
                               "Type '" + lt->getDemangledName() +
                                   "' does not define a constant or procedure "
                                   "named '" +
                                   r_id->getSymName() + "'.");
                }
            } else
                errorl(getRight()->getContext(), "Expected identifier.");
        } else if (lt->isEnum()) {
            if (getRight()->nodeKind == ASTNode::IDENTIFIER) {
                EnumType * e_t = (EnumType*)lt;
                IntegerLiteral * lit = e_t->getValueLiteral(r_id->getSymName(), getContext(), getScope());
                
                if (!lit) {
                    errorl(getRight()->getContext(), "'" + r_id->getSymName() + "' is not named in enum '" + e_t->getDemangledName() + "'.");
                }

                lit->analyze();
                lit->setType(e_t);

                (*this->replace)(parent, this, lit);
                setType(e_t);
                return -1;
            } else
                errorl(getRight()->getContext(), "Expected identifier."); // @bad error message
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

    if (lt->isDynamicArray()) {
        lt = ((DynamicArrayType*)lt)->getRealType();
    } else if (lt->isSlice()) {
        lt = ((SliceType*)lt)->getRealType();
    }

    // check for regular access
    if (lt->isStruct()
    || (lt->isPointer() && lt->under()->isStruct())) {

        StructType * struct_t = (StructType *)lt; // can't have constness here
        if (lt->isPointer())
            struct_t = (StructType *)lt->under();

        if (getRight()->nodeKind == ASTNode::IDENTIFIER) {
            // don't analyze here else we probably get 'use of undeclared
            // identifier' r_id->analyze(force);
            if (struct_t->memberIndices.count(r_id->getSymName()) > 0) {
                setType(struct_t->memberTypes
                            [struct_t->memberIndices[r_id->getSymName()]]);
                if (nextCall())
                    return -1;
                else return 1;
            } else if (struct_t->constantMap.count(r_id->getSymName()) >
                       0) {
                setType(
                    struct_t->constantMap[r_id->getSymName()]->getType());
                return 1;
            } else if (struct_t->memberProcs.count(r_id->getSymName()) >
                       0) {
                Identifier * proc_ident = createIdentifierFromAccess(
                    this, struct_t, r_id);
                (*getRight()->replace)(this, getRight(),
                                       proc_ident); // @lol this works
                proc_ident->addSymbols(mod, getScope());
                proc_ident->setFlag(ASTNode::CT, isCT(this));
                proc_ident->analyze();
                if (!nextCall())
                    setType(proc_ident->getType());
                // don't return.. fall to check handleInjection()
                return 1;
            } else if (struct_t->inheritedProcsToBaseStructType.count(r_id->getSymName()) > 0) {
                const StructType * extends_t = struct_t->inheritedProcsToBaseStructType[r_id->getSymName()];
                BJOU_DEBUG_ASSERT(extends_t);

                Identifier * proc_ident = createIdentifierFromAccess(
                    this, extends_t, r_id);
                (*getRight()->replace)(this, getRight(),
                                       proc_ident); // @lol this works
                proc_ident->addSymbols(mod, getScope());
                proc_ident->setFlag(ASTNode::CT, isCT(this));
                proc_ident->analyze();
                if (!nextCall())
                    setType(proc_ident->getType());
                // don't return.. fall to check handleInjection()
                return 1;
            } else if (!next_call)
                errorl(getRight()->getContext(),
                       "No member named '" + r_id->getSymName() + "' in '" +
                           struct_t->getDemangledName() + "'.");
        } else
            errorl(getRight()->getContext(), "Invalid structure accessor.",
                   true, "expected member name");
    } else if (lt->isTuple() || (lt->isPointer() && lt->under()->isTuple())) {
        TupleType * tuple_t = (TupleType *)lt;

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

    if (getLeft()->analyze(force), (i = handleThroughTemplate())) {
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

    if (i == 0 || i == 1) {
        if (handleInjection()) {
            setFlag(ANALYZED, true);
            return;
        } else if (i == 0) {
            errorl(getContext(),
                   "Access using the '.' operator does not apply to type '" + 
                    getLeft()->getType()->getDemangledName() + "'.");
        }
    }

    setFlag(ANALYZED, true);
}

ASTNode * AccessExpression::clone() { return ExpressionClone(this); }

void AccessExpression::dump(std::ostream & stream, unsigned int level,
                            bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << ".";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}

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

    getRight()->analyze();
    const Type * r_t = getRight()->getType();

    if (r_t->isArray())
        setType(r_t->under()->getPointer());
    else
        setType(r_t->getPointer());

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * NewExpression::clone() { return ExpressionClone(this); }

void NewExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    stream << "new ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void DeleteExpression::dump(std::ostream & stream, unsigned int level,
                            bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    stream << "delete ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
//

// ~~~~~ SizeofExpression ~~~~~

SizeofExpression::SizeofExpression() {
    contents = "sizeof";

    nodeKind = SIZEOF_EXPRESSION;
}

bool SizeofExpression::isConstant() { return true; }

Val SizeofExpression::eval() {
    analyze();
    Val a;
    a.t = getType();
    a.as_i64 = simpleSizer(getType());
    return a;
}

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

void SizeofExpression::dump(std::ostream & stream, unsigned int level,
                            bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    stream << "sizeof ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

    if (!conv(_bool, rt))
        errorl(getRight()->getContext(),
               "Operand right operator '" + getContents() +
                   "' must be convertible to type 'bool'.",
               true, "got '" + rt->getDemangledName() + "'");

    if (!equal(_bool, rt))
        emplaceConversion((Expression*)getRight(), _bool);

    setType(_bool);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * NotExpression::clone() { return ExpressionClone(this); }

void NotExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    stream << "not ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
//

// ~~~~~ BNEGExpression ~~~~~

BNEGExpression::BNEGExpression() {
    contents = "bneg";

    nodeKind = BNEG_EXPRESSION;
}

bool BNEGExpression::isConstant() { return UnaryPreExpression::isConstant(); }

Val BNEGExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a;
    a = ((Expression *)getRight())->eval();
    return evalBNEG(a, getType());
}

// Node interface
void BNEGExpression::analyze(bool force) {
    HANDLE_FORCE();

    getRight()->analyze(force);

    const Type * rt = getRight()->getType()->unRef();

    if (!rt->isInt()) {
        errorl(getContext(),
               "Could not match '" + rt->getDemangledName() +
                   "' with the operator '" + contents + "'.",
               true, "operands of bitwise operations must be integer types");
    }

    const Type * t = IntType::get(Type::Sign::UNSIGNED, ((IntType *)rt)->width);

    setType(t);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * BNEGExpression::clone() { return ExpressionClone(this); }

void BNEGExpression::dump(std::ostream & stream, unsigned int level,
                          bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    stream << "bneg ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void DerefExpression::dump(std::ostream & stream, unsigned int level,
                           bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    stream << "@";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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
                                                sym->unmangled +
                                                "' is ambiguous."); // @bad
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

void AddressExpression::dump(std::ostream & stream, unsigned int level,
                             bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    stream << "&";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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
            errorl(right->getContext(), "Use of '" + sym->unmangled +
                                            "' is ambiguous."); // @bad
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

void RefExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    stream << "~";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

Val AsExpression::eval() {
    if (!isConstant()) {
        errorl(getContext(), "Cannot evaluate non-constant expression.", false);
        internalError("There was an expression evaluation error.");
    }
    analyze();
    Val a;
    a = ((Expression *)getLeft())->eval();
    a.t = getType();
    return a;
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
            (conv(rt, lt) ||
             // (lt->isInt() && rt->isPointer()) || // for a NULL
             (lt->isPointer() && rt->isPointer()) ||
             (lt->isArray() && rt->isPointer() &&
              (equal(lt->under(), rt->under()) ||
               rt->under() == VoidType::get())) ||
             (lt->isPointer() && rt->isProcedure()) || // @temporary
             (lt->isProcedure() && rt->isPointer())    // @temporary
             ||(lt->isInt() && rt->isPointer() && rt->under()->isVoid()) || (lt->isPointer() && lt->under()->isVoid() && rt->isInt())))) {
        errorl(getContext(), "Invalid cast: '" + lt->getDemangledName() +
                                 "' to '" + rt->getDemangledName() + "'.");
    }

    setType(rt);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * AsExpression::clone() { return ExpressionClone(this); }

void AsExpression::dump(std::ostream & stream, unsigned int level,
                        bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getLeft()->dump(stream, level, dumpCT);
    stream << " as ";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
//

// ~~~~~ Identifier ~~~~~

Identifier::Identifier() : sym_name({}), sym_mod({}), sym_type({}), resolved(nullptr) {
    nodeKind = IDENTIFIER;
    setFlag(IDENT, true);
    setFlag(TERMINAL, true);
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

std::string & Identifier::getSymName() { return sym_name; }
void Identifier::setSymName(std::string _name) {
    sym_name = _name;
}

std::string & Identifier::getSymMod() { return sym_mod; }
void Identifier::setSymMod(std::string _mod) {
    sym_mod = _mod;
}

std::string & Identifier::getSymType() { return sym_type; }
void Identifier::setSymType(std::string _type) {
    sym_type = _type;
}

std::string Identifier::symAll() const {
    std::string r;

    if (!sym_mod.empty()) {
        r += sym_mod;
        r += "::";
    }
    if (!sym_type.empty()) {
        r += sym_type;
        r += ".";
    }
    r += sym_name;

    return r;
}

// Node interface
const Type * Identifier::getType() {
    analyze();

    if (!type) {
        Maybe<Symbol *> m_sym;
        Symbol * sym = nullptr;
        m_sym = getScope()->getSymbol(getScope(), this, &this->getContext());
        m_sym.assignTo(sym);
        BJOU_DEBUG_ASSERT(sym);

        if (sym->isProcSet()) {
            ProcSet * set = (ProcSet *)sym->node();
            ASTNode * proc = set->get(getScope(), nullptr, getRight(), &getContext());
            BJOU_DEBUG_ASSERT(proc && proc->nodeKind == ASTNode::PROCEDURE);
            setType(proc->getType());
        } else {
            if (sym->isType() && getParent() && getParent()->getParent()
            && (IS_EXPRESSION(getParent()->getParent()) || getParent()->getParent()->nodeKind == ARG_LIST)) {
                if (getParent()->getParent()->nodeKind == ASTNode::ACCESS_EXPRESSION) {
                    setType(sym->node()->getType());
                    return type;
                }
            }

            errorl(getContext(),
                   "Type declarator '" + sym->unmangled +
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
    m_sym = getScope()->getSymbol(getScope(), this, &this->getContext());
    m_sym.assignTo(sym);
    BJOU_DEBUG_ASSERT(sym);

    if (sym->isVar() || sym->isConstant() || sym->isProc()) {
        setType(sym->node()->getType());
    } else if (sym->isProcSet()) {
        bool call_independent = true;

        /* if this identifier is part of a call, the CallExpression
         * will be responsible for checking it in the set with args, etc.
         * Otherwise, we should fail here if we can't trivially deduce it
         * from the set. */
        ASTNode * p = getParent();
        if (p) {
            if (p->nodeKind == ASTNode::CALL_EXPRESSION) {
                CallExpression * call = (CallExpression*)p;
                if (this == call->getLeft()) {
                    call_independent = false;
                }
            } else if (p->nodeKind == ASTNode::ACCESS_EXPRESSION) {
                AccessExpression * access = (AccessExpression*)p;
                if (access->nextCall())
                    call_independent = false;
            }
        }

        ProcSet * set = (ProcSet *)sym->node();

        if (set->procs.size() == 1 && call_independent) {
            Symbol * first = set->procs.begin()->second;
            if (first->isTemplateProc()
            && (!getRight() || getRight()->nodeKind != ASTNode::TEMPLATE_INSTANTIATION)) {
                errorl(getContext(),
                        "Missing template instantiation arguments for template proc.");
            }
        }

        Procedure * proc =
            set->get(getScope(), nullptr, getRight(), &getContext(),
                     call_independent);
        if (proc) {
            resolved = proc;
            setType(proc->getType());
        }
    } else if (sym->isType() || sym->isTemplateType()) {
        if (parent && (IS_EXPRESSION(parent) || parent->nodeKind == ARG_LIST)) {
            if (parent->nodeKind != ASTNode::ACCESS_EXPRESSION)
                errorl(getContext(), "Can't use type name '" + symAll() +
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
        setFlag(TERMINAL, false);
    }

    if (!resolved && sym && !sym->isProcSet())
        resolved = sym->node();

    if (resolved) {
        if (!isCT(this) && isCT(resolved))
            errorl(getContext(), "Referenced symbol '" + symAll() +
                                     "' is only available at compile time.");
    }

    // Identifier get's its type lazily because it might be a reference
    // to an overloaded proc
    // BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

void Identifier::addSymbols(std::string& _mod, Scope * _scope) {
    Expression::addSymbols(_mod, _scope);

    ASTNode * p = getParent();
    if (p) {
        if (p->nodeKind == ASTNode::ACCESS_EXPRESSION) {
            if (((AccessExpression*)p)->getRight() == this)
                return;
        }
    }
    Maybe<Symbol *> m_sym;
    Symbol * sym = nullptr;
    m_sym = getScope()->getSymbol(getScope(), this, &this->getContext(), true, false, false, false);
    if (!m_sym) {
        compilation->frontEnd.idents_out_of_order.push_back(this);
    }
}

ASTNode * Identifier::clone() { return ExpressionClone(this); }

void Identifier::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << symAll();
    if (getRight())
        getRight()->dump(stream, level, dumpCT);
}

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

void InitializerList::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    Expression::addSymbols(mod, _scope);
    if (getObjDeclarator())
        getObjDeclarator()->addSymbols(mod, _scope);
    for (ASTNode * expr : getExpressions())
        expr->addSymbols(mod, _scope);
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
            if (!conv(mt, expr_t))
                errorl(expressions[i]->getContext(),
                       "Element for '" + name + "' in '" +
                           s_t->getDemangledName() +
                           "' literal differs from expected type '" +
                           mt->getDemangledName() + "'.",
                       true, "got '" + expr_t->getDemangledName() + "'");
            if (!equal(mt, expr_t))
                emplaceConversion((Expression *)expressions[i], mt);
        }

        for (ASTNode * _mem : s_t->_struct->getMemberVarDecls()) {
            VariableDeclaration * mem = (VariableDeclaration *)_mem;
            std::string & name = mem->getName();
            auto search = std::find(names.begin(), names.end(), name);
            if (search == names.end()) {
                if (mem->getType()->isRef()) {
                    errorl(getContext(), "Member '" + name + "' of '" +
                                             s_t->getDemangledName() +
                                             "' must be explicitly initialized "
                                             "because it is a reference.");
                } else if (mem->getType()->isStruct()) {
                    if (((StructType*)mem->getType())->containsRefs()) {
                        errorl(getContext(), "Member '" + name + "' of '" +
                                                 s_t->getDemangledName() +
                                                 "' must be explicitly initialized "
                                                 "because it contains one or more references.");
                    }
                }
            }
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

void InitializerList::dump(std::ostream & stream, unsigned int level,
                           bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "{ ";
    if (getObjDeclarator()) {
        getObjDeclarator()->dump(stream, level, dumpCT);
        stream << ": ";

        std::vector<std::string> & names = getMemberNames();
        std::vector<ASTNode *> & expressions = getExpressions();
        BJOU_DEBUG_ASSERT(names.size() == expressions.size());

        for (int i = 0; i < names.size(); i += 1) {
            stream << "." << names[i] << " = ";
            expressions[i]->dump(stream, level, dumpCT);
            if (i < names.size() - 1)
                stream << ", ";
        }
    } else {
        for (ASTNode * expr : getExpressions()) {
            expr->dump(stream, level, dumpCT);
        }
    }
    stream << " }";
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

void SliceExpression::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    src->addSymbols(mod, _scope);
    start->addSymbols(mod, _scope);
    length->addSymbols(mod, _scope);
}

void SliceExpression::analyze(bool force) {
    HANDLE_FORCE();

    getSrc()->analyze(force);
    getStart()->analyze(force);
    getLength()->analyze(force);

    const Type * src_t = getSrc()->getType()->unRef();
    const Type * start_t = getStart()->getType()->unRef();
    const Type * length_t = getLength()->getType()->unRef();

    if (!src_t->isPointer() && !src_t->isArray() && !src_t->isSlice() &&
        !src_t->isDynamicArray())
        errorl(getSrc()->getContext(),
               "Slice source must be either a pointer, an array, a dynamic "
               "array, or another "
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

void SliceExpression::dump(std::ostream & stream, unsigned int level,
                           bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "[";
    getSrc()->dump(stream, level, dumpCT);
    stream << ", ";
    getStart()->dump(stream, level, dumpCT);
    stream << ":";
    getLength()->dump(stream, level, dumpCT);
    stream << "]";
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
    create->setSymName("create");
    AccessExpression * l = new AccessExpression;
    l->setLeft(slice_decl);
    l->setRight(create);

    // (src, start, len)
    ArgList * r = new ArgList;
    if (getSrc()->getType()->isDynamicArray()) {
        AccessExpression * access = new AccessExpression;
        access->setContext(getSrc()->getContext());

        Identifier * __data = new Identifier;
        __data->setContext(getSrc()->getContext());
        __data->setSymName("__data");

        access->setLeft(getSrc());
        access->setRight(__data);

        access->setType(getSrc()->getType()->under()->getPointer());
        access->setFlag(ANALYZED, true);

        r->addExpression(access);
    } else if (getSrc()->getType()->isSlice()) {
        AccessExpression * access = new AccessExpression;
        access->setContext(getSrc()->getContext());

        Identifier * __data = new Identifier;
        __data->setContext(getSrc()->getContext());
        __data->setSymName("__data");

        access->setLeft(getSrc());
        access->setRight(__data);

        access->setType(getSrc()->getType()->under()->getPointer());
        access->setFlag(ANALYZED, true);

        r->addExpression(access);
    } else
        r->addExpression(getSrc()->clone());

    r->addExpression(getStart()->clone());
    r->addExpression(getLength()->clone());

    call->setLeft(l);
    call->setRight(r);

    call->addSymbols(mod, getScope());

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

void DynamicArrayExpression::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    typeDeclarator->addSymbols(mod, _scope);
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

void DynamicArrayExpression::dump(std::ostream & stream, unsigned int level,
                                  bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "[...";
    getTypeDeclarator()->dump(stream, level, dumpCT);
    stream << "]";
}

void DynamicArrayExpression::desugar() {
    getTypeDeclarator()->desugar();

    CallExpression * call = new CallExpression;
    call->setContext(getContext());

    // __bjou_dynamic_array!(T).create
    Declarator * da_decl =
        ((DynamicArrayType *)getType())->getRealType()->getGenericDeclarator();
    Identifier * create = new Identifier;
    create->setSymName("create");
    AccessExpression * l = new AccessExpression;
    l->setLeft(da_decl);
    l->setRight(create);

    // ()
    ArgList * r = new ArgList;

    call->setLeft(l);
    call->setRight(r);

    call->addSymbols(mod, getScope());

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

void LenExpression::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    expr->addSymbols(mod, _scope);
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

void LenExpression::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "|";
    getExpr()->dump(stream, level, dumpCT);
    stream << "|";
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
        __len->setSymName("__len");

        Expression * l = (Expression *)getExpr();

        // l->setType(((SliceType *)expr_t)->getRealType());
        l->setType(expr_t);
        l->setFlag(ANALYZED, true);

        access->setLeft(l->clone());
        access->setRight(__len);

        replacement = access;
    } else if (expr_t->isDynamicArray()) {
        AccessExpression * access = new AccessExpression;
        access->setContext(getContext());

        Identifier * __len = new Identifier;
        __len->setContext(getContext());
        __len->setSymName("__used");

        Expression * l = (Expression *)getExpr();

        // l->setType(((DynamicArrayType *)expr_t)->getRealType());
        l->setType(expr_t);
        l->setFlag(ANALYZED, true);

        access->setLeft(l->clone());
        access->setRight(__len);

        replacement = access;
    } else
        BJOU_DEBUG_ASSERT(false);

    replacement->addSymbols(mod, getScope());
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

void BooleanLiteral::dump(std::ostream & stream, unsigned int level,
                          bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << getContents();
}
//

// ~~~~~ IntegerLiteral ~~~~~

IntegerLiteral::IntegerLiteral()
    : is_hex(false), is_signed(false), is_neg(false) { nodeKind = INTEGER_LITERAL; }

uint64_t IntegerLiteral::getAsUnsigned() {
    uint64_t u;
    std::stringstream ss;
    ss << getContents();
    ss >> u;
    return u;
}

int64_t IntegerLiteral::getAsSigned() {
    int64_t  i;
    uint64_t u;
    std::stringstream ss;

    if (is_hex) {
        ss << getContents();
        ss >> u;
        return (int64_t)u;
    } else {
        ss << getContents();

        ss >> i;
        return i;
    }
}

bool IntegerLiteral::isConstant() { return true; }
Val IntegerLiteral::eval() {
    Val v;
    analyze();
    v.t = getType();
    v.as_i64 = getAsSigned();
    return v;
}

static bool uintFitsInWidth(uint64_t u, unsigned width) {
    switch (width) {
        case 8:  if (u > UINT8_MAX)     return false;
                 break;
        case 16: if (u > UINT16_MAX)    return false;
                 break;
        case 32: if (u > UINT32_MAX)    return false;
                 break;
        case 64: if (u > UINT64_MAX)    return false;
                 break;
        default:
            BJOU_DEBUG_ASSERT(false && "invalid width");
            return false;
    }
    return true;
}

static bool intFitsInWidth(int64_t i, unsigned width) {
    switch (width) {
        case 8:  if (i > INT8_MAX || i < INT8_MIN)      return false;
                 break;
        case 16: if (i > INT16_MAX || i < INT16_MIN)    return false;
                 break;
        case 32: if (i > INT32_MAX || i < INT32_MIN)    return false;
                 break;
        case 64: if (i > INT64_MAX || i < INT64_MIN)    return false;
                 break;
        default:
            BJOU_DEBUG_ASSERT(false && "invalid width");
            return false;
    }
    return true;
}

void IntegerLiteral::analyze(bool force) {
    HANDLE_FORCE();

    /* @bad @hack
     * If this came from an enum value
     * (via AccessExpression::handleAccessThroughDeclarator()), this
     * node could be forced to run analysis again. If that happens, we'll
     * lose the EnumType.
     */
    if (type && type->isEnum())    { return; }

    const std::string& con = getContents();
    if (con.size() > 2 &&
        con[0] == '0'  &&
        con[1] == 'x') {

        is_hex = true;
    }

    auto pos = getContents().find("i");
    if (pos == std::string::npos)
        pos = getContents().find("u");
    if (pos != std::string::npos) {
        suffix = getContents().substr(pos, getContents().size() - pos);
        setContents(getContents().substr(0, pos));
    }

    Type::Sign sign = Type::Sign::SIGNED;
    unsigned int width = 32;

    if (is_hex) {
        std::stringstream ss;
        std::string h = getContents();
        ss << std::hex << getContents();
        uint64_t val;
        ss >> val;
        setContents(std::to_string(val));

        sign = Type::Sign::UNSIGNED;
        is_signed = false;

        unsigned int bytes = (h.size() - 2) / 2;
        if (bytes == 0)
            bytes = 1;
        else if (bytes % 2 == 1)
            bytes += 1;

        width = bytes * 8;

        // next power of 2
        width--;
        width |= width >> 1;
        width |= width >> 2;
        width |= width >> 4;
        width |= width >> 8;
        width |= width >> 16;
        width++;
    }
   
    if (suffix.empty()) {
        std::stringstream ss(getContents());
        int i;
        ss >> i;
        is_neg = i < 0;
    } else {
        if (suffix[0] == 'u') {
            sign = Type::Sign::UNSIGNED;
        } else if (is_hex) {
            warningl(getContext(), "Ignoring signed suffix specification.", "Hex literals are unsigned.");
        }

        std::string w = suffix.c_str() + 1;
        std::stringstream ss(w);
        ss >> width;

        if (sign == Type::Sign::UNSIGNED && getContents()[0] == '-')
            errorl(getContext(),
                   "Literal is negative, but type suffix specifies unsigned.");

        ss.clear();

        if (sign == Type::Sign::UNSIGNED) {
            uint64_t u64;
            ss.str(getContents());

            ss >> u64;

            if (!uintFitsInWidth(u64, width)) {
                errorl(getContext(), "Literal value is invalid for type "
                                     "specified in its suffix.");
            }
        } else {
            int64_t i64;
            ss.str(getContents());

            ss >> i64;

            is_signed = true;
            is_neg    = i64 < 0;

            if (!intFitsInWidth(i64, width)) {
                errorl(getContext(), "Literal value is invalid for type "
                                     "specified in its suffix.");
            }
        }
    }

    setType(IntType::get(sign, width));

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * IntegerLiteral::clone() { return ExpressionClone(this); }

void IntegerLiteral::dump(std::ostream & stream, unsigned int level,
                          bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << getContents();
}
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

    if (getContents().find('e') != std::string::npos
    ||  getContents().find('E') != std::string::npos) {
        std::stringstream ss(getContents());
        double d; 
        ss >> d;
        setContents(std::to_string(d));
    }

    setType(FloatType::get(32));

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * FloatLiteral::clone() { return ExpressionClone(this); }

void FloatLiteral::dump(std::ostream & stream, unsigned int level,
                        bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << getContents();
}
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

    setType(CharType::get()->getPointer());

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * StringLiteral::clone() { return ExpressionClone(this); }

void StringLiteral::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "\"" << getContents() << "\"";
}
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

void CharLiteral::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << getContents();
}
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

    proc->analyze(force);

    const Type * rt = proc->getType();
    setType(rt);

    Identifier * proc_ident = new Identifier;
    proc_ident->setScope(getScope());
    proc_ident->setSymName(proc->getName());
    proc_ident->setFlag(Expression::TERMINAL, true);

    (*this->replace)(parent, this, proc_ident);

    proc_ident->addSymbols(mod, getScope());
    proc_ident->analyze();

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

void ProcLiteral::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    ASTNode * left = getLeft();
    ASTNode * right = getRight();
    if (left)
        left->addSymbols(mod, scope);
    if (right)
        right->addSymbols(mod, scope);
}

ASTNode * ProcLiteral::clone() { return ExpressionClone(this); }

void ProcLiteral::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void ExternLiteral::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    ASTNode * left = getLeft();
    ASTNode * right = getRight();
    if (left)
        left->addSymbols(mod, scope);
    if (right)
        right->addSymbols(mod, compilation->frontEnd.globalScope);
}

ASTNode * ExternLiteral::clone() { return ExpressionClone(this); }

void ExternLiteral::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    getRight()->dump(stream, level, dumpCT);
    stream << ")";
}
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

void SomeLiteral::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "some ";
    getRight()->dump(stream, level, dumpCT);
}
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

void NothingLiteral::dump(std::ostream & stream, unsigned int level,
                          bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "nothing";
}
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

void TupleLiteral::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    Expression::addSymbols(mod, _scope);
    for (ASTNode * sub : getSubExpressions())
        sub->addSymbols(mod, _scope);
}

void TupleLiteral::analyze(bool force) {
    HANDLE_FORCE();

    std::vector<const Type *> types;
    for (int i = 0; i < getSubExpressions().size(); i += 1) {
        getSubExpressions()[i]->analyze();
        types.push_back(getSubExpressions()[i]->getType());
    }

    setType(TupleType::get(types));

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * TupleLiteral::clone() {
    TupleLiteral * t = ExpressionClone(this);
    t->getSubExpressions().clear();
    std::vector<ASTNode *> & my_subExpressions = getSubExpressions();
    for (ASTNode * e : my_subExpressions)
        t->addSubExpression(e->clone());
    return t;
}

void TupleLiteral::dump(std::ostream & stream, unsigned int level,
                        bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    std::string comma = ", ";
    for (ASTNode *& expr : getSubExpressions()) {
        expr->dump(stream, level, dumpCT);
        if (&expr == &getSubExpressions().back())
            comma = "";
        stream << comma;
    }
    stream << ")";
}

void TupleLiteral::desugar() {
    for (ASTNode * expr : getSubExpressions())
        expr->desugar();
}
//

// ~~~~~ ExprBlock ~~~~~

ExprBlock::ExprBlock() { nodeKind = EXPR_BLOCK; }

bool ExprBlock::isConstant() { return false; }

std::vector<ASTNode *> & ExprBlock::getStatements() { return statements; }
void ExprBlock::setStatements(std::vector<ASTNode *> _statements) {
    statements = _statements;
}
void ExprBlock::addStatement(ASTNode * _statement) {
    _statement->parent = this;
    _statement->replace = rpget<replacementPolicy_ExprBlock_Statement>();
    statements.push_back(_statement);
}

void ExprBlock::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    Expression::addSymbols(mod, _scope);
    for (ASTNode * statement : getStatements())
        statement->addSymbols(mod, _scope);
}

void ExprBlock::analyze(bool force) {
    HANDLE_FORCE();

    for (ASTNode * statement : getStatements()) {
        statement->analyze(force);
    }

    if (yieldTypes.empty()) {
        errorl(getContext(), "Expression block does not yield any value.", true,
               "to yield a value: '<-the_value' in the expression block");
    }

    const Type * first_t = nullptr;

    for (auto & yt : yieldTypes) {
        if (!first_t) {
            first_t = yt.second;
        } else {
            if (first_t->isRef()) {
                Expression * expr = (Expression *)yt.first->getExpression();
                if (!expr->canBeLVal()) {
                    errorl(yt.first->getExpression()->getContext(),
                           "Type of expression block yield does not match "
                           "previous yields.",
                           false,
                           "expected '" + first_t->getDemangledName() + "'",
                           "got '" + yt.second->getDemangledName() + "'",
                           "value can't be used as a reference",
                           "Note: All expression yielded from an expression "
                           "block must be of compatible types.");
                    errorl(yieldTypes[0].first->getContext(),
                           "Expected yield type of '" +
                               first_t->getDemangledName() +
                               "' established by this yield:");
                }
            }
            if (!conv(first_t, yt.second)) {
                errorl(yt.first->getExpression()->getContext(),
                       "Type of expression block yield does not match previous "
                       "yields.",
                       false, "expected '" + first_t->getDemangledName() + "'",
                       "got '" + yt.second->getDemangledName() + "'",
                       "Note: All expression yielded from an expression block "
                       "must be of compatible types.");
                errorl(yieldTypes[0].first->getContext(),
                       "Expected yield type of '" +
                           first_t->getDemangledName() +
                           "' established by this yield:");
            }

            if (!equal(first_t, yt.second))
                emplaceConversion((Expression *)yt.first->getExpression(),
                                  first_t);
        }
    }

    setType(first_t);

    BJOU_DEBUG_ASSERT(type && "expression does not have a type");
    setFlag(ANALYZED, true);
}

ASTNode * ExprBlock::clone() {
    ExprBlock * t = ExpressionClone(this);
    std::vector<ASTNode *> & my_statements = getStatements();
    for (ASTNode * statement : my_statements)
        t->addStatement(statement->clone());
    return t;
}

void ExprBlock::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "<<\n";
    level += 1;
    for (ASTNode * statement : getStatements())
        statement->dump(stream, level);
    level -= 1;
    stream << std::string(4 * level, ' ');
    stream << ">>\n";
}

void ExprBlock::desugar() {
    for (ASTNode * statement : getStatements())
        statement->desugar();
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
    identifier->setFlag(Expression::TERMINAL, false);
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

    std::string s = asString();

    // Maybe<Symbol*> m_sym = getScope()->getSymbol(getScope(), getIdentifier(),
    // &getContext(), /* traverse = */true, /* fail = */false);
    Maybe<Symbol *> m_sym =
        getScope()->getSymbol(getScope(), getIdentifier(), &getContext(),
                              /* traverse = */ true, /* fail = */ false);
    Symbol * sym = nullptr;

    if (m_sym.assignTo(sym)) {
        if (!sym->isAlias()) {
            const Type * t = sym->node()->getType();
            BJOU_DEBUG_ASSERT(t);

            type = t;

            if (t->isStruct() || t->isEnum()) {
                if (getTemplateInst())
                    errorl(getTemplateInst()->getContext(),
                           "'" + sym->unmangled +
                               "' is not a template type.");
            } else if (sym->isTemplateType()) {
                if (!getTemplateInst())
                    errorl(getContext(),
                           "Missing template instantiation arguments for template type.");
                TemplateStruct * ttype = (TemplateStruct *)sym->node();
                Declarator * new_decl =
                    makeTemplateStruct(ttype, getTemplateInst())
                        ->getType()
                        ->getGenericDeclarator();
                new_decl->setScope(getScope());
                new_decl->setContext(getContext());
                (*replace)(parent, this, new_decl);
                new_decl->templateInst = nullptr;
                new_decl->analyze(true);
                type = new_decl->getType();
                return;
            } else if (!t->isPrimative()) {
                errorl(getContext(),
                       "'" + sym->unmangled + "' is not a type.");
            }
        }
    } else if (compilation->frontEnd.typeTable.count(s) == 0 ||
               !compilation->frontEnd.typeTable[s]->isPrimative())
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

void Declarator::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    getIdentifier()->dump(stream, level, dumpCT);
}

void Declarator::desugar() {
    getIdentifier()->desugar();
    if (getTemplateInst())
        getTemplateInst()->desugar();
}

const Type * Declarator::getType() {
    analyze();

    if (type)
        return type;

    if (compilation->frontEnd.primativeTypeTable.count(asString()) > 0)
        return compilation->frontEnd.primativeTypeTable[asString()];

    // Maybe<Symbol*> m_sym = getScope()->getSymbol(getScope(), identifier,
    // &getContext());
    Maybe<Symbol *> m_sym =
        getScope()->getSymbol(getScope(), getIdentifier(), &getContext());
    Symbol * sym = nullptr;
    m_sym.assignTo(sym);
    BJOU_DEBUG_ASSERT(sym);

    if (sym->isTemplateType())
        return PlaceholderType::get(); // @bad -- not intended use of
                                       // PlaceholderType
    return sym->node()->getType();
}

void Declarator::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    if (getTemplateInst())
        getTemplateInst()->addSymbols(mod, _scope);
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
std::string Declarator::asString() {
    return ((Identifier*)getIdentifier())->symAll();
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
    : expression(nullptr), size(-2) {
    nodeKind = ARRAY_DECLARATOR;
    setArrayOf(_arrayOf);
}

ArrayDeclarator::ArrayDeclarator(ASTNode * _arrayOf, ASTNode * _expression)
    : size(-2) {
    setArrayOf(_arrayOf);
    setExpression(_expression);
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

    if (getArrayOf()->getType()->isRef())
        errorl(getContext(), "Can't have an array of references because "
                             "references are not assignable.");

    Expression * expr = (Expression *)getExpression();
    expr->analyze();
    expr = (Expression *)getExpression(); // refresh
    if (expr->isConstant()) {
        Val v = expr->eval();
        size = (int)v.as_i64;
    } else {
        size = -1;
    }

    if (size == -1 && (!getParent() || getParent()->nodeKind != ASTNode::NEW_EXPRESSION)) {
        std::string errMsg = "Array lengths must be compile-time constants.";
            errorl(getExpression()->getContext(), errMsg, true,
                   "Note: For a dynamic array type, use '[...]'");
    }

    setFlag(ANALYZED, true);
}

ArrayDeclarator::~ArrayDeclarator() {
    BJOU_DEBUG_ASSERT(arrayOf);
    delete arrayOf;
    if (expression && !createdFromType)
        delete expression;
}

void ArrayDeclarator::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    getArrayOf()->addSymbols(mod, _scope);
    if (expression)
        expression->addSymbols(mod, scope);
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

void ArrayDeclarator::dump(std::ostream & stream, unsigned int level,
                           bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    getArrayOf()->dump(stream, level, dumpCT);
    stream << "[";
    getExpression()->dump(stream, level, dumpCT);
    stream << "]";
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
std::string ArrayDeclarator::asString() {
    return ((Declarator *)getArrayOf())->asString() + "[" + std::to_string(size) + "]";
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

SliceDeclarator::SliceDeclarator(ASTNode * _sliceOf) {
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

    if (getSliceOf()->getType()->isRef())
        errorl(getContext(), "Can't have a slice of references because "
                             "references are not assignable.");

    setFlag(ANALYZED, true);

    desugar();
}

SliceDeclarator::~SliceDeclarator() {
    BJOU_DEBUG_ASSERT(sliceOf);
    delete sliceOf;
}

void SliceDeclarator::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    getSliceOf()->addSymbols(mod, _scope);
}

void SliceDeclarator::unwrap(std::vector<ASTNode *> & terminals) {
    getSliceOf()->unwrap(terminals);
}

ASTNode * SliceDeclarator::clone() {
    SliceDeclarator * c = DeclaratorClone(this);

    c->setSliceOf(c->getSliceOf()->clone());

    return c;
}

void SliceDeclarator::dump(std::ostream & stream, unsigned int level,
                           bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    getSliceOf()->dump(stream, level, dumpCT);
    stream << "[]";
}

void SliceDeclarator::desugar() {
    /*
    getSliceOf()->desugar();

    SliceType * slice_t = (SliceType*)getType();

    Declarator * new_decl = slice_t->getRealType()->getGenericDeclarator();

    (*replace)(parent, this, new_decl);

    new_decl->addSymbols(mod, getScope());
    new_decl->analyze();
    */
}

const Type * SliceDeclarator::getType() {
    return SliceType::get(getSliceOf()->getType());
}

//

// Declarator interface
std::string SliceDeclarator::asString() {
    return ((Declarator *)getSliceOf())->asString() + "[]";
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

DynamicArrayDeclarator::DynamicArrayDeclarator(ASTNode * _arrayOf) {
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

    if (getArrayOf()->getType()->isRef())
        errorl(getContext(), "Can't have a dynamic array of references because "
                             "references are not assignable.");

    setFlag(ANALYZED, true);

    desugar();
}

DynamicArrayDeclarator::~DynamicArrayDeclarator() {
    BJOU_DEBUG_ASSERT(arrayOf);
    delete arrayOf;
}

void DynamicArrayDeclarator::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    getArrayOf()->addSymbols(mod, _scope);
}

void DynamicArrayDeclarator::unwrap(std::vector<ASTNode *> & terminals) {
    getArrayOf()->unwrap(terminals);
}

ASTNode * DynamicArrayDeclarator::clone() {
    DynamicArrayDeclarator * c = DeclaratorClone(this);

    c->setArrayOf(c->getArrayOf()->clone());

    return c;
}

void DynamicArrayDeclarator::dump(std::ostream & stream, unsigned int level,
                                  bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    getArrayOf()->dump(stream, level, dumpCT);
    stream << "[...]";
}

void DynamicArrayDeclarator::desugar() {
    /*
    getArrayOf()->desugar();

    DynamicArrayType * dyn_t = (DynamicArrayType*)getType();

    Declarator * new_decl = dyn_t->getRealType()->getGenericDeclarator();

    (*replace)(parent, this, new_decl);

    new_decl->addSymbols(mod, getScope());
    new_decl->analyze();
    */
}

const Type * DynamicArrayDeclarator::getType() {
    return DynamicArrayType::get(getArrayOf()->getType());
}

//

// Declarator interface
std::string DynamicArrayDeclarator::asString() {
    return ((Declarator *)getArrayOf())->asString() + "[...]";
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

PointerDeclarator::PointerDeclarator(ASTNode * _pointerOf) {
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

    if (getPointerOf()->getType()->isRef())
        errorl(getContext(), "Can't have a pointer to a reference because "
                             "references are not assignable.");

    setFlag(ANALYZED, true);
}

PointerDeclarator::~PointerDeclarator() {
    BJOU_DEBUG_ASSERT(pointerOf);
    delete pointerOf;
}

void PointerDeclarator::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    getPointerOf()->addSymbols(mod, _scope);
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

void PointerDeclarator::dump(std::ostream & stream, unsigned int level,
                             bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    getPointerOf()->dump(stream, level, dumpCT);
    stream << "*";
}

void PointerDeclarator::desugar() { getPointerOf()->desugar(); }

const Type * PointerDeclarator::getType() {
    Declarator * pointerOf = (Declarator *)getPointerOf();
    return PointerType::get(pointerOf->getType());
}

// Declarator interface
std::string PointerDeclarator::asString() {
    return ((Declarator *)getPointerOf())->asString() + "*";
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

    if (getRefOf()->getType()->isRef())
        errorl(getContext(), "Can't have a reference to a reference because "
                             "references are not assignable.");

    setFlag(ANALYZED, true);
}

RefDeclarator::~RefDeclarator() {
    BJOU_DEBUG_ASSERT(refOf);
    delete refOf;
}

void RefDeclarator::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    getRefOf()->addSymbols(mod, _scope);
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

void RefDeclarator::dump(std::ostream & stream, unsigned int level,
                         bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    getRefOf()->dump(stream, level, dumpCT);
    stream << " ref";
}

void RefDeclarator::desugar() { getRefOf()->desugar(); }

const Type * RefDeclarator::getType() {
    Declarator * refOf = (Declarator *)getRefOf();
    return RefType::get(refOf->getType());
}
//

// Declarator interface
std::string RefDeclarator::asString() {
    return ((Declarator *)getRefOf())->asString() + " ref";
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

void MaybeDeclarator::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    getMaybeOf()->addSymbols(mod, _scope);
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

void MaybeDeclarator::dump(std::ostream & stream, unsigned int level,
                           bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    getMaybeOf()->dump(stream, level, dumpCT);
    stream << "?";
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
std::string MaybeDeclarator::asString() {
    return ((Declarator *)getMaybeOf())->asString() + "?";
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

void TupleDeclarator::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    for (ASTNode * sd : getSubDeclarators())
        sd->addSymbols(mod, _scope);
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

void TupleDeclarator::dump(std::ostream & stream, unsigned int level,
                           bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "(";
    std::string comma = ", ";
    for (ASTNode *& sub : getSubDeclarators()) {
        sub->dump(stream, level, dumpCT);
        if (&sub == &getSubDeclarators().back())
            comma = "";
        stream << comma;
    }
    stream << ")";
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
std::string TupleDeclarator::asString() {
    const char * lazy_comma = "";
    std::string mangled = "(";
    std::vector<ASTNode *> & subDeclarators = getSubDeclarators();
    for (ASTNode * sd : subDeclarators) {
        mangled += lazy_comma + ((Declarator *)sd)->asString();
        lazy_comma = ", ";
    }
    mangled += ")";

    return mangled;
}

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

void ProcedureDeclarator::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    for (ASTNode * pd : paramDeclarators)
        pd->addSymbols(mod, _scope);
    BJOU_DEBUG_ASSERT(retDeclarator);
    retDeclarator->addSymbols(mod, _scope);
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

void ProcedureDeclarator::dump(std::ostream & stream, unsigned int level,
                               bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "<(";
    std::string comma = ", ";
    for (ASTNode *& p : getParamDeclarators()) {
        p->dump(stream, level, dumpCT);
        if (&p == &getParamDeclarators().back() &&
            !getFlag(ProcedureDeclarator::IS_VARARG))
            comma = "";
        stream << comma;
    }
    if (getFlag(ProcedureDeclarator::IS_VARARG))
        stream << "...";
    stream << ") : ";
    getRetDeclarator()->dump(stream, level, dumpCT);
    stream << ">";
}

void ProcedureDeclarator::desugar() {
    for (ASTNode * p : getParamDeclarators())
        p->desugar();
    getRetDeclarator()->desugar();
}

const Type * ProcedureDeclarator::getType() {
    std::vector<ASTNode *> & paramDeclarators = getParamDeclarators();
    std::vector<const Type *> paramTypes;
    paramTypes.reserve(paramDeclarators.size());

    for (ASTNode * node : paramDeclarators) {
        Declarator * decl = (Declarator*)node;
        paramTypes.push_back(decl->getType());
    }

    const Type * retType = ((Declarator *)getRetDeclarator())->getType();
    bool isVararg = getFlag(IS_VARARG);

    return ProcedureType::get(paramTypes, retType, isVararg);
}
//

// Declarator interface
std::string ProcedureDeclarator::asString() {
    const char * lazy_comma = "";
    std::string mangled = "<(";
    std::vector<ASTNode *> & subDeclarators = getParamDeclarators();
    for (ASTNode * sd : subDeclarators) {
        mangled += lazy_comma + ((Declarator *)sd)->asString();
        lazy_comma = ", ";
    }
    if (getFlag(ProcedureDeclarator::IS_VARARG)) {
        mangled += lazy_comma + std::string("...");
    }
    mangled += ")";
    ASTNode * retDeclarator = getRetDeclarator();
    if (retDeclarator) {
        mangled += " : ";
        mangled += ((Declarator *)retDeclarator)->asString();
    }
    mangled += ">";

    return mangled;
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

void PlaceholderDeclarator::addSymbols(std::string& _mod, Scope * _scope) { 
    mod = _mod;
    setScope(_scope);
}

void PlaceholderDeclarator::unwrap(std::vector<ASTNode *> & terminals) {
    terminals.push_back(this);
}

ASTNode * PlaceholderDeclarator::clone() { return DeclaratorClone(this); }

void PlaceholderDeclarator::dump(std::ostream & stream, unsigned int level,
                                 bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "_";
}

void PlaceholderDeclarator::desugar() {}

const Type * PlaceholderDeclarator::getType() { return PlaceholderType::get(); }
//

// Declarator interface
std::string PlaceholderDeclarator::asString() { return "_"; }

const ASTNode * PlaceholderDeclarator::getBase() const { return this; }
//

// ~~~~~ Constant ~~~~~

Constant::Constant()
    : name({}), lookupName({}), typeDeclarator(nullptr),
      initialization(nullptr) {
    nodeKind = CONSTANT;
}

std::string & Constant::getName() { return name; }
void Constant::setName(std::string _name) { name = _name; }

std::string & Constant::getLookupName() { return lookupName; }
void Constant::setLookupName(std::string _lookupName) {
    lookupName = _lookupName;
}

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

    BJOU_DEBUG_ASSERT(getInitialization());

    if (getTypeDeclarator()) {

        std::stack<const Type *> & lValStack = compilation->frontEnd.lValStack;

        const Type * t = getTypeDeclarator()->getType(); // @leak
        lValStack.push(t);

        if (t->isVoid()) {
            errorl(getTypeDeclarator()->getContext(),
                   "Cannot declare constant '" + getName() +
                       "' with type 'void'.");
        }

        if (!conv(t, getInitialization()->getType()))
            errorl(
                getInitialization()->getContext(),
                "Can't create '" + t->getDemangledName() + "'" + " constant " +
                    "'" + getName() + "' with expression of type '" +
                    getInitialization()->getType()->getDemangledName() + "'.");

        /* check if integer type initializers fit in the declarator type */
        const Type * decl_t = getTypeDeclarator()->getType();
        if (decl_t->isInt()) {
            IntType        * int_t          = (IntType*)decl_t;
            IntegerLiteral * lit            = nullptr;
            bool             constant       = false;
            std::string      constant_name;
           
            if (getInitialization()->nodeKind == ASTNode::INTEGER_LITERAL) {
                lit = (IntegerLiteral*)getInitialization();
            } else if (getInitialization()->nodeKind == ASTNode::IDENTIFIER) {
                Identifier * i = (Identifier*)getInitialization();
                if (i->resolved && i->resolved->nodeKind == ASTNode::CONSTANT) {
                    Constant * c = (Constant*)i->resolved;
                    if (c->getInitialization()->nodeKind == ASTNode::INTEGER_LITERAL) {
                        constant = true;
                        constant_name = c->name;
                        lit = (IntegerLiteral*)c->getInitialization();
                    }
                }
            }

            if (lit) {
                if (lit->is_neg && int_t->sign == Type::Sign::UNSIGNED) {
                    errorl(getInitialization()->getContext(), "Sign mismatch for initialization of constant '" + name + "'.", true, "Wanted unsigned but got negative initializer");
                }

                bool bad_int_init = false;

                if (lit->is_signed) {
                    uint64_t u = lit->getAsUnsigned();
                    if (!uintFitsInWidth(u, int_t->width))
                        bad_int_init = true;
                } else {
                    int64_t i = lit->getAsSigned();
                    if (!intFitsInWidth(i, int_t->width))
                        bad_int_init = true;
                }

                if (bad_int_init) {
                    if (constant) {
                        errorl(getInitialization()->getContext(), "Integer constant initializer from constant '" + constant_name + "' does not fit into the given type '" + int_t->getDemangledName() + "'.", true, "Has type '" + lit->getType()->getDemangledName() + "'");
                    } else {
                        errorl(getInitialization()->getContext(), "Integer literal constant initializer does not fit into the given type '" + int_t->getDemangledName() + "'.");
                    }
                }
            }
        }

        lValStack.pop();
    } else {
        const Type * t = getInitialization()->getType();

        if (t->isVoid()) {
            errorl(getInitialization()->getContext(),
                   "Cannot initialize constant '" + getName() +
                       "' with expression resulting in type 'void'.");
        }

        setTypeDeclarator(t->getGenericDeclarator());
        getTypeDeclarator()->addSymbols(mod, getScope());
    }

    if (getTypeDeclarator()->getType()->isVoid())
        errorl(getInitialization()->getContext(),
               "Contant '" + getName() + "' cannot be void.");

    if (!((Expression *)getInitialization())->isConstant())
        errorl(getInitialization()->getContext(),
               "Value for '" + getName() + "' is not constant.");

    Expression * init = (Expression *)getInitialization();
    Expression * as = nullptr;
    const Context& cxt = init->getContext();

    /* @bad: this only works through one layer of 'as' */
    if (getInitialization()->nodeKind == ASTNode::AS_EXPRESSION) {
        as   = init;
        init = (Expression *)as->getLeft();
    }
         
    if (!init->getType()->isProcedure()) {
        /* init->setType(getTypeDeclarator()->getType()); // @hack */
        ASTNode * folded = init->eval().toExpr();
        folded->setContext(cxt);
        folded->addSymbols(mod, getScope());
        folded->analyze();
            
        // @leak?
        if (as) {
            as->setLeft(folded); 
        } else {
            setInitialization(folded);
        }
    }

    setFlag(ANALYZED, true);
}

void Constant::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    if (!getFlag(IS_TYPE_MEMBER)) {
        _Symbol<Constant> * symbol = new _Symbol<Constant>(getName(), (_scope->parent && !_scope->is_module_scope) ? "" : mod, "", this);

        setLookupName(symbol->unmangled);
        setMangledName(symbol->real_mangled);
        
        _scope->addSymbol(symbol, &getNameContext());
    }
    if (getTypeDeclarator())
        getTypeDeclarator()->addSymbols(mod, _scope);
    getInitialization()->addSymbols(mod, _scope);
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

void Constant::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;

    BJOU_DEBUG_ASSERT(getInitialization());

    stream << std::string(4 * level, ' ');
    stream << "const " << getName();
    if (getTypeDeclarator()) {
        stream << " : ";
        getTypeDeclarator()->dump(stream, level, dumpCT);
        stream << " = ";
        getInitialization()->dump(stream, level, dumpCT);
    } else {
        BJOU_DEBUG_ASSERT(getInitialization());
        stream << " := ";
        getInitialization()->dump(stream, level, dumpCT);
    }
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
    : name({}), lookupName({}), typeDeclarator(nullptr),
      initialization(nullptr) {
    nodeKind = VARIABLE_DECLARATION;
}

std::string & VariableDeclaration::getName() { return name; }
void VariableDeclaration::setName(std::string _name) { name = _name; }

std::string & VariableDeclaration::getLookupName() { return lookupName; }
void VariableDeclaration::setLookupName(std::string _lookupName) {
    lookupName = _lookupName;
}

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
            getScope(), getLookupName(), &getNameContext(), true, true, false);
        m_sym.assignTo(sym);
        BJOU_DEBUG_ASSERT(sym);
    }

    if (!getFlag(IS_TYPE_MEMBER) && getScope()->is_module_scope) {
        sym->initializedInScopes.insert(compilation->frontEnd.globalScope);
    }

    if (!getFlag(IS_TYPE_MEMBER)
    &&  getFlag(IS_EXTERN)
    && (getScope()->parent && !getScope()->is_module_scope)) {
        errorl(getNameContext(), "External variable bindings must be made at global scope.");
    }

    if (getInitialization()) {
        const Type * init_t = nullptr;

        if (getFlag(IS_TYPE_MEMBER)) {
            errorl(getInitialization()->getContext(),
                   "Type member variables cannot be initialized.", true,
                   "did you mean to make '" + getName() + "' a constant?");
        }
        
        if (getFlag(IS_EXTERN)) {
            errorl(getInitialization()->getContext(),
                   "External variable bindings cannot be initialized.");
        }


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

            /* check if integer type initializers fit in the declarator type */
            if (decl_t->isInt()) {
                IntType        * int_t          = (IntType*)decl_t;
                IntegerLiteral * lit            = nullptr;
                bool             constant       = false;
                std::string      constant_name;
               
                if (getInitialization()->nodeKind == ASTNode::INTEGER_LITERAL) {
                    lit = (IntegerLiteral*)getInitialization();
                } else if (getInitialization()->nodeKind == ASTNode::IDENTIFIER) {
                    Identifier * i = (Identifier*)getInitialization();
                    if (i->resolved && i->resolved->nodeKind == ASTNode::CONSTANT) {
                        Constant * c = (Constant*)i->resolved;
                        if (c->getInitialization()->nodeKind == ASTNode::INTEGER_LITERAL) {
                            constant = true;
                            constant_name = c->name;
                            lit = (IntegerLiteral*)c->getInitialization();
                        }
                    }
                }

                if (lit) {
                    if (lit->is_neg && int_t->sign == Type::Sign::UNSIGNED) {
                        errorl(getInitialization()->getContext(), "Sign mismatch for initialization of variable '" + name + "'.", true, "Wanted unsigned but got negative initializer");
                    }

                    bool bad_int_init = false;

                    if (lit->is_signed) {
                        uint64_t u = lit->getAsUnsigned();
                        if (!uintFitsInWidth(u, int_t->width))
                            bad_int_init = true;
                    } else {
                        int64_t i = lit->getAsSigned();
                        if (!intFitsInWidth(i, int_t->width))
                            bad_int_init = true;
                    }

                    if (bad_int_init) {
                        if (constant) {
                            errorl(getInitialization()->getContext(), "Integer variable initializer from constant '" + constant_name + "' does not fit into the given type '" + int_t->getDemangledName() + "'.", true, "Has type '" + lit->getType()->getDemangledName() + "'");
                        } else {
                            errorl(getInitialization()->getContext(), "Integer literal variable initializer does not fit into the given type '" + int_t->getDemangledName() + "'.");
                        }
                    }
                }
            }

            if (!equal(decl_t, init_t))
                emplaceConversion((Expression *)getInitialization(), decl_t);

            lValStack.pop();
        } else {
            my_t = init_t = getInitialization()->getType();
            setTypeDeclarator(init_t->getGenericDeclarator());
            getTypeDeclarator()->addSymbols(mod, getScope());
        }

        sym->initializedInScopes.insert(getScope());

        if (!getScope()->parent || getScope()->is_module_scope) {
            if (!((Expression *)getInitialization())->isConstant()) {
                errorl(getInitialization()->getContext(),
                   "Global variable initializations must be compile time "
                   "constants.");
            }
        }
    } else {
        my_t = getTypeDeclarator()->getType();

        if (!getFlag(IS_PROC_PARAM) && !getFlag(IS_TYPE_MEMBER)) {
            if (getTypeDeclarator()->getType()->isRef()) {
                errorl(getContext(),
                       "'" + getName() +
                           "' is a reference and must be initialized.");
            } else if (!getScope()->parent || getScope()->is_module_scope) {
                if (my_t->isStruct()) {
                    if (((StructType*)my_t)->containsRefs()) {
                        errorl(getContext(),
                               "'" + getName() +
                                   "' must be initialized because it contains one or more reference.");
                    }
                }
            }
        }
    }

    BJOU_DEBUG_ASSERT(getTypeDeclarator() &&
                      "No typeDeclarator in VariableDeclaration");

    getTypeDeclarator()->analyze();

    if (my_t->isVoid()) {
        if (getInitialization()) {
            errorl(getInitialization()->getContext(),
                   "Cannot initialize variable '" + getName() +
                       "' with expression resulting in type 'void'.");
        } else {
            BJOU_DEBUG_ASSERT(getTypeDeclarator());
            errorl(getTypeDeclarator()->getContext(),
                   "Cannot declare variable '" + getName() +
                       "' with type 'void'.");
        }
    }

    if (getTypeDeclarator()->getType()->isArray() && sym) {
        // arrays don't require initialization before reference
        sym->initializedInScopes.insert(getScope());
    }

    BJOU_DEBUG_ASSERT(my_t);

    if (my_t->isStruct() && ((StructType *)my_t)->isAbstract)
        errorl(getTypeDeclarator()->getContext(),
               "Can't instantiate '" + my_t->getDemangledName() +
                   "', which is an abstract type.");
}

void VariableDeclaration::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);

    if (getTypeDeclarator())
        getTypeDeclarator()->addSymbols(mod, _scope);
    if (getInitialization())
        getInitialization()->addSymbols(mod, _scope);

    if (!getFlag(IS_TYPE_MEMBER)) {
        _Symbol<VariableDeclaration> * symbol =
            new _Symbol<VariableDeclaration>(getName(), (_scope->parent && !_scope->is_module_scope) ? "" : mod, "", this);

        setLookupName(symbol->unmangled);
        if (getFlag(VariableDeclaration::IS_EXTERN)) {
            setMangledName(symbol->unmangled);
        } else {
            setMangledName(symbol->real_mangled);
        }

        _scope->addSymbol(symbol, &getNameContext());
    }
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

void VariableDeclaration::dump(std::ostream & stream, unsigned int level,
                               bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');
    stream << getName();
    if (getTypeDeclarator()) {
        stream << " : ";
        getTypeDeclarator()->dump(stream, level, dumpCT);
        if (getInitialization()) {
            stream << " = ";
            getInitialization()->dump(stream, level, dumpCT);
        }
    } else {
        BJOU_DEBUG_ASSERT(getInitialization());
        stream << " := ";
        getInitialization()->dump(stream, level, dumpCT);
    }
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

Alias::Alias() : name({}), lookupName({}), declarator(nullptr) {
    nodeKind = ALIAS;
}

std::string & Alias::getName() { return name; }
void Alias::setName(std::string _name) { name = _name; }

std::string & Alias::getLookupName() { return lookupName; }
void Alias::setLookupName(std::string _lookupName) {
    lookupName = _lookupName;
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

void Alias::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);

    getDeclarator()->addSymbols(mod, _scope);

    setScope(_scope);
    _Symbol<Alias> * symbol = new _Symbol<Alias>(getName(), mod, "", this);
    setLookupName(symbol->unmangled);

    // do this before adding symbol so that it can't self reference
    getDeclarator()->analyze();
    Type::alias(symbol->unmangled, getDeclarator()->getType());

    _scope->addSymbol(symbol, &getNameContext());
}

const Type * Alias::getType() {
    analyze();

    const Type * t = getTypeFromTable(getLookupName());

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

void Alias::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    // @incomplete -- don't forget about this when we do template
    // aliases
    stream << std::string(4 * level, ' ');
    stream << "type " << getName() << " = ";
    getDeclarator()->dump(stream, level, dumpCT);
}

void Alias::desugar() { getDeclarator()->desugar(); }

Alias::~Alias() {
    BJOU_DEBUG_ASSERT(declarator);
    delete declarator;
}
//

// ~~~~~ Struct ~~~~~

Struct::Struct()
    : name({}), lookupName({}), extends(nullptr), memberVarDecls({}),
      inst(nullptr) {
    nodeKind = STRUCT;
}

std::string & Struct::getName() { return name; }
void Struct::setName(std::string _name) { name = _name; }

std::string & Struct::getLookupName() { return lookupName; }
void Struct::setLookupName(std::string _lookupName) {
    lookupName = _lookupName;
}

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
        if (!e_t->isStruct()) {
            errorl(extends->getContext(),
                   "Can't extend a type that is not a struct."); // @bad error
                                                                 // message
        }
    }

    setFlag(ANALYZED, true);
}

void Struct::preDeclare(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    _Symbol<Struct> * symbol = new _Symbol<Struct>(getName(), mod, "", this, inst, nullptr);
    setLookupName(symbol->unmangled);
    setMangledName(symbol->real_mangled);
    _scope->addSymbol(symbol, &getNameContext());

    // @refactor? should this really be here?
    StructType::get(symbol->unmangled, this, (TemplateInstantiation *)inst);
}

void Struct::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    /*
    setScope(_scope);
    _Symbol<Struct>* symbol = new _Symbol<Struct>(getName(), this, inst);
    setLookupName(symbol->real_mangled);
    _scope->addSymbol(symbol, &getNameContext());

    // @refactor? should this really be here?
    compilation->frontEnd.typeTable[getLookupName()] = new
    StructType(getLookupName(), this, (TemplateInstantiation*)inst);
    */

    if (extends)
        extends->addSymbols(mod, _scope);
    for (ASTNode * mem : getMemberVarDecls())
        mem->addSymbols(mod, _scope);
    for (ASTNode * constant : getConstantDecls())
        constant->addSymbols(mod, _scope);
    for (ASTNode * proc : getMemberProcs())
        proc->addSymbols(mod, _scope);
    for (ASTNode * tproc : getMemberTemplateProcs()) {
        ((Procedure*)((TemplateProc*)tproc)->_template)->desugarThis();
        tproc->addSymbols(mod, _scope);
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
}

ASTNode * Struct::clone() {
    Struct * c = new Struct(*this);

    std::vector<ASTNode *> & my_memberVarDecls = c->getMemberVarDecls();
    std::vector<ASTNode *> vars = my_memberVarDecls;
    std::vector<ASTNode *> & my_constantDecls = c->getConstantDecls();
    std::vector<ASTNode *> constants = my_constantDecls;
    std::vector<ASTNode *> & my_memberProcs = c->getMemberProcs();
    std::vector<ASTNode *> procs = my_memberProcs;

    if (c->getExtends())
        c->setExtends(c->getExtends()->clone());

    my_memberVarDecls.clear();
    my_constantDecls.clear();
    my_memberProcs.clear();

    for (ASTNode * v : vars)
        c->addMemberVarDecl(v->clone());
    for (ASTNode * co : constants)
        c->addConstantDecl(co->clone());
    for (ASTNode * p : procs)
        c->addMemberProc(p->clone());

    return c;
}

void Struct::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    if (getFlag(Struct::IS_TEMPLATE_DERIVED))
        return;

    stream << std::string(4 * level, ' ');
    if (getFlag(IS_ABSTRACT))
        stream << "abstract ";
    stream << "type " << getName();

    if (getParent() && getParent()->nodeKind == TEMPLATE_STRUCT) {
        TemplateStruct * tstruct = (TemplateStruct *)getParent();
        tstruct->getTemplateDef()->dump(stream, level, dumpCT);
    }

    stream << " ";

    if (getExtends()) {
        getExtends()->dump(stream, level, dumpCT);
        stream << " ";
    }
    stream << "{\n";

    level += 1;
    for (ASTNode * constant : getConstantDecls()) {
        constant->dump(stream, level, dumpCT);
        stream << "\n";
    }
    for (ASTNode * mem : getMemberVarDecls()) {
        mem->dump(stream, level, dumpCT);
        stream << "\n";
    }
    for (ASTNode * proc : getMemberProcs()) {
        proc->dump(stream, level, dumpCT);
        stream << "\n";
    }
    for (ASTNode * tproc : getMemberTemplateProcs()) {
        tproc->dump(stream, level, dumpCT);
        stream << "\n";
    }
    level -= 1;

    stream << std::string(4 * level, ' ');
    stream << "}";
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
}

Struct::~Struct() {
    for (ASTNode * mvd : memberVarDecls)
        delete mvd;
    for (ASTNode * constant : constantDecls)
        delete constant;
    // ownership transferred to top of AST
    // for (ASTNode * proc : memberProcs)
    // delete proc;
}
//

const Type * Struct::getType() {
    // analyze(); // should not be necessary
    std::string mangled = getLookupName();

    return StructType::get(mangled);
}

// ~~~~~ Enum ~~~~~

Enum::Enum() : name({}), lookupName({}), identifiers({}) { nodeKind = ENUM; }

std::string & Enum::getName() { return name; }
void Enum::setName(std::string _name) { name = _name; }

std::string & Enum::getLookupName() { return lookupName; }
void Enum::setLookupName(std::string _lookupName) {
    lookupName = _lookupName;
}

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

void Enum::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    _Symbol<Enum> * symbol = new _Symbol<Enum>(getName(), mod, "", this);

    setLookupName(symbol->unmangled);
    setMangledName(symbol->real_mangled);

    _scope->addSymbol(symbol, &getNameContext());

    EnumType::get(symbol->unmangled, this);
}

void Enum::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');
    stream << "enum " << getName() << " {\n";

    level += 1;
    std::string comma = ",\n";
    for (auto & id : getIdentifiers()) {
        stream << std::string(4 * level, ' ');
        if (&id == &getIdentifiers().back())
            comma = "\n";
        stream << id << comma;
    }
    level -= 1;

    stream << std::string(4 * level, ' ');
    stream << "}\n";
}

Enum::~Enum() {}
//

const Type * Enum::getType() {
    analyze();

    std::string mangled = getLookupName();
    return EnumType::get(mangled);
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

void ArgList::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    std::string comma = ", ";
    for (ASTNode *& expr : getExpressions()) {
        if (&expr == &getExpressions().back())
            comma = "";
        expr->dump(stream, level, dumpCT);
        stream << comma;
    }
}

void ArgList::desugar() {
    for (ASTNode * expr : getExpressions())
        expr->desugar();
}

void ArgList::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    for (ASTNode * expr : getExpressions())
        expr->addSymbols(mod, _scope);
}

ArgList::~ArgList() {
    for (ASTNode * e : expressions)
        delete e;
}
//

static void searchUnreach(ASTNode *& the_node, std::vector<ASTNode *> & the_statements) {
    auto search = std::find(the_statements.begin(),
                            the_statements.end(), the_node);

    auto end = the_statements.end();

    BJOU_DEBUG_ASSERT(search != end);

    search++;

    for (; search != end; search++) {
        ASTNode::NodeKind search_kind = (*search)->nodeKind;

        if (search_kind != ASTNode::MACRO_USE &&
            search_kind != ASTNode::IGNORE) {

            std::string err_str;

            if (the_node->nodeKind == ASTNode::RETURN)
                err_str = "return";
            else if (the_node->nodeKind == ASTNode::BREAK)
                err_str = "break";
            else if (the_node->nodeKind == ASTNode::CONTINUE)
                err_str = "continue";

            errorl(the_node->getContext(), "Code below this " +
                                               err_str +
                                               " will never execute.");
        }
    }
}

static void handleTerminators(ASTNode * statement,
                              std::vector<ASTNode *> & statements,
                              ASTNode *& node, bool from_multi = false,
                              MultiNode ** m = nullptr) {

    ASTNode::NodeKind n_k = node->nodeKind;

    if (n_k == ASTNode::IF) {
        if (((If *)node)->alwaysReturns()) {
            statement->setFlag(ASTNode::HAS_TOP_LEVEL_RETURN, true);
        }
    } else if (n_k == ASTNode::RETURN ||
               n_k == ASTNode::BREAK  ||
               n_k == ASTNode::CONTINUE) {

        statement->setFlag(ASTNode::HAS_TOP_LEVEL_RETURN, true);

        if (!from_multi) {
            searchUnreach(node, statements);
        } else {
            BJOU_DEBUG_ASSERT(m && "no MulitNode");
            if (&node == &((*m)->nodes.back())) {
                // node is last in multinode
                // search for nodes after the multinode
                searchUnreach(*(ASTNode **)m, statements);
            } else {
                // there are nodes after this one in the multinode
                // check those for error
                searchUnreach(node, (*m)->nodes);
            }
        }
    } else if (n_k == ASTNode::MULTINODE) {
        MultiNode * multi = (MultiNode *)node;
        for (ASTNode * n : multi->nodes) {
            handleTerminators(statement, statements, n, true,
                              (MultiNode **)&node);
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

void This::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    // and...
}

void This::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "this";
}

This::~This() {}
//

// ~~~~~ Procedure ~~~~~

Procedure::Procedure()
    : name({}), lookupName({}), paramVarDeclarations({}),
      retDeclarator(nullptr), procDeclarator(nullptr), statements({}),
      inst(nullptr) {
    nodeKind = PROCEDURE;
}

std::string & Procedure::getName() { return name; }
void Procedure::setName(std::string _name) { name = _name; }

std::string & Procedure::getLookupName() { return lookupName; }
void Procedure::setLookupName(std::string _lookupName) {
    lookupName = _lookupName;
}

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

    if (parent) {
        if (parent->nodeKind == ASTNode::STRUCT) {
            s = (Struct *)parent;
        } else if (parent->nodeKind == ASTNode::TEMPLATE_PROC &&
                   parent->getParent()->nodeKind == ASTNode::STRUCT) {
            s = (Struct *)parent->getParent();
        }
    }

    return s;
}

void Procedure::desugarThis() {
    This * seen = nullptr;
    for (ASTNode * node : getParamVarDeclarations()) {
        if (node->nodeKind == ASTNode::THIS) {
            if (!getParentStruct() || getParentStruct()->nodeKind != ASTNode::STRUCT)
                errorl(node->getContext(),
                       "'this' not allowed here."); // @bad error message

            Struct * s = getParentStruct();

            BJOU_DEBUG_ASSERT(s);

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
            param->getTypeDeclarator()->addSymbols(mod, 
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

    bool returnInIf = false;
    for (ASTNode *& statement : getStatements()) {
        statement->analyze(force);
        handleTerminators(this, getStatements(), statement);
        if (statement->nodeKind == ASTNode::IF)
            if (((If *)statement)->alwaysReturns())
                returnInIf = true;
    }

    setFlag(HAS_TOP_LEVEL_RETURN, getFlag(HAS_TOP_LEVEL_RETURN) || returnInIf);

    if (!getFlag(HAS_TOP_LEVEL_RETURN) && !getFlag(IS_EXTERN) &&
        !conv(getRetDeclarator()->getType(), VoidType::get())) {

        errorl(getContext().lastchar(),
               "'" + getName() + "' must explicitly return a value of type '" +
                   retDeclarator->getType()->getDemangledName() + "'");
    }

    compilation->frontEnd.procStack.pop();

    if (getProcDeclarator()) {
        delete getProcDeclarator();
        procDeclarator = nullptr;
    }
    getType();

    setFlag(ANALYZED, true);
}

void Procedure::addSymbols(std::string& _mod, Scope * _scope) {
    if (!getFlag(Procedure::IS_EXTERN)) {
        mod = _mod;
    }
    setScope(_scope);

    _Symbol<Procedure> * symbol = nullptr;

    std::string t = "";
    ASTNode * p_root = this;
    if (p_root->getParent() && p_root->getParent()->nodeKind == ASTNode::TEMPLATE_PROC) {
        p_root = p_root->getParent();
    }
    if (p_root->getParent()) {
        ASTNode * p = p_root->getParent();
        if (p->nodeKind == ASTNode::STRUCT) {
            Struct * s = (Struct*)p;
            if (!p->getParent() || p->getParent()->nodeKind != ASTNode::TEMPLATE_STRUCT) {
                t = string_sans_mod(s->getLookupName());
            } else if (p->getParent() && p->getParent()->nodeKind == ASTNode::TEMPLATE_STRUCT) {
                t = s->getName();
            }
        } else if (p->nodeKind == ASTNode::TEMPLATE_STRUCT) {
            t         = ((Struct*)((TemplateStruct*)p_root->getParent())->_template)->getName();
        }
    }

    symbol = new _Symbol<Procedure>(getName(), mod, t, (Procedure *)this, inst, nullptr);

    Scope * myScope = new Scope("", _scope);
    myScope->description =
        "scope opened by " + symbol->unmangled;

    // open scope normally unless it is a local proc
    // if so, put it in the global scope
    if (!_scope->parent) {
        _scope->scopes.push_back(myScope);
    } else {
        Scope * walk = _scope;
        while (walk->parent && !walk->parent->is_module_scope)
            walk = walk->parent;
        walk->scopes.push_back(myScope);
    }

    if (!getFlag(Procedure::IS_EXTERN)) {
        desugarThis();

        for (ASTNode * param : getParamVarDeclarations())
            param->addSymbols(mod, myScope);
        getRetDeclarator()->addSymbols(mod, myScope);
        for (ASTNode * statement : getStatements())
            statement->addSymbols(mod, myScope);
        // necessary if proc was imported
        if (procDeclarator)
            procDeclarator->addSymbols(mod, myScope);


        setLookupName(symbol->proc_name);
        if (getFlag(Procedure::NO_MANGLE)) {
            setMangledName(symbol->proc_name);
        } else {
            setMangledName(symbol->real_mangled);
        }

        if (getName() == "__bjou_rt_init")
            compilation->frontEnd.__bjou_rt_init_def = this;
    } else {
        setLookupName(symbol->proc_name);
        setMangledName(symbol->proc_name);

        for (ASTNode * param : getParamVarDeclarations())
            param->addSymbols(mod, myScope);
        getRetDeclarator()->addSymbols(mod, myScope);
    }

    if (getFlag(Procedure::IS_EXTERN) || getFlag(Procedure::NO_MANGLE)) {
        if (getLookupName() == "printf")
            compilation->frontEnd.printf_decl = this;
        else if (getLookupName() == "malloc")
            compilation->frontEnd.malloc_decl = this;
        else if (getLookupName() == "free")
            compilation->frontEnd.free_decl = this;
        else if (getLookupName() == "memset")
            compilation->frontEnd.memset_decl = this;
        else if (getLookupName() == "memcpy")
            compilation->frontEnd.memcpy_decl = this;
    }

    _scope->addSymbol(symbol, &getNameContext());
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

void Procedure::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    if (getFlag(Procedure::IS_TEMPLATE_DERIVED))
        return;

    stream << std::string(4 * level, ' ');

    if (getFlag(Procedure::IS_EXTERN))
        stream << "extern ";
    else
        stream << "proc ";

    if (!getFlag(Procedure::IS_ANONYMOUS))
        stream << getName();

    if (getParent() && getParent()->nodeKind == TEMPLATE_PROC) {
        TemplateProc * tproc = (TemplateProc *)getParent();
        tproc->getTemplateDef()->dump(stream, level, dumpCT);
    }

    stream << "(";
    std::string comma = ", ";
    for (ASTNode *& param : getParamVarDeclarations()) {
        if (&param == &getParamVarDeclarations().back())
            comma = "";
        if (!getFlag(Procedure::IS_EXTERN)) {
            param->dump(stream, level, dumpCT);
        } else {
            VariableDeclaration * var = (VariableDeclaration *)param;
            BJOU_DEBUG_ASSERT(var->getTypeDeclarator());
            var->getTypeDeclarator()->dump(stream, level, dumpCT);
        }
        stream << comma;
    }
    stream << ") : ";

    getRetDeclarator()->dump(stream, level, dumpCT);

    if (!getFlag(Procedure::IS_EXTERN)) {
        stream << " {\n";

        level += 1;
        for (ASTNode * s : getStatements()) {
            s->dump(stream, level, dumpCT);
            stream << "\n";
        }
        level -= 1;

        stream << std::string(4 * level, ' ');
        stream << "}";
    }
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
        d->addSymbols(mod, p->getScope());
        procDeclarator->addParamDeclarator(d);
    }

    procDeclarator->setRetDeclarator(
        getRetDeclarator()->getType()->getGenericDeclarator());
    procDeclarator->addSymbols(mod, getRetDeclarator()->getScope());
    procDeclarator->setFlag(ProcedureDeclarator::eBitFlags::IS_VARARG,
                            getFlag(IS_VARARG));
    setProcDeclarator(procDeclarator);

    return procDeclarator->getType();
}


// ~~~~~ Import ~~~~~

Import::Import()
    : fileError(false), notModuleError(false), module({}), theModule(nullptr) {
    nodeKind = IMPORT;
}

std::string & Import::getModule() { return module; }
void Import::setModule(std::string _module) { module = _module; }

void Import::activate(bool ct) {
    BJOU_DEBUG_ASSERT(theModule);

    theModule->activate(this, ct); // might replace 'this'
}

// Node interface
void Import::unwrap(std::vector<ASTNode *> & terminals) {
    terminals.push_back(this);
}

void Import::analyze(bool force) {
    HANDLE_FORCE();

    setFlag(ANALYZED, true);
}

ASTNode * Import::clone() { return new Import(*this); }

void Import::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);

    if (fileError) {
        errorl(getContext(), "Unable to read file '" + de_quote(module) + "'.");
    }

    if (notModuleError) {
        error(getContext(),
              "File '" + de_quote(module) + "' does not declare a module.",
              false, "'" + de_quote(module) + "' resolved to path '" + full_path + "'");
        errorl(getContext(), "Attempted import from this location.");
    }

    if (getScope()->parent && !getScope()->is_module_scope) {
        errorl(getContext(), "Module imports must be made at global scope.",
               false);
        errorl(getContext(), "Attempting to import " + getModule() +
                                 " in scope with description '" +
                                 getScope()->description + "'.");
    }

    bool ct = false;

    ASTNode * p = getParent();
    while (p) {
        if (p->nodeKind == MACRO_USE) {
            MacroUse * use = (MacroUse *)p;
            if (use->getMacroName() == "ct") {
                ct = true;
                break;
            }
        }
        p = p->getParent();
    }


    activate(ct);
}

void Import::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "import " << getModule();

    stream << "\n";
}

Import::~Import() {}
//

// ~~~~~ Using ~~~~~

Using::Using()
    : import(nullptr), module({}) {
    nodeKind = USING;
}

Import * Using::getImport() { return import; }
void Using::setImport(Import * _import) { import = _import; }
std::string & Using::getModule() { return module; }
void Using::setModule(std::string _module) { module = _module; }

// Node interface
void Using::unwrap(std::vector<ASTNode *> & terminals) {
    terminals.push_back(this);
}

void Using::analyze(bool force) {
    HANDLE_FORCE();
    setFlag(ANALYZED, true);
}

ASTNode * Using::clone() { return new Using(*this); }

void Using::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);

    if (getImport()) {
        BJOU_DEBUG_ASSERT(getModule().empty());
        setModule(getImport()->theModule->identifier);
    }

    if (compilation->frontEnd.modulesByID.count(getModule()) == 0) {
        errorl(getContext(), "No module by the name '" + getModule() + "' was found.");
    }

    getScope()->usings.push_back(getModule());
}

void Using::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "using " << getModule();

    stream << "\n";
}

Using::~Using() {}
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

void Print::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    BJOU_DEBUG_ASSERT(getArgs());
    getArgs()->addSymbols(mod, _scope);
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

void Print::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "print ";
    getArgs()->dump(stream, level, dumpCT);

    stream << "\n";
}

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

    ASTNode * p = getParent();
    while (p) {
        if (p->nodeKind == PROCEDURE)
            break;
        p = p->getParent();
    }

    Procedure * proc = (Procedure *)p;

    if (proc) {
        const Type * retLVal = proc->getRetDeclarator()->getType();
        compilation->frontEnd.lValStack.push(retLVal);

        if (getExpression()) {
            getExpression()->analyze();
            // @leaks abound
            if (retLVal == VoidType::get()) {
                errorl(getExpression()->getContext(),
                       "'" + proc->getName() + "' does not return a value.");
            } else {
                const Type * expr_t = getExpression()->getType();
                if (!conv(retLVal, expr_t))
                    errorl(getExpression()->getContext(),
                           "return statement does not match the return type "
                           "for procedure '" +
                               proc->getName() + "'.",
                           true,
                           "expected '" + retLVal->getDemangledName() + "'",
                           "got '" + expr_t->getDemangledName() + "'");
                if (!equal(retLVal, expr_t))
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

void Return::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    if (expression)
        expression->addSymbols(mod, _scope);
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

void Return::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "return";
    if (getExpression()) {
        stream << " ";
        getExpression()->dump(stream, level, dumpCT);
    }

    stream << "\n";
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
        p = p->getParent();
    }

    if (!p || !p->isStatement())
        errorl(getContext(), "no loop to break out of");

    setFlag(ANALYZED, true);
}

ASTNode * Break::clone() { return new Break(*this); }

void Break::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    // and...
}

void Break::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');
    stream << "break";

    stream << "\n";
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
        p = p->getParent();
    }

    if (!p || !p->isStatement())
        errorl(getContext(), "no loop to continue");

    setFlag(ANALYZED, true);
}

ASTNode * Continue::clone() { return new Continue(*this); }

void Continue::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    // and...
}

void Continue::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');
    stream << "continue";

    stream << "\n";
}

bool Continue::isStatement() const { return true; }

Continue::~Continue() {}
//

// ~~~~~ If ~~~~~

If::If() : conditional(nullptr), statements({}), _else(nullptr), shouldEmitMerge(true) {
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

bool If::alwaysReturns() const {
    if (getElse()) {
        Else * _else = (Else *)getElse();
        bool me = getFlag(ASTNode::HAS_TOP_LEVEL_RETURN);

        if (_else->getFlag(ASTNode::HAS_TOP_LEVEL_RETURN) && me) {
            return true;
        } else if (_else->getStatements()[0]->nodeKind == ASTNode::IF) {
            if (((If *)_else->getStatements()[0])->alwaysReturns() && me)
                return true;
        }
    }
    return false;
}

// Node interface
void If::analyze(bool force) {
    HANDLE_FORCE();

    getConditional()->analyze(force);

    const Type * bool_type = BoolType::get();

    if (!conv(bool_type, getConditional()->getType())) {
        errorl(
            getConditional()->getContext(),
            "Expression convertible to type 'bool' is required for conditionals.",
            true,
            "'" + getConditional()->getType()->getDemangledName() +
                "' is invalid");
    }

    if (!equal(bool_type, getConditional()->getType())) {
        emplaceConversion((Expression*)getConditional(), bool_type);
    }

    for (ASTNode *& statement : getStatements()) {
        statement->analyze(force);
        handleTerminators(this, getStatements(), statement);
    }

    if (getElse()) {
        getElse()->analyze(force);
    }

    setFlag(ANALYZED, true);
}

void If::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    Scope * myScope = new Scope(
        "line " + std::to_string(getContext().begin.line) + " if", _scope);
    _scope->scopes.push_back(myScope);
    conditional->addSymbols(mod, _scope);
    for (ASTNode * statement : getStatements())
        statement->addSymbols(mod, myScope);
    if (_else)
        _else->addSymbols(mod, _scope);
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

void If::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "if ";
    getConditional()->dump(stream, level, dumpCT);
    stream << " {\n";

    level += 1;
    for (ASTNode * s : getStatements()) {
        s->dump(stream, level, dumpCT);
        stream << "\n";
    }
    level -= 1;

    stream << std::string(4 * level, ' ');
    stream << "}";

    if (getElse()) {
        stream << " ";
        getElse()->dump(stream, level, dumpCT);
    } else {
        stream << "\n";
    }
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

void Else::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    Scope * myScope = new Scope(
        "line " + std::to_string(getContext().begin.line) + " else", _scope);
    _scope->scopes.push_back(myScope);
    for (ASTNode *& statement : getStatements())
        statement->addSymbols(mod, myScope);
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

void Else::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "else {\n";

    level += 1;
    for (ASTNode * s : getStatements()) {
        s->dump(stream, level, dumpCT);
        stream << "\n";
    }
    level -= 1;

    stream << std::string(4 * level, ' ');
    stream << "}";

    stream << "\n";
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

    if (!conv(BoolType::get(), getConditional()->getType()))
        errorl(
            getConditional()->getContext(),
            "Expression resulting in type 'bool' is required for conditionals.",
            true,
            "'" + getConditional()->getType()->getDemangledName() +
                "' is invalid");

    if (!equal(BoolType::get(), getConditional()->getType()))
        emplaceConversion((Expression*)getConditional(), BoolType::get());

    for (ASTNode * at : getAfterthoughts())
        at->analyze(force);

    for (ASTNode *& statement : getStatements()) {
        statement->analyze(force);
        handleTerminators(this, getStatements(), statement);
    }

    setFlag(ANALYZED, true);
}

void For::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    Scope * myScope = new Scope(
        "line " + std::to_string(getContext().begin.line) + " for", _scope);
    _scope->scopes.push_back(myScope);
    for (ASTNode * initialization : getInitializations())
        initialization->addSymbols(mod, myScope);
    conditional->addSymbols(mod, myScope);
    for (ASTNode * afterthought : getAfterthoughts())
        afterthought->addSymbols(mod, myScope);
    for (ASTNode * statement : getStatements())
        statement->addSymbols(mod, myScope);
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

void For::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "for ";

    std::string comma = ", ";
    for (ASTNode *& init : getInitializations()) {
        if (&init == &getInitializations().back())
            comma = "; ";
        init->dump(stream, level, dumpCT);
        stream << comma;
    }

    getConditional()->dump(stream, level, dumpCT);
    stream << "; ";

    comma = ", ";
    for (ASTNode *& at : getAfterthoughts()) {
        if (&at == &getAfterthoughts().back())
            comma = "; ";
        at->dump(stream, level, dumpCT);
        stream << comma;
    }

    stream << " {\n";

    level += 1;
    for (ASTNode * s : getStatements()) {
        s->dump(stream, level, dumpCT);
        stream << "\n";
    }
    level -= 1;

    stream << std::string(4 * level, ' ');
    stream << "}";

    stream << "\n";
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

    if (t->under()->isArray() && !getFlag(TAKE_REF))
        errorl(getExpression()->getContext(),
               "The element type being iterated over is an array and must be "
               "taken by reference.",
               true, "Expression has type '" + t->getDemangledName() + "'",
               "Element type '" + t->under()->getDemangledName() +
                   "' requires that the loop be by 'ref'");

    desugar();

    setFlag(ANALYZED, true);
}

void Foreach::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    Scope * myScope = new Scope(
        "line " + std::to_string(getContext().begin.line) + " foreach", _scope);
    _scope->scopes.push_back(myScope);
    getExpression()->addSymbols(mod, myScope);
    for (ASTNode * statement : getStatements())
        statement->addSymbols(mod, myScope);
}

void Foreach::unwrap(std::vector<ASTNode *> & terminals) {
    getExpression()->unwrap(terminals);
    for (ASTNode * statement : getStatements())
        statement->unwrap(terminals);
}

ASTNode * Foreach::clone() {
    Foreach * c = new Foreach(*this);

    c->setExpression(getExpression()->clone());

    for (ASTNode * statement : getStatements())
        c->addStatement(statement);

    return c;
}

void Foreach::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "foreach ";

    if (getFlag(Foreach::TAKE_REF))
        stream << "ref ";

    stream << getIdent() << " in ";
    getExpression()->dump(stream, level, dumpCT);

    stream << " {\n";

    level += 1;
    for (ASTNode * s : getStatements()) {
        s->dump(stream, level, dumpCT);
        stream << "\n";
    }
    level -= 1;

    stream << std::string(4 * level, ' ');
    stream << "}";

    stream << "\n";
}

void Foreach::desugar() {
    const Type * t = getExpression()->getType()->unRef();
    const Type * elem_t = t->under();

    For * _for = new For;
    _for->setContext(getContext());

    VariableDeclaration * it = new VariableDeclaration;
    it->setName(compilation->frontEnd.makeUID("foreach_it"));

    IntegerLiteral * zero = new IntegerLiteral;
    zero->setContents("0");

    it->setInitialization(zero);

    _for->addInitialization(it);

    LssExpression * lss = new LssExpression;

    Identifier * i_it = new Identifier;
    i_it->setSymName(it->getName());

    LenExpression * len = new LenExpression;
    len->setExpr(getExpression());

    lss->setLeft(i_it);
    lss->setRight(len);

    _for->setConditional(lss);

    AddAssignExpression * inc = new AddAssignExpression;

    Identifier * _i_it = new Identifier;
    _i_it->setSymName(it->getName());

    IntegerLiteral * one = new IntegerLiteral;
    one->setContents("1");

    inc->setLeft(_i_it);
    inc->setRight(one);

    _for->addAfterthought(inc);

    VariableDeclaration * elem = new VariableDeclaration;
    elem->setContext(getIdentContext());
    elem->setNameContext(getIdentContext());
    elem->setName(getIdent());

    if (getFlag(TAKE_REF)) {
        elem->setTypeDeclarator(elem_t->getRef()->getGenericDeclarator());
    } else {
        elem->setTypeDeclarator(elem_t->getGenericDeclarator());
    }

    SubscriptExpression * sub = new SubscriptExpression;

    Identifier * __i_it = new Identifier;
    __i_it->setSymName(it->getName());

    sub->setLeft(getExpression()->clone());
    sub->setRight(__i_it);

    elem->setInitialization(sub);

    _for->addStatement(elem);
    for (ASTNode * s : getStatements())
        _for->addStatement(s);

    _for->addSymbols(mod, getScope());

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

    if (!conv(BoolType::get(), getConditional()->getType()))
        errorl(
            getConditional()->getContext(),
            "Expression resulting in type 'bool' is required for conditionals.",
            true,
            "'" + getConditional()->getType()->getDemangledName() +
                "' is invalid");

    if (!equal(BoolType::get(), getConditional()->getType()))
        emplaceConversion((Expression*)getConditional(), BoolType::get());

    for (ASTNode *& statement : getStatements()) {
        statement->analyze(force);
        handleTerminators(this, getStatements(), statement);
    }

    setFlag(ANALYZED, true);
}

void While::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    Scope * myScope = new Scope(
        "line " + std::to_string(getContext().begin.line) + " while", _scope);
    _scope->scopes.push_back(myScope);
    conditional->addSymbols(mod, _scope);
    for (ASTNode * statement : getStatements())
        statement->addSymbols(mod, myScope);
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

void While::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "while ";

    getConditional()->dump(stream, level, dumpCT);

    stream << " {\n";

    level += 1;
    for (ASTNode * s : getStatements()) {
        s->dump(stream, level, dumpCT);
        stream << "\n";
    }
    level -= 1;

    stream << std::string(4 * level, ' ');
    stream << "}";

    stream << "\n";
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

    if (!conv(BoolType::get(), getConditional()->getType()))
        errorl(
            getConditional()->getContext(),
            "Expression resulting in type 'bool' is required for conditionals.",
            true,
            "'" + getConditional()->getType()->getDemangledName() +
                "' is invalid");

    if (!equal(BoolType::get(), getConditional()->getType()))
        emplaceConversion((Expression*)getConditional(), BoolType::get());

    for (ASTNode *& statement : getStatements()) {
        statement->analyze(force);
        handleTerminators(this, getStatements(), statement);
    }

    setFlag(ANALYZED, true);
}

void DoWhile::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    Scope * myScope = new Scope(
        "line " + std::to_string(getContext().begin.line) + " if", _scope);
    _scope->scopes.push_back(myScope);
    conditional->addSymbols(mod, _scope);
    for (ASTNode * statement : getStatements())
        statement->addSymbols(mod, myScope);
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

void DoWhile::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "do ";

    stream << " {\n";

    level += 1;
    for (ASTNode * s : getStatements()) {
        s->dump(stream, level, dumpCT);
        stream << "\n";
    }
    level -= 1;

    stream << std::string(4 * level, ' ');
    stream << "} while ";

    getConditional()->dump(stream, level, dumpCT);

    stream << "\n";
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

void Match::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    getExpression()->addSymbols(mod, _scope);
    for (ASTNode * with : getWiths())
        with->addSymbols(mod, _scope);
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

void Match::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "match ";

    getExpression()->dump(stream, level, dumpCT);

    stream << " {\n";

    level += 1;
    for (ASTNode * w : getWiths()) {
        w->dump(stream, level, dumpCT);
        stream << "\n";
    }
    level -= 1;

    stream << std::string(4 * level, ' ');
    stream << "}";

    stream << "\n";
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

void With::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    Scope * myScope = new Scope(
        "line " + std::to_string(getContext().begin.line) + " with", _scope);
    _scope->scopes.push_back(myScope);
    getExpression()->addSymbols(mod, _scope);
    for (ASTNode * statement : getStatements())
        statement->addSymbols(mod, myScope);
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

void With::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "with ";

    if (getFlag(WITH_ELSE)) {
        stream << "else";
    } else {
        getExpression()->dump(stream, level, dumpCT);
    }

    stream << " : {\n";

    level += 1;
    for (ASTNode * s : getStatements()) {
        s->dump(stream, level, dumpCT);
        stream << "\n";
    }
    level -= 1;

    stream << std::string(4 * level, ' ');
    stream << "}";

    stream << "\n";
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

void TemplateDefineList::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
}

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

void TemplateDefineList::dump(std::ostream & stream, unsigned int level,
                              bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "$";
    if (getElements().size() > 1) {
        stream << "(";
        std::string comma = ", ";
        for (ASTNode *& elem : getElements()) {
            if (&elem == &getElements().back())
                comma = "";
            stream << ((TemplateDefineElement *)elem)->getName() << comma;
        }
        stream << ")";
    } else {
        stream << ((TemplateDefineElement *)getElements()[0])->getName();
    }
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

void TemplateDefineTypeDescriptor::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
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

void TemplateDefineVariadicTypeArgs::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
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

void TemplateDefineExpression::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
}

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

void TemplateInstantiation::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    for (ASTNode * element : getElements())
        element->addSymbols(mod, _scope);
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

void TemplateInstantiation::dump(std::ostream & stream, unsigned int level,
                                 bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << "$";
    if (getElements().size() > 1) {
        stream << "(";
        std::string comma = "";
        for (ASTNode * elem : getElements()) {
            stream << comma;
            elem->dump(stream, level, dumpCT);
            comma = ", ";
        }
        stream << ")";
    } else {
        getElements()[0]->dump(stream, level, dumpCT);
    }
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

void TemplateAlias::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    Alias * typeTemplate = (Alias *)getTemplate();
    _Symbol<TemplateAlias> * symbol =
        new _Symbol<TemplateAlias>(typeTemplate->getName(), mod, "", this, nullptr, getTemplateDef());
    _scope->addSymbol(symbol, &typeTemplate->getNameContext());

    BJOU_DEBUG_ASSERT(false);

    /*
    compilation->frontEnd.typeTable[lookupName] =
        new TemplateAliasType(lookupName);
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

void TemplateStruct::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    Struct * typeTemplate = (Struct *)getTemplate();
    _Symbol<TemplateStruct> * symbol =
        new _Symbol<TemplateStruct>(typeTemplate->getName(), mod, "", this);
    _scope->addSymbol(symbol, &typeTemplate->getNameContext());

    BJOU_DEBUG_ASSERT(_template->nodeKind == ASTNode::STRUCT);
    Struct * s = (Struct*)_template;

    for (ASTNode * _proc : s->getMemberProcs()) {
        Procedure * proc = (Procedure *)_proc->clone();
        proc->setScope(getScope());

        TemplateProc * tproc = new TemplateProc;
        tproc->parent = s;
        tproc->mod = proc->mod = mod;
        tproc->setFlag(TemplateProc::FROM_THROUGH_TEMPLATE,
                true);
        tproc->setFlag(TemplateProc::IS_TYPE_MEMBER, true);
        tproc->parent = s;
        tproc->setScope(proc->getScope());
        tproc->setContext(proc->getContext());
        tproc->setNameContext(proc->getNameContext());
        tproc->setTemplateDef(getTemplateDef()->clone());
        tproc->setTemplate(proc);

        /* proc->desugarThis(); */

        through_procs.push_back(tproc);
    }

    for (ASTNode * _tproc : s->getMemberTemplateProcs()) {
        TemplateProc * tproc = (TemplateProc *)_tproc;

        TemplateProc * new_tproc =
            (TemplateProc *)tproc->clone();
        Procedure * proc = (Procedure *)new_tproc->_template;
        proc->setScope(getScope());
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
                ((TemplateDefineList *)getTemplateDef())
                ->getElements())
            new_tproc_def->addElement(elem);
        for (ASTNode * elem : save_elems)
            new_tproc_def->addElement(elem);

        new_tproc->setTemplate(proc);

        /* proc->desugarThis(); */
    }

    for (ASTNode * through_proc : through_procs) {
        TemplateProc * tproc = (TemplateProc*)through_proc;
        Procedure * p = (Procedure*)tproc->_template;
        _Symbol<TemplateProc> * symbol =
            new _Symbol<TemplateProc>(p->getName(), mod, s->getName(), tproc, nullptr, tproc->getTemplateDef());
        getScope()->addSymbol(symbol, &p->getNameContext());
        tproc->setLookupName(symbol->real_mangled);
    }
}

void TemplateStruct::dump(std::ostream & stream, unsigned int level,
                          bool dumpCT) {
    getTemplate()->dump(stream, level, dumpCT);
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

std::string & TemplateProc::getLookupName() { return lookupName; }
void TemplateProc::setLookupName(std::string _lookupName) {
    lookupName = _lookupName;
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

void TemplateProc::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    Procedure * procedureTemplate = (Procedure *)getTemplate();

    Struct * parent_struct = nullptr;
    if (getParent() && getParent()->nodeKind == ASTNode::STRUCT) {
        parent_struct = (Struct*)getParent();
    }

    std::string t = "";
    if (getParent()) {
        ASTNode * p = getParent();
        if (p->nodeKind == ASTNode::STRUCT) {
            Struct * s = (Struct*)p;
            if (!p->getParent() || p->getParent()->nodeKind != ASTNode::TEMPLATE_STRUCT) {
                t = string_sans_mod(s->getLookupName());
            } else if (p->getParent() && p->getParent()->nodeKind == ASTNode::TEMPLATE_STRUCT) {
                t = s->getName();
            }
        } else if (p->nodeKind == ASTNode::TEMPLATE_STRUCT) {
            t         = ((Struct*)((TemplateStruct*)getParent())->_template)->getName();
        }
    }

    _Symbol<TemplateProc> * symbol = new _Symbol<TemplateProc>(procedureTemplate->getName(), mod, t, this, nullptr, getTemplateDef());

    setLookupName(symbol->real_mangled);
    _scope->addSymbol(symbol, &procedureTemplate->getNameContext());
}

void TemplateProc::dump(std::ostream & stream, unsigned int level,
                        bool dumpCT) {
    getTemplate()->dump(stream, level, dumpCT);
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

void SLComment::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
}

void SLComment::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');
    stream << "# " << getContents();
}

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

void ModuleDeclaration::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
}

void ModuleDeclaration::dump(std::ostream & stream, unsigned int level,
                             bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "module " << getIdentifier();
}

ModuleDeclaration::~ModuleDeclaration() {}
//

// ~~~~~ ExprBlockYield ~~~~~

ExprBlockYield::ExprBlockYield() : expression(nullptr) {
    nodeKind = EXPR_BLOCK_YIELD;
}

ASTNode * ExprBlockYield::getExpression() const { return expression; }
void ExprBlockYield::setExpression(ASTNode * _expression) {
    _expression->parent = this;
    _expression->replace = rpget<replacementPolicy_ExprBlockYield_Expression>();
    expression = _expression;
}

// Node interface
void ExprBlockYield::analyze(bool force) {
    HANDLE_FORCE();

    ASTNode * p = getParent();
    while (p) {
        if (p->nodeKind == EXPR_BLOCK)
            break;
        p = p->getParent();
    }

    ExprBlock * block = (ExprBlock *)p;

    if (block) {
        BJOU_DEBUG_ASSERT(getExpression());
        getExpression()->analyze();
        const Type * expr_t = getExpression()->getType();
        block->yieldTypes.push_back(std::make_pair(this, expr_t));
    } else
        errorl(getContext(),
               "Unexpected yield statement outside of expression block.");

    setFlag(ANALYZED, true);
}

void ExprBlockYield::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
    if (expression)
        expression->addSymbols(mod, _scope);
}

void ExprBlockYield::unwrap(std::vector<ASTNode *> & terminals) {
    if (getExpression())
        getExpression()->unwrap(terminals);
}

ASTNode * ExprBlockYield::clone() {
    ExprBlockYield * c = new ExprBlockYield(*this);

    if (c->getExpression())
        c->setExpression(c->getExpression()->clone());

    return c;
}

void ExprBlockYield::dump(std::ostream & stream, unsigned int level,
                          bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "<-";
    getExpression()->dump(stream, level, dumpCT);

    stream << "\n";
}

void ExprBlockYield::desugar() {
    if (getExpression())
        getExpression()->desugar();
}

bool ExprBlockYield::isStatement() const { return true; }

ExprBlockYield::~ExprBlockYield() {
    if (expression)
        delete expression;
}
//

// ~~~~~ IgnoreNode ~~~~~

IgnoreNode::IgnoreNode() { nodeKind = IGNORE; }

// Node interface
void IgnoreNode::analyze(bool force) {
    HANDLE_FORCE();

    setFlag(ANALYZED, true);
}

ASTNode * IgnoreNode::clone() { return new IgnoreNode(*this); }

void IgnoreNode::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);
}

void * IgnoreNode::generate(BackEnd & backEnd, bool flag) { return nullptr; }

void IgnoreNode::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    return;
}

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

void MacroUse::addSymbols(std::string& _mod, Scope * _scope) {
    mod = _mod;
    setScope(_scope);

    bool fast_track = false;

    if (getArgs().size() == 0) {
        fast_track = true;
    } else {
        int i = 0;
        for (ASTNode * arg : getArgs()) {
            if (arg->nodeKind != STRUCT) {
                if (compilation->frontEnd.macroManager.shouldAddSymbols(this, i))
                    arg->addSymbols(mod, _scope);
                else
                    fast_track = true;
            }
            i += 1;
        }
    }

    if (!compilation->frontEnd.stop_tracking_macros) {
        if (fast_track && getMacroName() != "static_do")
            compilation->frontEnd.macros_need_fast_tracked_analysis.insert(
                this);
        else if (getMacroName() != "run" && getMacroName() != "static_do")
            compilation->frontEnd.non_run_non_fast_tracked_macros.insert(this);
    }
}

void MacroUse::dump(std::ostream & stream, unsigned int level, bool dumpCT) {
    if (!dumpCT && isCT(this))
        return;
    stream << std::string(4 * level, ' ');

    stream << "\\" << getMacroName() << "{\n";

    level += 1;
    for (ASTNode * arg : getArgs()) {
        stream << std::string(4 * level, ' ');
        arg->dump(stream, level, dumpCT);
        stream << "\n";
    }
    level -= 1;

    stream << std::string(4 * level, ' ');
    stream << "}\n";
}

MacroUse::~MacroUse() {}
//

} // namespace bjou
