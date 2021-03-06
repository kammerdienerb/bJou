//
//  ASTNode.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/4/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef ASTNode_hpp
#define ASTNode_hpp

#include "CLI.hpp"
#include "Context.hpp"
#include "Evaluate.hpp"
#include "Maybe.hpp"
#include "Misc.hpp"
#include "Range.hpp"
#include "ReplacementPolicy.hpp"
#include "Type.hpp"

#include <map>
#include <ostream>
#include <unordered_map>
#include <vector>

namespace bjou {
struct Scope;
struct SymbolNamespace;
struct ASTNode;

struct SelfDestruct {
    SelfDestruct();
    SelfDestruct(ASTNode * _node);

    void set(ASTNode * _node);
    void defuse();

    ~SelfDestruct();

    ASTNode * node;
};

// @refactor: with the addition of NodeKind, some of E_BIT_FLAGS are redundant
// and should be removed/replaced

#define E_BIT_FLAGS()                                                          \
    {                                                                          \
        ANALYZED, IS_TEMPLATE, SYMBOL_OVERWRITE, HAS_TOP_LEVEL_RETURN,         \
            IGNORE_GEN, CT                                                     \
    }
#define E_BIT_FLAGS_AND(...)                                                   \
    {                                                                          \
        ANALYZED, IS_TEMPLATE, SYMBOL_OVERWRITE, HAS_TOP_LEVEL_RETURN,         \
            IGNORE_GEN, CT, __VA_ARGS__                                        \
    }

/* ============================================================================
 *
 *                                  ASTNode
 *  This is base type from which all different node types will derive.
 *
 * ===========================================================================*/

struct ASTNode {
    ASTNode();

    enum NodeKind {
        NONE,
        PROC_SET,
        EXPRESSION,
        BINARY_EXPRESSION,
        ADD_EXPRESSION,
        SUB_EXPRESSION,
        BSHL_EXPRESSION,
        BSHR_EXPRESSION,
        MULT_EXPRESSION,
        DIV_EXPRESSION,
        MOD_EXPRESSION,
        ASSIGNMENT_EXPRESSION,
        ADD_ASSIGN_EXPRESSION,
        SUB_ASSIGN_EXPRESSION,
        MULT_ASSIGN_EXPRESSION,
        DIV_ASSIGN_EXPRESSION,
        MOD_ASSIGN_EXPRESSION,
        MAYBE_ASSIGN_EXPRESSION,
        LSS_EXPRESSION,
        LEQ_EXPRESSION,
        GTR_EXPRESSION,
        GEQ_EXPRESSION,
        EQU_EXPRESSION,
        NEQ_EXPRESSION,
        LOG_AND_EXPRESSION,
        LOG_OR_EXPRESSION,
        BAND_EXPRESSION,
        BOR_EXPRESSION,
        BXOR_EXPRESSION,
        SUBSCRIPT_EXPRESSION,
        CALL_EXPRESSION,
        ACCESS_EXPRESSION,
        UNARY_PRE_EXPRESSION,
        NOT_EXPRESSION,
        BNEG_EXPRESSION,
        DEREF_EXPRESSION,
        ADDRESS_EXPRESSION,
        REF_EXPRESSION,
        NEW_EXPRESSION,
        DELETE_EXPRESSION,
        SIZEOF_EXPRESSION,
        UNARY_POST_EXPRESSION,
        AS_EXPRESSION,
        IDENTIFIER,
        INITIALZER_LIST,
        SLICE_EXPRESSION,
        DYNAMIC_ARRAY_EXPRESSION,
        LEN_EXPRESSION,
        BOOLEAN_LITERAL,
        INTEGER_LITERAL,
        FLOAT_LITERAL,
        STRING_LITERAL,
        CHAR_LITERAL,
        PROC_LITERAL,
        EXTERN_LITERAL,
        SOME_LITERAL,
        NOTHING_LITERAL,
        TUPLE_LITERAL,
        EXPR_BLOCK,
        NAMED_ARG,
        _END_EXPRESSIONS,
        DECLARATOR,
        ARRAY_DECLARATOR,
        SLICE_DECLARATOR,
        DYNAMIC_ARRAY_DECLARATOR,
        POINTER_DECLARATOR,
        MAYBE_DECLARATOR,
        SUM_DECLARATOR,
        TUPLE_DECLARATOR,
        PROCEDURE_DECLARATOR,
        REF_DECLARATOR,
        PLACEHOLDER_DECLARATOR,
        _END_DECLARATORS,
        CONSTANT,
        VARIABLE_DECLARATION,
        ALIAS,
        STRUCT,
        ENUM,
        ARG_LIST,
        THIS,
        PROCEDURE,
        NAMESPACE,
        IMPORT,
        INCLUDE,
        USING,
        PRINT,
        RETURN,
        BREAK,
        CONTINUE,
        EXPR_BLOCK_YIELD,
        IF,
        ELSE,
        FOR,
        FOREACH,
        WHILE,
        DO_WHILE,
        MATCH,
        WITH,
        TEMPLATE_DEFINE_LIST,
        TEMPLATE_DEFINE_ELEMENT,
        TEMPLATE_DEFINE_TYPE_DESCRIPTOR,
        TEMPLATE_DEFINE_VARIADIC_TYPE_ARGS,
        TEMPLATE_DEFINE_EXPRESSION,
        TEMPLATE_INSTANTIATION,
        TEMPLATE_ALIAS,
        TEMPLATE_STRUCT,
        TEMPLATE_PROC,
        SL_COMMENT,
        MODULE_DECL,
        IGNORE,
        MULTINODE,
        MACRO_USE
    };

#define ANY_NODE                                                               \
    ASTNode::NodeKind::PROC_SET, ASTNode::NodeKind::EXPRESSION,                \
        ASTNode::NodeKind::BINARY_EXPRESSION,                                  \
        ASTNode::NodeKind::ADD_EXPRESSION, ASTNode::NodeKind::SUB_EXPRESSION,  \
        ASTNode::NodeKind::BSHL_EXPRESSION,                                    \
        ASTNode::NodeKind::BSHR_EXPRESSION,                                    \
        ASTNode::NodeKind::MULT_EXPRESSION, ASTNode::NodeKind::DIV_EXPRESSION, \
        ASTNode::NodeKind::MOD_EXPRESSION,                                     \
        ASTNode::NodeKind::ASSIGNMENT_EXPRESSION,                              \
        ASTNode::NodeKind::ADD_ASSIGN_EXPRESSION,                              \
        ASTNode::NodeKind::SUB_ASSIGN_EXPRESSION,                              \
        ASTNode::NodeKind::MULT_ASSIGN_EXPRESSION,                             \
        ASTNode::NodeKind::DIV_ASSIGN_EXPRESSION,                              \
        ASTNode::NodeKind::MOD_ASSIGN_EXPRESSION,                              \
        ASTNode::NodeKind::MAYBE_ASSIGN_EXPRESSION,                            \
        ASTNode::NodeKind::LSS_EXPRESSION, ASTNode::NodeKind::LEQ_EXPRESSION,  \
        ASTNode::NodeKind::GTR_EXPRESSION, ASTNode::NodeKind::GEQ_EXPRESSION,  \
        ASTNode::NodeKind::EQU_EXPRESSION, ASTNode::NodeKind::NEQ_EXPRESSION,  \
        ASTNode::NodeKind::LOG_AND_EXPRESSION,                                 \
        ASTNode::NodeKind::LOG_OR_EXPRESSION,                                  \
        ASTNode::NodeKind::BAND_EXPRESSION, ASTNode::NodeKind::BOR_EXPRESSION, \
        ASTNode::NodeKind::BXOR_EXPRESSION,                                    \
        ASTNode::NodeKind::SUBSCRIPT_EXPRESSION,                               \
        ASTNode::NodeKind::CALL_EXPRESSION,                                    \
        ASTNode::NodeKind::ACCESS_EXPRESSION,                                  \
        ASTNode::NodeKind::UNARY_PRE_EXPRESSION,                               \
        ASTNode::NodeKind::NOT_EXPRESSION, ASTNode::NodeKind::BNEG_EXPRESSION, \
        ASTNode::NodeKind::DEREF_EXPRESSION,                                   \
        ASTNode::NodeKind::ADDRESS_EXPRESSION,                                 \
        ASTNode::NodeKind::REF_EXPRESSION, ASTNode::NodeKind::NEW_EXPRESSION,  \
        ASTNode::NodeKind::DELETE_EXPRESSION,                                  \
        ASTNode::NodeKind::SIZEOF_EXPRESSION,                                  \
        ASTNode::NodeKind::UNARY_POST_EXPRESSION,                              \
        ASTNode::NodeKind::AS_EXPRESSION, ASTNode::NodeKind::IDENTIFIER,       \
        ASTNode::NodeKind::INITIALZER_LIST,                                    \
        ASTNode::NodeKind::SLICE_EXPRESSION,                                   \
        ASTNode::NodeKind::DYNAMIC_ARRAY_EXPRESSION,                           \
        ASTNode::NodeKind::LEN_EXPRESSION, ASTNode::NodeKind::BOOLEAN_LITERAL, \
        ASTNode::NodeKind::INTEGER_LITERAL, ASTNode::NodeKind::FLOAT_LITERAL,  \
        ASTNode::NodeKind::STRING_LITERAL, ASTNode::NodeKind::CHAR_LITERAL,    \
        ASTNode::NodeKind::PROC_LITERAL, ASTNode::NodeKind::EXTERN_LITERAL,    \
        ASTNode::NodeKind::SOME_LITERAL, ASTNode::NodeKind::NOTHING_LITERAL,   \
        ASTNode::NodeKind::TUPLE_LITERAL, ASTNode::NodeKind::EXPR_BLOCK,       \
        ASTNode::NodeKind::_END_EXPRESSIONS, ASTNode::NodeKind::DECLARATOR,    \
        ASTNode::NodeKind::ARRAY_DECLARATOR,                                   \
        ASTNode::NodeKind::SLICE_DECLARATOR,                                   \
        ASTNode::NodeKind::DYNAMIC_ARRAY_DECLARATOR,                           \
        ASTNode::NodeKind::POINTER_DECLARATOR,                                 \
        ASTNode::NodeKind::MAYBE_DECLARATOR,                                   \
        ASTNode::NodeKind::SUM_DECLARATOR,                                     \
        ASTNode::NodeKind::TUPLE_DECLARATOR,                                   \
        ASTNode::NodeKind::PROCEDURE_DECLARATOR,                               \
        ASTNode::NodeKind::REF_DECLARATOR,                                     \
        ASTNode::NodeKind::PLACEHOLDER_DECLARATOR,                             \
        ASTNode::NodeKind::_END_DECLARATORS, ASTNode::NodeKind::CONSTANT,      \
        ASTNode::NodeKind::VARIABLE_DECLARATION, ASTNode::NodeKind::ALIAS,     \
        ASTNode::NodeKind::STRUCT, ASTNode::NodeKind::ENUM,                    \
        ASTNode::NodeKind::ARG_LIST, ASTNode::NodeKind::THIS,                  \
        ASTNode::NodeKind::PROCEDURE, ASTNode::NodeKind::NAMESPACE,            \
        ASTNode::NodeKind::IMPORT, ASTNode::NodeKind::INCLUDE, ASTNode::NodeKind::USING, ASTNode::NodeKind::PRINT,                   \
        ASTNode::NodeKind::RETURN, ASTNode::NodeKind::BREAK,                   \
        ASTNode::NodeKind::CONTINUE, ASTNode::NodeKind::IF,                    \
        ASTNode::NodeKind::ELSE, ASTNode::NodeKind::FOR,                       \
        ASTNode::NodeKind::FOREACH, ASTNode::NodeKind::WHILE,                  \
        ASTNode::NodeKind::DO_WHILE, ASTNode::NodeKind::MATCH,                 \
        ASTNode::NodeKind::WITH, ASTNode::NodeKind::TEMPLATE_DEFINE_LIST,      \
        ASTNode::NodeKind::TEMPLATE_DEFINE_ELEMENT,                            \
        ASTNode::NodeKind::TEMPLATE_DEFINE_TYPE_DESCRIPTOR,                    \
        ASTNode::NodeKind::TEMPLATE_DEFINE_VARIADIC_TYPE_ARGS,                 \
        ASTNode::NodeKind::TEMPLATE_DEFINE_EXPRESSION,                         \
        ASTNode::NodeKind::TEMPLATE_INSTANTIATION,                             \
        ASTNode::NodeKind::TEMPLATE_ALIAS, ASTNode::NodeKind::TEMPLATE_STRUCT, \
        ASTNode::NodeKind::TEMPLATE_PROC, ASTNode::NodeKind::SL_COMMENT,       \
        ASTNode::NodeKind::MODULE_DECL, ASTNode::NodeKind::EXPR_BLOCK_YIELD,   \
        ASTNode::NodeKind::IGNORE, ASTNode::NodeKind::MULTINODE,               \
        ASTNode::NodeKind::MACRO_USE

#define ANY_EXPRESSION                                                         \
    ASTNode::NodeKind::EXPRESSION, ASTNode::NodeKind::BINARY_EXPRESSION,       \
        ASTNode::NodeKind::ADD_EXPRESSION, ASTNode::NodeKind::SUB_EXPRESSION,  \
        ASTNode::NodeKind::BSHL_EXPRESSION,                                    \
        ASTNode::NodeKind::BSHR_EXPRESSION,                                    \
        ASTNode::NodeKind::MULT_EXPRESSION, ASTNode::NodeKind::DIV_EXPRESSION, \
        ASTNode::NodeKind::MOD_EXPRESSION,                                     \
        ASTNode::NodeKind::ASSIGNMENT_EXPRESSION,                              \
        ASTNode::NodeKind::ADD_ASSIGN_EXPRESSION,                              \
        ASTNode::NodeKind::SUB_ASSIGN_EXPRESSION,                              \
        ASTNode::NodeKind::MULT_ASSIGN_EXPRESSION,                             \
        ASTNode::NodeKind::DIV_ASSIGN_EXPRESSION,                              \
        ASTNode::NodeKind::MOD_ASSIGN_EXPRESSION,                              \
        ASTNode::NodeKind::MAYBE_ASSIGN_EXPRESSION,                            \
        ASTNode::NodeKind::LSS_EXPRESSION, ASTNode::NodeKind::LEQ_EXPRESSION,  \
        ASTNode::NodeKind::GTR_EXPRESSION, ASTNode::NodeKind::GEQ_EXPRESSION,  \
        ASTNode::NodeKind::EQU_EXPRESSION, ASTNode::NodeKind::NEQ_EXPRESSION,  \
        ASTNode::NodeKind::LOG_AND_EXPRESSION,                                 \
        ASTNode::NodeKind::LOG_OR_EXPRESSION,                                  \
        ASTNode::NodeKind::BAND_EXPRESSION, ASTNode::NodeKind::BOR_EXPRESSION, \
        ASTNode::NodeKind::BXOR_EXPRESSION,                                    \
        ASTNode::NodeKind::SUBSCRIPT_EXPRESSION,                               \
        ASTNode::NodeKind::CALL_EXPRESSION,                                    \
        ASTNode::NodeKind::ACCESS_EXPRESSION,                                  \
        ASTNode::NodeKind::UNARY_PRE_EXPRESSION,                               \
        ASTNode::NodeKind::NOT_EXPRESSION, ASTNode::NodeKind::BNEG_EXPRESSION, \
        ASTNode::NodeKind::DEREF_EXPRESSION,                                   \
        ASTNode::NodeKind::ADDRESS_EXPRESSION,                                 \
        ASTNode::NodeKind::REF_EXPRESSION, ASTNode::NodeKind::NEW_EXPRESSION,  \
        ASTNode::NodeKind::DELETE_EXPRESSION,                                  \
        ASTNode::NodeKind::SIZEOF_EXPRESSION,                                  \
        ASTNode::NodeKind::UNARY_POST_EXPRESSION,                              \
        ASTNode::NodeKind::AS_EXPRESSION, ASTNode::NodeKind::IDENTIFIER,       \
        ASTNode::NodeKind::INITIALZER_LIST,                                    \
        ASTNode::NodeKind::SLICE_EXPRESSION,                                   \
        ASTNode::NodeKind::DYNAMIC_ARRAY_EXPRESSION,                           \
        ASTNode::NodeKind::LEN_EXPRESSION, ASTNode::NodeKind::BOOLEAN_LITERAL, \
        ASTNode::NodeKind::INTEGER_LITERAL, ASTNode::NodeKind::FLOAT_LITERAL,  \
        ASTNode::NodeKind::STRING_LITERAL, ASTNode::NodeKind::CHAR_LITERAL,    \
        ASTNode::NodeKind::PROC_LITERAL, ASTNode::NodeKind::EXTERN_LITERAL,    \
        ASTNode::NodeKind::SOME_LITERAL, ASTNode::NodeKind::NOTHING_LITERAL,   \
        ASTNode::NodeKind::TUPLE_LITERAL, ASTNode::NodeKind::EXPR_BLOCK

#define ANY_DECLARATOR                                                         \
    ASTNode::NodeKind::DECLARATOR, ASTNode::NodeKind::ARRAY_DECLARATOR,        \
        ASTNode::NodeKind::SLICE_DECLARATOR,                                   \
        ASTNode::NodeKind::DYNAMIC_ARRAY_DECLARATOR,                           \
        ASTNode::NodeKind::POINTER_DECLARATOR,                                 \
        ASTNode::NodeKind::MAYBE_DECLARATOR,                                   \
        ASTNode::NodeKind::SUM_DECLARATOR,                                     \
        ASTNode::NodeKind::TUPLE_DECLARATOR,                                   \
        ASTNode::NodeKind::PROCEDURE_DECLARATOR,                               \
        ASTNode::NodeKind::REF_DECLARATOR,                                     \
        ASTNode::NodeKind::PLACEHOLDER_DECLARATOR

#define ANY_STATEMENT                                                          \
    ASTNode::NodeKind::IGNORE, ANY_EXPRESSION, ASTNode::NodeKind::CONSTANT,    \
        ASTNode::NodeKind::VARIABLE_DECLARATION, ASTNode::NodeKind::IMPORT, ASTNode::NodeKind::INCLUDE, ASTNode::NodeKind::USING,    \
        ASTNode::NodeKind::PRINT, ASTNode::NodeKind::RETURN,                   \
        ASTNode::NodeKind::BREAK, ASTNode::NodeKind::CONTINUE,                 \
        ASTNode::NodeKind::IF, ASTNode::NodeKind::ELSE,                        \
        ASTNode::NodeKind::FOR, ASTNode::NodeKind::FOREACH,                    \
        ASTNode::NodeKind::WHILE, ASTNode::NodeKind::DO_WHILE,                 \
        ASTNode::NodeKind::MATCH, ASTNode::NodeKind::MODULE_DECL,              \
        ASTNode::NodeKind::EXPR_BLOCK_YIELD

#define IS_DECLARATOR(node)                                                    \
    ((node)->nodeKind >= ASTNode::DECLARATOR &&                                \
     (node)->nodeKind < ASTNode::_END_DECLARATORS)
#define IS_EXPRESSION(node)                                                    \
    ((node)->nodeKind >= ASTNode::EXPRESSION &&                                \
     (node)->nodeKind < ASTNode::_END_EXPRESSIONS)

#define IS_CONTROL_TERMINATOR(node)                                            \
    ((node)->nodeKind == ASTNode::RETURN ||                                    \
     (node)->nodeKind == ASTNode::BREAK  ||                                    \
     (node)->nodeKind == ASTNode::CONTINUE)

    NodeKind nodeKind;

    int flags;
    Context context;
    Context nameContext;
    Scope * scope;
    ASTNode * parent;
    std::string mod;
    replacementPolicy * replace;

    enum eBitFlags E_BIT_FLAGS();

    int getFlags() const;
    void setFlags(int _flags);
    int getFlag(int flag) const;
    void setFlag(int flag, bool _val);
    void printFlags();

    ASTNode * getParent() const;

    Context & getContext();
    void setContext(Context _context);

    Context & getNameContext();
    void setNameContext(Context _context);

    Scope * getScope() const;
    void setScope(Scope * _scope);

    // Node interface
    virtual void analyze(bool force = false) = 0;
    virtual void addSymbols(std::string& _mod, Scope * _scope) = 0;
    virtual void unwrap(std::vector<ASTNode *> & terminals);
    virtual ASTNode * clone() = 0;
    virtual void desugar();
    virtual const Type * getType();
    virtual bool isStatement() const;
    // virtual void serializePass1(Serializer& ser) = 0;
    // virtual void serializePass2(Serializer& ser) = 0;

    virtual void * generate(BackEnd & backEnd, bool flag = false);

    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);

    virtual ~ASTNode() = 0;
    //
};

bool isCT(ASTNode * node);

struct MultiNode : ASTNode {
    bool isModuleContainer;

    std::vector<ASTNode *> nodes;

    MultiNode();
    MultiNode(std::vector<ASTNode *> & _nodes);

    void take(std::vector<ASTNode *> & _nodes);

    void analyze(bool force = false);
    void addSymbols(std::string& _mod, Scope * _scope);
    void unwrap(std::vector<ASTNode *> & terminals);
    void * generate(BackEnd & backEnd, bool flag = false);

    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);

    void flatten(std::vector<ASTNode *> & out);

    ASTNode * clone();
    ~MultiNode();
};

/* ============================================================================
 *
 *                              Expression
 *  These nodes make up a tree which form an expression. Expressions can
 *  be either operators or terminal expression like 1, "hello", my_var, etc.
 *
 * ===========================================================================*/

struct Expression : ASTNode {
    Expression();

    std::string contents;
    const Type * type;
    ASTNode * left;
    ASTNode * right;

    enum eBitFlags E_BIT_FLAGS_AND(PAREN, TERMINAL, IDENT, VOLATILE_R, VOLATILE_W);

    std::string & getContents();
    void setContents(std::string _contents);
    void setType(const Type * _type);
    ASTNode * getLeft() const;
    void setLeft(ASTNode * _left);
    ASTNode * getRight() const;
    void setRight(ASTNode * _right);

    bool opOverload();
    bool canBeLVal();

    virtual bool isConstant() = 0;
    virtual Val eval();

    // Node interface
    virtual void unwrap(std::vector<ASTNode *> & terminals);
    virtual ASTNode * clone() = 0;
    virtual void desugar();
    virtual void analyze(bool force = false) = 0;
    virtual void addSymbols(std::string& _mod, Scope * _scope);

    virtual const Type * getType();
    virtual bool isStatement() const;

    virtual ~Expression();
    //
};

template <typename T> static inline T * ExpressionClone(T * expr) {
    T * c = new T(*expr);

    if (expr->getLeft())
        c->setLeft(expr->getLeft()->clone());
    if (expr->getRight())
        c->setRight(expr->getRight()->clone());

    return c;
}

/* ============================================================================
 *
 *                               ExprBlockYield
 *
 * ===========================================================================*/

/* ============================================================================
 *
 *                               BinaryExpression
 *  Expression trees made from binary operators.
 *
 * ===========================================================================*/

struct BinaryExpression : Expression {
    BinaryExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone() = 0;
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    //
};

/* ============================================================================
 *
 *                               AddExpression
 *  Expression trees made from the binary call operator +.
 *
 * ===========================================================================*/

struct AddExpression : BinaryExpression {
    AddExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~AddExpression();
    //
};

/* ============================================================================
 *
 *                               SubExpression
 *  Expression trees made from the binary call operator -.
 *
 * ===========================================================================*/

struct SubExpression : BinaryExpression {
    SubExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~SubExpression();
    //
};

/* ============================================================================
 *
 *                               BSHLExpression
 *  Expression trees made from the binary call operator bshl.
 *
 * ===========================================================================*/

struct BSHLExpression : BinaryExpression {
    BSHLExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~BSHLExpression();
    //
};

/* ============================================================================
 *
 *                               BSHRExpression
 *  Expression trees made from the binary call operator bshr.
 *
 * ===========================================================================*/

struct BSHRExpression : BinaryExpression {
    BSHRExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~BSHRExpression();
    //
};

/* ============================================================================
 *
 *                               MultExpression
 *  Expression trees made from the binary call operator *.
 *
 * ===========================================================================*/

struct MultExpression : BinaryExpression {
    MultExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~MultExpression();
    //
};

/* ============================================================================
 *
 *                               DivExpression
 *  Expression trees made from the binary call operator /.
 *
 * ===========================================================================*/

struct DivExpression : BinaryExpression {
    DivExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~DivExpression();
    //
};

/* ============================================================================
 *
 *                               ModExpression
 *  Expression trees made from the binary call operator %.
 *
 * ===========================================================================*/

struct ModExpression : BinaryExpression {
    ModExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~ModExpression();
    //
};

/* ============================================================================
 *
 *                               AssignmentExpression
 *  Expression trees made from the binary call operator =.
 *
 * ===========================================================================*/

struct AssignmentExpression : BinaryExpression {
    AssignmentExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~AssignmentExpression();
    //
};

/* ============================================================================
 *
 *                               AddAssignExpression
 *  Expression trees made from the binary call operator +=.
 *
 * ===========================================================================*/

struct AddAssignExpression : BinaryExpression {
    AddAssignExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~AddAssignExpression();
    //
};

/* ============================================================================
 *
 *                               SubAssignExpression
 *  Expression trees made from the binary call operator -=.
 *
 * ===========================================================================*/

struct SubAssignExpression : BinaryExpression {
    SubAssignExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~SubAssignExpression();
    //
};

/* ============================================================================
 *
 *                               MultAssignExpression
 *  Expression trees made from the binary call operator *=.
 *
 * ===========================================================================*/

struct MultAssignExpression : BinaryExpression {
    MultAssignExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~MultAssignExpression();
    //
};

/* ============================================================================
 *
 *                               DivAssignExpression
 *  Expression trees made from the binary call operator /=.
 *
 * ===========================================================================*/

struct DivAssignExpression : BinaryExpression {
    DivAssignExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~DivAssignExpression();
    //
};

/* ============================================================================
 *
 *                               ModAssignExpression
 *  Expression trees made from the binary call operator %=.
 *
 * ===========================================================================*/

struct ModAssignExpression : BinaryExpression {
    ModAssignExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~ModAssignExpression();
    //
};

/* ============================================================================
 *
 *                               MaybeAssignExpression
 *  Expression trees made from the binary call operator ??.
 *
 * ===========================================================================*/

struct MaybeAssignExpression : BinaryExpression {
    MaybeAssignExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~MaybeAssignExpression();
    //
};

/* ============================================================================
 *
 *                               LssExpression
 *  Expression trees made from the binary call operator <.
 *
 * ===========================================================================*/

struct LssExpression : BinaryExpression {
    LssExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~LssExpression();
    //
};

/* ============================================================================
 *
 *                               LeqExpression
 *  Expression trees made from the binary call operator <=.
 *
 * ===========================================================================*/

struct LeqExpression : BinaryExpression {
    LeqExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~LeqExpression();
    //
};

/* ============================================================================
 *
 *                               GtrExpression
 *  Expression trees made from the binary call operator >.
 *
 * ===========================================================================*/

struct GtrExpression : BinaryExpression {
    GtrExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~GtrExpression();
    //
};

/* ============================================================================
 *
 *                               GeqExpression
 *  Expression trees made from the binary call operator >=.
 *
 * ===========================================================================*/

struct GeqExpression : BinaryExpression {
    GeqExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~GeqExpression();
    //
};

/* ============================================================================
 *
 *                               EquExpression
 *  Expression trees made from the binary call operator ==.
 *
 * ===========================================================================*/

struct EquExpression : BinaryExpression {
    EquExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~EquExpression();
    //
};

/* ============================================================================
 *
 *                               NeqExpression
 *  Expression trees made from the binary call operator !=.
 *
 * ===========================================================================*/

struct NeqExpression : BinaryExpression {
    NeqExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~NeqExpression();
    //
};

/* ============================================================================
 *
 *                               LogAndExpression
 *  Expression trees made from the binary call operator && or 'and'.
 *
 * ===========================================================================*/

struct LogAndExpression : BinaryExpression {
    LogAndExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~LogAndExpression();
    //
};

/* ============================================================================
 *
 *                               LogOrExpression
 *  Expression trees made from the binary call operator || or 'or'.
 *
 * ===========================================================================*/

struct LogOrExpression : BinaryExpression {
    LogOrExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~LogOrExpression();
    //
};

/* ============================================================================
 *
 *                               BANDExpression
 *  Expression trees made from the binary call operator bshr.
 *
 * ===========================================================================*/

struct BANDExpression : BinaryExpression {
    BANDExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~BANDExpression();
    //
};

/* ============================================================================
 *
 *                               BORExpression
 *  Expression trees made from the binary call operator bshr.
 *
 * ===========================================================================*/

struct BORExpression : BinaryExpression {
    BORExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~BORExpression();
    //
};

/* ============================================================================
 *
 *                               BXORExpression
 *  Expression trees made from the binary call operator bshr.
 *
 * ===========================================================================*/

struct BXORExpression : BinaryExpression {
    BXORExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~BXORExpression();
    //
};

/* ============================================================================
 *
 *                               CallExpression
 *  Expression trees made from the binary call operator ().
 *
 * ===========================================================================*/

struct Procedure;

struct CallExpression : BinaryExpression {
    CallExpression();

    enum eBitFlags E_BIT_FLAGS_AND(PAREN, TERMINAL, IDENT);

    Procedure * resolved_proc;

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~CallExpression();
    //
};

/* ============================================================================
 *
 *                               SubscriptExpression
 *  Expression trees made from the binary call operator [].
 *
 * ===========================================================================*/

struct SubscriptExpression : BinaryExpression {
    SubscriptExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    void desugar();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~SubscriptExpression();
    //
};

/* ============================================================================
 *
 *                               AccessExpression
 *  Expression trees made from the binary call operator '.'.
 *
 * ===========================================================================*/

struct AccessExpression : BinaryExpression {
    AccessExpression();

    enum eBitFlags E_BIT_FLAGS_AND(PAREN, TERMINAL, IDENT, UFC, SUM_DATA);

    CallExpression * injection;

    CallExpression * nextCall();
    int handleThroughTemplate();
    int handleAccessThroughDeclarator(bool force);
    int handleContainerAccess();
    bool handleInjection();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~AccessExpression();
    //
};

/* ============================================================================
 *
 *                               UnaryPreExpression
 *  Expression trees made from unary prefix operators.
 *
 * ===========================================================================*/

struct UnaryPreExpression : Expression {
    UnaryPreExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone() = 0;
    // virtual ~UnaryPreExpression();
    //
};

/* ============================================================================
 *
 *                               NewExpression
 *  Expression trees made from the unary prefix operator new.
 *
 * ===========================================================================*/

struct NewExpression : UnaryPreExpression {
    NewExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~NewExpression();
    //
};

/* ============================================================================
 *
 *                               DeleteExpression
 *  Expression trees made from the unary prefix operator delete.
 *
 * ===========================================================================*/

struct DeleteExpression : UnaryPreExpression {
    DeleteExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~DeleteExpression();
    //
};

/* ============================================================================
 *
 *                               SizeofExpression
 *  Expression trees made from the unary prefix operator sizeof.
 *
 * ===========================================================================*/

struct SizeofExpression : UnaryPreExpression {
    SizeofExpression();

    bool isConstant();

    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~SizeofExpression();
    //
};

/* ============================================================================
 *
 *                               NotExpression
 *  Expression trees made from the unary prefix operators ! or 'not'.
 *
 * ===========================================================================*/

struct NotExpression : UnaryPreExpression {
    NotExpression();

    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~NotExpression();
    //
};

/* ============================================================================
 *
 *                               BNEGExpression
 *  Expression trees made from the binary call operator bshr.
 *
 * ===========================================================================*/

struct BNEGExpression : UnaryPreExpression {
    BNEGExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~BNEGExpression();
    //
};

/* ============================================================================
 *
 *                               DerefExpression
 *  Expression trees made from the unary prefix operator @.
 *
 * ===========================================================================*/

struct DerefExpression : UnaryPreExpression {
    DerefExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~DerefExpression();
    //
};

/* ============================================================================
 *
 *                               AddressExpression
 *  Expression trees made from the unary prefix operator &.
 *
 * ===========================================================================*/

struct AddressExpression : UnaryPreExpression {
    AddressExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~AddressExpression();
    //
};

/* ============================================================================
 *
 *                               RefExpression
 *  Expression trees made from the unary prefix operator ~.
 *
 * ===========================================================================*/

struct RefExpression : UnaryPreExpression {
    RefExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~RefExpression();
    //
};

/* ============================================================================
 *
 *                               UnaryPostExpression
 *  Expression trees made from unary postfix operators.
 *
 * ===========================================================================*/

struct UnaryPostExpression : Expression {
    UnaryPostExpression();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone() = 0;
    // virtual ~UnaryPostExpression();
    //
};

/* ============================================================================
 *
 *                               AsExpression
 *  Expression trees made from the unary postfix operator as.
 *
 * ===========================================================================*/

struct AsExpression : UnaryPostExpression {
    AsExpression();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~AsExpression();
    //
};

/* ============================================================================
 *
 *                              Identifier
 *  Extension of Expression. Fields, flags, access, etc. MUST start
 *  with the same items as Expression (in the same order). Represents
 *  identifiers that can be qualified with modules and types.
 *  Used in symbol lookup.
 *
 * ===========================================================================*/

struct Identifier : Expression {
    Identifier();

    std::string
        sym_name,
        sym_mod,
        sym_type,
        sym_all;
    enum eBitFlags E_BIT_FLAGS_AND(PAREN, TERMINAL, IDENT);

    ASTNode * resolved;

    std::string & getSymName();
    void setSymName(std::string _name);
    std::string & getSymMod();
    void setSymMod(std::string _mod);
    std::string & getSymType();
    void setSymType(std::string _type);

    std::string symAll();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);

    virtual const Type * getType();

    virtual ~Identifier();
    //
};

/* ============================================================================
 *
 *                              InitializerList
 *  Extension of Expression. Fields, flags, access, etc. MUST start
 *  with the same items as Expression (in the same order). Object or array
 *  literals. Unlike C, these are not anonymous and must be typed before
 *  considered valid.
 *
 * ===========================================================================*/

struct InitializerList : Expression {
    InitializerList();

    ASTNode * objDeclarator;
    std::vector<std::string> memberNames;
    std::vector<ASTNode *> expressions;

    ASTNode * getObjDeclarator() const;
    void setObjDeclarator(ASTNode * _decl);
    std::vector<std::string> & getMemberNames();
    void setMemberNames(std::vector<std::string> _memberNamess);
    void addMemberName(std::string memberName);
    std::vector<ASTNode *> & getExpressions();
    void setExpressions(std::vector<ASTNode *> _expressions);
    void addExpression(ASTNode * expression);

    bool isConstant();

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    virtual void analyze(bool force = false);
    ASTNode * clone();
    void desugar();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual ~InitializerList();
    //
};

/* ============================================================================
 *
 *                             SliceExpression
 *  Expression to create a slice of a static array, dynamic array, or another
 *  slice. syntax: '[ src, start_index:length ]'
 *
 * ===========================================================================*/

struct SliceExpression : Expression {
    SliceExpression();

    ASTNode * src;
    ASTNode * start;
    ASTNode * length;

    ASTNode * getSrc() const;
    void setSrc(ASTNode * _src);
    ASTNode * getStart() const;
    void setStart(ASTNode * _start);
    ASTNode * getLength() const;
    void setLength(ASTNode * _length);

    bool isConstant();

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    virtual void analyze(bool force = false);
    ASTNode * clone();
    void desugar();
    // no generation, will be desugared
    // virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~SliceExpression();
    //
};

/* ============================================================================
 *
 *                             DynamicArrayExpression
 *  Expression to create a dynamic array
 *  syntax: '[...type]'
 *
 * ===========================================================================*/

struct DynamicArrayExpression : Expression {
    DynamicArrayExpression();

    ASTNode * typeDeclarator;

    ASTNode * getTypeDeclarator() const;
    void setTypeDeclarator(ASTNode * _decl);

    bool isConstant();

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    virtual void analyze(bool force = false);
    ASTNode * clone();
    void desugar();
    // no generation, will be desugared
    // virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~DynamicArrayExpression();
    //
};

/* ============================================================================
 *
 *                            LenExpression
 *  A cardinality expression for arrays and slices. |expr|
 *
 * ===========================================================================*/

struct LenExpression : Expression {
    LenExpression();

    ASTNode * expr;

    ASTNode * getExpr() const;
    void setExpr(ASTNode * _expr);

    bool isConstant();

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    virtual void analyze(bool force = false);
    ASTNode * clone();
    void desugar();
    // no generation, will be desugared
    // virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~LenExpression();
    //
};

/* ============================================================================
 *
 *                              BooleanLiteral
 *  Derivation of Expression. Fields, flags, access, etc. MUST start
 *  with the same items as Expression (in the same order). Expression for
 *  literal boolean values true and false.
 *
 * ===========================================================================*/

struct BooleanLiteral : Expression {
    BooleanLiteral();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~BooleanLiteral();
    //
};

/* ============================================================================
 *
 *                              IntegerLiteral
 *  Derivation of Expression. Fields, flags, access, etc. MUST start
 *  with the same items as Expression (in the same order). Expression for
 *  literal integer values.
 *
 * ===========================================================================*/

struct IntegerLiteral : Expression {
    IntegerLiteral();

    bool isConstant();
    Val eval();

    bool is_hex;
    std::string suffix;
    bool is_signed;
    bool is_neg;

    uint64_t getAsUnsigned();
    int64_t getAsSigned();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~IntegerLiteral();
    //
};

/* ============================================================================
 *
 *                              FloatLiteral
 *  Derivation of Expression. Fields, flags, access, etc. MUST start
 *  with the same items as Expression (in the same order). Expression for
 *  literal floating point values.
 *
 * ===========================================================================*/

struct FloatLiteral : Expression {
    FloatLiteral();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);

    // virtual ~FloatLiteral();
    //
};

/* ============================================================================
 *
 *                              StringLiteral
 *  Derivation of Expression. Fields, flags, access, etc. MUST start
 *  with the same items as Expression (in the same order). Expression for
 *  literal string values.
 *
 * ===========================================================================*/

struct StringLiteral : Expression {
    StringLiteral();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~StringLiteral();
    //
};

/* ============================================================================
 *
 *                              CharLiteral
 *  Derivation of Expression. Fields, flags, access, etc. MUST start
 *  with the same items as Expression (in the same order). Expression for
 *  literal characer values.
 *
 * ===========================================================================*/

struct CharLiteral : Expression {
    CharLiteral();

    bool isConstant();
    Val eval();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~CharLiteral();
    //
};

/* ============================================================================
 *
 *                              ProcLiteral
 *  Derivation of Expression. Fields, flags, access, etc. MUST start
 *  with the same items as Expression (in the same order). Expression for
 *  literal procedure values.
 *
 * ===========================================================================*/

struct ProcLiteral : UnaryPreExpression {
    ProcLiteral();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~ProcLiteral();
    //
};

/* ============================================================================
 *
 *                              ExternLiteral
 *  Derivation of Expression. Fields, flags, access, etc. MUST start
 *  with the same items as Expression (in the same order). Expression for
 *  literal procedure values.
 *
 * ===========================================================================*/

struct ExternLiteral : UnaryPreExpression {
    ExternLiteral();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~ExternLiteral();
    //
};

/* ============================================================================
 *
 *                              SomeLiteral
 *  Derivation of Expression. Fields, flags, access, etc. MUST start
 *  with the same items as Expression (in the same order). Expression for
 *  literal some (valid maybe values) values.
 *
 * ===========================================================================*/

struct SomeLiteral : UnaryPreExpression {
    SomeLiteral();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~SomeLiteral();
    //
};

/* ============================================================================
 *
 *                              NothingLiteral
 *  Derivation of Expression. Fields, flags, access, etc. MUST start
 *  with the same items as Expression (in the same order). Expression for
 *  literal nothing (invalid maybe values) values.
 *
 * ===========================================================================*/

struct NothingLiteral : Expression {
    NothingLiteral();

    bool isConstant();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~NothingLiteral();
    //
};

/* ============================================================================
 *
 *                              TupleLiteral
 *  Derivation of Expression. Fields, flags, access, etc. MUST start
 *  with the same items as Expression (in the same order). Expression for
 *  literal tuples. i.e. (1, "hello")
 *
 * ===========================================================================*/

struct TupleLiteral : Expression {
    TupleLiteral();

    std::vector<ASTNode *> subExpressions;

    std::vector<ASTNode *> & getSubExpressions();
    void setSubExpressions(std::vector<ASTNode *> _subExpressions);
    void addSubExpression(ASTNode * _subExpression);

    bool isConstant();

    // Node interface
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void analyze(bool force = false);
    ASTNode * clone();
    void desugar();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~TupleLiteral();
    //
};

/* ============================================================================
 *
 *                              ExprBlock
 *  A block of statements that acts as an expression.
 *  Uses ExprBlockYield statements to set the value of the block.
 *  Each ExprBlockYield within a block must yield expressions of
 *  compatible types. Serves as a more powerful and generic version
 *  of C's ternary operator. Example:
 *
 *  C:    'T val = condition ? value1 : value2;'
 *  bJou: 'val := << if condition <-value1 else <-value2 >>'
 *
 * ===========================================================================*/

struct ExprBlockYield;
typedef std::pair<ExprBlockYield *, const Type *> YieldType;

struct ExprBlock : Expression {
    ExprBlock();

    std::vector<ASTNode *> statements;

    // filled by children
    std::vector<YieldType> yieldTypes;

    std::vector<ASTNode *> & getStatements();
    void setStatements(std::vector<ASTNode *> _statements);
    void addStatement(ASTNode * _statement);

    bool isConstant();

    // Node interface
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void analyze(bool force = false);
    ASTNode * clone();
    void desugar();
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~ExprBlock();
    //
};


/* ============================================================================
 *
 *                              NamedArg
 *
 *  p(arg_name: expr)
 * ===========================================================================*/

struct NamedArg : Expression {
    NamedArg();


    std::string arg_name;
    ASTNode *expression;

    std::string& getName();
    void setName(std::string name);
    ASTNode * getExpression();
    void setExpression(ASTNode * _expression);

    bool isConstant();

    // Node interface
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void analyze(bool force = false);
    ASTNode * clone();
    void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // virtual ~NamedArg();
    //
};






/* ============================================================================
 *
 *                              Declarators
 *  Declarators are syntax tree nodes that describe a type within the language
 *  type system. They consist of at least a type name and can be pointers to
 *  the n degree, arrays, etc, and may have additional type specifiers.
 *  Declarators also contain information for instantiating template types.
 *
 * ===========================================================================*/

/* ============================================================================
 *
 *                              Declarator
 *  This is the base type from which all Declarator types will derive. This type
 *  has all of the information needed to get from ASTNode to a derived
 *  declarator type. This structure also represents simple types such as int or
 *  char or a simple user-defined type.
 *
 * ===========================================================================*/

struct Declarator : ASTNode {
    Declarator();

    ASTNode * identifier;
    ASTNode * templateInst;
    std::vector<std::string> typeSpecifiers;

    // If this declarator was created by Type::GetGenericDeclarator(),
    // we don't want to delete child nodes like ArrayDeclarator::expression
    // because they should already have owners.
    bool createdFromType;

    const Type * type = nullptr;

    std::string str_rep;

    enum eBitFlags E_BIT_FLAGS_AND(IMPLIES_COMPLETE);

    ASTNode * getIdentifier() const;
    void setIdentifier(ASTNode * _identifier);
    ASTNode * getTemplateInst() const;
    void setTemplateInst(ASTNode * _templateInst);
    std::vector<std::string> & getTypeSpecifiers();
    void setTypeSpecifiers(std::vector<std::string> _specifiers);
    void addTypeSpecifier(std::string specifier);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual const Type * getType();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~Declarator();
    //

    // Declarator interface
    virtual std::string asString();
    virtual const ASTNode * getBase() const;
    virtual ASTNode * under() const;
    virtual void propagateScope(Scope * _scope);
    //
};

template <typename T> static inline T * DeclaratorClone(T * decl) {
    T * c = new T(*decl);

    if (c->getIdentifier())
        c->setIdentifier(c->getIdentifier()->clone());
    if (c->getTemplateInst())
        c->setTemplateInst(c->getTemplateInst()->clone());

    return c;
}

/* ============================================================================
 *
 *                              ArrayDeclarator
 *  Describes a static array type. Size is found in the expression field as an
 *  Expression.
 *
 * ===========================================================================*/

struct ArrayDeclarator : Declarator {
    ArrayDeclarator();
    ArrayDeclarator(ASTNode * _arrayOf);
    ArrayDeclarator(ASTNode * _arrayOf, ASTNode * _expression);

    ASTNode * arrayOf;
    ASTNode * expression;
    int size;

    ASTNode * getArrayOf() const;
    void setArrayOf(ASTNode * _arrayOf);
    ASTNode * getExpression() const;
    void setExpression(ASTNode * _expression);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    void addSymbols(std::string& _mod, Scope * _scope);
    virtual void analyze(bool force = false);
    virtual const Type * getType();
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~ArrayDeclarator();
    //

    // Declarator interface
    virtual std::string asString();
    virtual const ASTNode * getBase() const;
    virtual ASTNode * under() const;
    void propagateScope(Scope * _scope);
    //
};

/* ============================================================================
 *
 *                              SliceDeclarator
 *  Describes a slice type.
 *
 * ===========================================================================*/

struct SliceDeclarator : Declarator {
    SliceDeclarator();
    SliceDeclarator(ASTNode * _sliceOf);

    ASTNode * sliceOf;

    ASTNode * getSliceOf() const;
    void setSliceOf(ASTNode * _sliceOf);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    void addSymbols(std::string& _mod, Scope * _scope);
    virtual void analyze(bool force = false);
    virtual const Type * getType();
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~SliceDeclarator();
    //

    // Declarator interface
    virtual std::string asString();
    virtual const ASTNode * getBase() const;
    virtual ASTNode * under() const;
    void propagateScope(Scope * _scope);
    //
};

/* ============================================================================
 *
 *                              DynamicArrayDeclarator
 *  Describes a growable array type.
 *
 * ===========================================================================*/

struct DynamicArrayDeclarator : Declarator {
    DynamicArrayDeclarator();
    DynamicArrayDeclarator(ASTNode * _arrayOf);

    ASTNode * arrayOf;

    ASTNode * getArrayOf() const;
    void setArrayOf(ASTNode * _arrayOf);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    void addSymbols(std::string& _mod, Scope * _scope);
    virtual void analyze(bool force = false);
    virtual const Type * getType();
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~DynamicArrayDeclarator();
    //

    // Declarator interface
    virtual std::string asString();
    virtual const ASTNode * getBase() const;
    virtual ASTNode * under() const;
    void propagateScope(Scope * _scope);

    //
};

/* ============================================================================
 *
 *                              PointerDeclarator
 *  Describes a pointer type.
 *
 * ===========================================================================*/

struct PointerDeclarator : Declarator {
    PointerDeclarator();
    PointerDeclarator(ASTNode * _pointerOf);

    ASTNode * pointerOf;

    ASTNode * getPointerOf() const;
    void setPointerOf(ASTNode * _pointerOf);

    // Node interface
    void addSymbols(std::string& _mod, Scope * _scope);
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual const Type * getType();
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~PointerDeclarator();
    //

    // Declarator interface
    virtual std::string asString();
    virtual const ASTNode * getBase() const;
    virtual ASTNode * under() const;
    void propagateScope(Scope * _scope);

    //
};

/* ============================================================================
 *
 *                              RefDeclarator
 *  Describes a reference type.
 *
 * ===========================================================================*/

struct RefDeclarator : Declarator {
    RefDeclarator();
    RefDeclarator(ASTNode * _pointerOf);

    ASTNode * refOf;

    ASTNode * getRefOf() const;
    void setRefOf(ASTNode * _refOf);

    // Node interface
    void addSymbols(std::string& _mod, Scope * _scope);
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual const Type * getType();
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~RefDeclarator();
    //

    // Declarator interface
    virtual std::string asString();
    virtual const ASTNode * getBase() const;
    virtual ASTNode * under() const;
    void propagateScope(Scope * _scope);

    //
};

/* ============================================================================
 *
 *                              MaybeDeclarator
 *  Describes an optional type. Denoted by a question mark ('int?').
 *
 * ===========================================================================*/

struct MaybeDeclarator : Declarator {
    MaybeDeclarator();
    MaybeDeclarator(ASTNode * _maybeOf);

    ASTNode * maybeOf;

    ASTNode * getMaybeOf() const;
    void setMaybeOf(ASTNode * _maybeOf);

    // Node interface
    void addSymbols(std::string& _mod, Scope * _scope);
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual const Type * getType();
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~MaybeDeclarator();
    //

    // Declarator interface
    virtual std::string asString();
    virtual const ASTNode * getBase() const;
    virtual ASTNode * under() const;
    void propagateScope(Scope * _scope);

    //
};

/* ============================================================================
 *
 *                              SumDeclarator
 *  Describes a sum type.
 *
 * ===========================================================================*/

struct SumDeclarator : Declarator {
    SumDeclarator();

    std::vector<ASTNode *> subDeclarators;

    std::vector<ASTNode *> & getSubDeclarators();
    void setSubDeclarators(std::vector<ASTNode *> _subDeclarators);
    void addSubDeclarator(ASTNode * _subDeclarator);

    // Node interface
    void addSymbols(std::string& _mod, Scope * _scope);
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual const Type * getType();
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~SumDeclarator();
    //

    // Declarator interface
    virtual std::string asString();
    virtual const ASTNode * getBase() const;
    void propagateScope(Scope * _scope);

    //
};


/* ============================================================================
 *
 *                              TupleDeclarator
 *  Describes a tuple type. Tuples are multiple types grouped together as a
 *  package, i.e. (int, double)
 *
 * ===========================================================================*/

struct TupleDeclarator : Declarator {
    TupleDeclarator();

    std::vector<ASTNode *> subDeclarators;

    std::vector<ASTNode *> & getSubDeclarators();
    void setSubDeclarators(std::vector<ASTNode *> _subDeclarators);
    void addSubDeclarator(ASTNode * _subDeclarator);

    // Node interface
    void addSymbols(std::string& _mod, Scope * _scope);
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual const Type * getType();
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~TupleDeclarator();
    //

    // Declarator interface
    virtual std::string asString();
    virtual const ASTNode * getBase() const;
    void propagateScope(Scope * _scope);

    //
};

/* ============================================================================
 *
 *                              ProcedureDeclarator
 *  Describes a procedure type. In the bJou language, procedures are
 *  'first-class citizens'. A declarator for a procedure taking no arguments and
 *  returning nothing is simply '<()>'. A declarator describing a procedure
 * taking two ints and returning an int looks like '<(int, int) -> int>'. A
 * procedure that has the same signature as C's printf is '<(immut char*, ...)
 * -> int>'. This data structure describes any of these procedure types.
 *
 * ===========================================================================*/

struct ProcedureDeclarator : Declarator {
    ProcedureDeclarator();

    std::vector<ASTNode *> paramDeclarators;
    ASTNode * retDeclarator;

    enum eBitFlags E_BIT_FLAGS_AND(IMPLIES_COMPLETE, IS_VARARG);

    std::vector<ASTNode *> & getParamDeclarators();
    void setParamDeclarators(std::vector<ASTNode *> _paramDeclarators);
    void addParamDeclarator(ASTNode * paramDeclarator);
    ASTNode * getRetDeclarator() const;
    void setRetDeclarator(ASTNode * _retDeclarator);

    // Node interface
    void addSymbols(std::string& _mod, Scope * _scope);
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual const Type * getType();
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~ProcedureDeclarator();
    //

    // Declarator interface
    virtual std::string asString();
    virtual const ASTNode * getBase() const;
    void propagateScope(Scope * _scope);

    //
};

/* ============================================================================
 *
 *                              PlaceholderDeclarator
 *  Describes a temporarily unknown type. Used in This.
 *
 * ===========================================================================*/

struct PlaceholderDeclarator : Declarator {
    PlaceholderDeclarator();

    // Node interface
    void addSymbols(std::string& _mod, Scope * _scope);
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual const Type * getType();
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~PlaceholderDeclarator();
    //

    // Declarator interface
    virtual std::string asString();
    virtual const ASTNode * getBase() const;
    //
};

// end Declarators

/* ============================================================================
 *
 *                              Constant
 *  Constants are expressions that can be type checked, validated, and even
 *  evaluated at compile time.
 *
 * ===========================================================================*/

struct Constant : ASTNode {
    Constant();

    std::string name;
    std::string lookupName;
    std::string mangledName;
    ASTNode * typeDeclarator;
    ASTNode * initialization;

    enum eBitFlags E_BIT_FLAGS_AND(C_BUILTIN_MACRO, IS_TYPE_MEMBER);

    std::string & getName();
    void setName(std::string _name);
    std::string & getLookupName();
    void setLookupName(std::string _loopupName);
    ASTNode * getTypeDeclarator() const;
    std::string & getMangledName();
    void setMangledName(std::string _mangledName);
    void setTypeDeclarator(ASTNode * _typeDeclarator);
    ASTNode * getInitialization() const;
    void setInitialization(ASTNode * _initialization);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual bool isStatement() const;
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~Constant();
    //

    const Type * getType();
};

/* ============================================================================
 *
 *                              VariableDeclaration
 *  Holds data and nodes pertaining to variable creation. This can be regular
 *  stack variables, procedure arguments, or type members. This compiler has
 *  type inferring capabilities, so there is some data here that is used for
 *  that.
 *
 * ===========================================================================*/

struct VariableDeclaration : ASTNode {
    VariableDeclaration();

    std::string name;
    std::string lookupName;
    std::string mangledName;
    ASTNode * typeDeclarator;
    ASTNode * initialization;

    enum eBitFlags E_BIT_FLAGS_AND(IS_EXTERN, IS_TYPE_MEMBER, IS_PROC_PARAM, IS_DESTRUCTURE, DESTRUCTURE_REF);

    std::string & getName();
    void setName(std::string _name);
    std::string & getLookupName();
    void setLookupName(std::string _loopupName);
    std::string & getMangledName();
    void setMangledName(std::string _mangledName);
    ASTNode * getTypeDeclarator() const;
    void setTypeDeclarator(ASTNode * _typeDeclarator);
    ASTNode * getInitialization() const;
    void setInitialization(ASTNode * _initialization);

    void analyze_destructure(Symbol * sym);

    // Node interface
    const Type * getType();
    bool isStatement() const;
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~VariableDeclaration();
    //
};

/* ============================================================================
 *
 *                              Alias
 *  Alias is bJou's equivalent of C's typedef.
 *
 * ===========================================================================*/

struct Alias : ASTNode {
    Alias();

    std::string name;
    std::string lookupName;
    ASTNode * declarator;

    enum eBitFlags E_BIT_FLAGS();

    std::string & getName();
    void setName(std::string _name);
    std::string & getLookupName();
    void setLookupName(std::string _loopupName);
    ASTNode * getDeclarator() const;
    void setDeclarator(ASTNode * _declarator);

    // Node interface
    const Type * getType();
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~Alias();
    //
};

/* ============================================================================
 *
 *                              Struct
 *  This node represents all data used in defining types in bJou. This includes
 *  information about type inheritance and interfaces.
 *
 * ===========================================================================*/

struct Struct : ASTNode {
    Struct();

    std::string name;
    std::string lookupName;
    std::string mangledName;
    ASTNode * extends;
    std::vector<ASTNode *> memberVarDecls;
    std::vector<ASTNode *> constantDecls;
    std::vector<ASTNode *> memberProcs;
    std::vector<ASTNode *> memberTemplateProcs;

    ASTNode * inst; // not owned!

    enum eBitFlags E_BIT_FLAGS_AND(IS_ABSTRACT, IS_C_UNION,
                                   IS_TEMPLATE_DERIVED);

    std::string & getName();
    void setName(std::string _name);
    std::string & getLookupName();
    void setLookupName(std::string _loopupName);
    std::string & getMangledName();
    void setMangledName(std::string _mangledName);
    ASTNode * getExtends() const;
    void setExtends(ASTNode * _extends);
    std::vector<ASTNode *> & getMemberVarDecls();
    void setMemberVarDecls(std::vector<ASTNode *> _memberVarDecls);
    void addMemberVarDecl(ASTNode * memberVarDecl);
    std::vector<ASTNode *> & getConstantDecls();
    void setConstantDecls(std::vector<ASTNode *> _constantDecls);
    void addConstantDecl(ASTNode * constantDecl);
    std::vector<ASTNode *> & getMemberProcs();
    void setMemberProcs(std::vector<ASTNode *> _memberProcs);
    void addMemberProc(ASTNode * memberProc);
    std::vector<ASTNode *> & getMemberTemplateProcs();
    void setMemberTemplateProcs(std::vector<ASTNode *> _memberTemplateProcs);
    void addMemberTemplateProc(ASTNode * memberTemplateProc);

    void preDeclare(std::string& _mod, Scope * _scope);

    // Node interface
    const Type * getType();
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~Struct();
    //
};


/* ============================================================================
 *
 *                              Enum
 *  This node represents a classical enumeration structure that distributes
 *  numeric value to identifiers.
 *
 * ===========================================================================*/

struct Enum : ASTNode {
    Enum();

    std::string name;
    std::string lookupName;
    std::string mangledName;
    std::vector<std::string> identifiers;

    enum eBitFlags E_BIT_FLAGS();

    std::string & getName();
    void setName(std::string _name);
    std::string & getLookupName();
    void setLookupName(std::string _loopupName);
    std::string & getMangledName();
    void setMangledName(std::string _mangledName);
    std::vector<std::string> & getIdentifiers();
    void setIdentifiers(std::vector<std::string> _identifiers);
    void addIdentifier(std::string _identifier);

    // Node interface
    const Type * getType();
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~Enum();
    //
};

/* ============================================================================
 *
 *                              ArgList
 *  List of Expressions to be passed to a procedure.
 *
 * ===========================================================================*/

struct ArgList : ASTNode {
    ArgList();

    std::vector<ASTNode *> expressions;

    enum eBitFlags E_BIT_FLAGS();

    std::vector<ASTNode *> & getExpressions();
    void setExpressions(std::vector<ASTNode *> _expressions);
    void addExpression(ASTNode * _expression);
    void addExpressionToFront(ASTNode * _expression);

    // Node interface
    virtual void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~ArgList();
    //
};

/* ============================================================================
 *
 *                              This
 *  Syntactic sugar to create parameter with type equal to pointer of current
 *  type.
 *
 * ===========================================================================*/

struct This : ASTNode {
    This();

    // Node interface
    const Type * getType();
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~This();
    //
};

/* ============================================================================
 *
 *                              Procedure
 *  Data structure encapsulating a procedure, or routine.
 *
 * ===========================================================================*/

struct Procedure : ASTNode {
    Procedure();

    std::string name;
    std::string lookupName;
    std::string mangledName;
    std::vector<ASTNode *> paramVarDeclarations;
    ASTNode * retDeclarator;
    ASTNode * procDeclarator;
    std::vector<ASTNode *> statements;

    const Type * type;
    ASTNode * inst; // not owned!

    enum eBitFlags E_BIT_FLAGS_AND(IS_ANONYMOUS, IS_TEMPLATE_DERIVED, IS_EXTERN,
                                   IS_VARARG, IS_TYPE_MEMBER, NO_MANGLE, IS_INLINE);

    std::string & getName();
    void setName(std::string _name);
    std::string & getLookupName();
    void setLookupName(std::string _loopupName);
    std::string & getMangledName();
    void setMangledName(std::string _mangledName);
    std::vector<ASTNode *> & getParamVarDeclarations();
    void setParamVarDeclarations(std::vector<ASTNode *> _paramVarDeclarations);
    void addParamVarDeclaration(ASTNode * _paramVarDeclaration);
    ASTNode * getRetDeclarator() const;
    void setRetDeclarator(ASTNode * _retDeclarator);
    ASTNode * getProcDeclarator() const;
    void setProcDeclarator(ASTNode * _procDeclarator);
    std::vector<ASTNode *> & getStatements();
    void setStatements(std::vector<ASTNode *> _statements);
    void addStatement(ASTNode * _statement);

    Struct * getParentStruct();
    void desugarThis();

    // Node interface
    const Type * getType();
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);

    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);

    virtual ~Procedure();
    //
};

/* ============================================================================
 *
 *                              Import
 *  Import is the statement that directs the compiler to import a module and
 *  link to its binaries. These statements are only allowed at the global level.
 *
 * ===========================================================================*/

struct Module;

struct Import : ASTNode {
    Import();

    bool fileError, notModuleError;
    std::string module;
    std::string full_path;
    Module * theModule;

    enum eBitFlags E_BIT_FLAGS_AND(FROM_PATH);

    std::string & getModule();
    void setModule(std::string _module);

    void activate(bool ct);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~Import();
    //
};

/* ============================================================================
 *                              Include
 * ===========================================================================*/

struct Include : ASTNode {
    Include();

    std::string full_path;

    enum eBitFlags E_BIT_FLAGS();

    std::string & getPath();
    void setPath(std::string _path);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~Include();
    //
};

struct Using : ASTNode {
    Using();

    Import * import;
    std::string module;
    
    enum eBitFlags E_BIT_FLAGS();

    Import * getImport();
    void setImport(Import * _import);
    std::string& getModule();
    void setModule(std::string _module);
    
    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~Using();
    //
};

/* ============================================================================
 *
 *                              Print
 *  Print is a built-in statement that prints values to stdout. In the
 *  compiler's preliminary stages, I will keep this in to ease debugging, but
 *  eventually this will be replaced by procedures in an IO library.
 *
 * ===========================================================================*/

struct Print : ASTNode {
    Print();

    ASTNode * args;

    enum eBitFlags E_BIT_FLAGS();

    ASTNode * getArgs() const;
    void setArgs(ASTNode * _args);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);

    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual bool isStatement() const;
    virtual ~Print();
    //
};

/* ============================================================================
 *
 *                              Return
 *  Returns control to the point of execution before the current procedure was
 *  called.
 *
 * ===========================================================================*/

struct Return : ASTNode {
    Return();

    ASTNode * expression;

    enum eBitFlags E_BIT_FLAGS();

    ASTNode * getExpression() const;
    void setExpression(ASTNode * _expression);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    bool isStatement() const;
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);

    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);

    virtual ~Return();
    //
};

/* ============================================================================
 *
 *                              Break
 *  Exit control of loop
 *
 * ===========================================================================*/

struct Break : ASTNode {
    Break();

    enum eBitFlags E_BIT_FLAGS();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    bool isStatement() const;
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~Break();
    //
};

/* ============================================================================
 *
 *                              Continue
 *  Continue to next iteration of loop
 *
 * ===========================================================================*/

struct Continue : ASTNode {
    Continue();

    enum eBitFlags E_BIT_FLAGS();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    bool isStatement() const;
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~Continue();
    //
};

/* ============================================================================
 *
 *                              ExprBlockYield
 *  Sets the value of the containing ExprBlock.
 *
 * ===========================================================================*/

struct ExprBlockYield : ASTNode {
    ExprBlockYield();

    ASTNode * expression;

    enum eBitFlags E_BIT_FLAGS();

    ASTNode * getExpression() const;
    void setExpression(ASTNode * _expression);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    bool isStatement() const;
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);

    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);

    virtual ~ExprBlockYield();
    //
};

/* ============================================================================
 *
 *                              If
 *  Conditional statement that executes statements if the conditional expression
 *  evaluates to true.
 *
 * ===========================================================================*/

struct If : ASTNode {
    If();

    ASTNode * conditional;
    std::vector<ASTNode *> statements;
    ASTNode * _else;
    bool shouldEmitMerge;

    enum eBitFlags E_BIT_FLAGS_AND(HAS_DESTRUCTURE);

    ASTNode * getConditional() const;
    void setConditional(ASTNode * _conditional);
    std::vector<ASTNode *> & getStatements();
    void setStatements(std::vector<ASTNode *> _statements);
    void addStatement(ASTNode * _statement);
    ASTNode * getElse() const;
    void setElse(ASTNode * __else);

    bool alwaysReturns() const;

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    bool isStatement() const;
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~If();
    //
};

/* ============================================================================
 *
 *                              Else
 *  Provides statements to execute when a conditional statement evaluates to
 *  false.
 *
 * ===========================================================================*/

struct Else : ASTNode {
    Else();

    std::vector<ASTNode *> statements;

    enum eBitFlags E_BIT_FLAGS();

    std::vector<ASTNode *> & getStatements();
    void setStatements(std::vector<ASTNode *> _statements);
    void addStatement(ASTNode * _statement);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    bool isStatement() const;
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~Else();
    //
};

/* ============================================================================
 *
 *                              For
 *  The traditional 'for' loop. Provides a variable initialization, conditional
 *  checking, and statements to control the loop as well as statements to
 *  execute when the condition is true.
 *
 * ===========================================================================*/

struct For : ASTNode {
    For();

    std::vector<ASTNode *> initializations;
    ASTNode * conditional;
    std::vector<ASTNode *> afterthoughts;
    std::vector<ASTNode *> statements;

    enum eBitFlags E_BIT_FLAGS();

    std::vector<ASTNode *> & getInitializations();
    void setInitializations(std::vector<ASTNode *> _initializations);
    void addInitialization(ASTNode * _initialization);
    ASTNode * getConditional() const;
    void setConditional(ASTNode * _conditional);
    std::vector<ASTNode *> & getAfterthoughts();
    void setAfterthoughts(std::vector<ASTNode *> _afterthoughts);
    void addAfterthought(ASTNode * afterthought);
    std::vector<ASTNode *> & getStatements();
    void setStatements(std::vector<ASTNode *> _statements);
    void addStatement(ASTNode * _statement);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    bool isStatement() const;
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~For();
    //
};

/* ============================================================================
 *
 *                              Foreach
 *  Loop over array, slice, or dynamic array elements
 *
 * ===========================================================================*/

struct Foreach : ASTNode {
    Foreach();

    Context identContext;
    std::string ident;
    ASTNode * expression;
    std::vector<ASTNode *> statements;

    enum eBitFlags E_BIT_FLAGS_AND(TAKE_REF);

    Context & getIdentContext();
    void setIdentContext(Context & _context);
    std::string & getIdent();
    void setIdent(std::string & _ident);
    ASTNode * getExpression();
    void setExpression(ASTNode * _expr);
    std::vector<ASTNode *> & getStatements();
    void setStatements(std::vector<ASTNode *> _statements);
    void addStatement(ASTNode * _statement);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    bool isStatement() const;
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    // no generate(). will be desugared
    // virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual ~Foreach();
    //
};

/* ============================================================================
 *
 *                              While
 *  Standard loop until condition is false.
 *
 * ===========================================================================*/

struct While : ASTNode {
    While();

    ASTNode * conditional;
    std::vector<ASTNode *> statements;

    enum eBitFlags E_BIT_FLAGS_AND(HAS_DESTRUCTURE);

    ASTNode * getConditional() const;
    void setConditional(ASTNode * _conditional);
    std::vector<ASTNode *> & getStatements();
    void setStatements(std::vector<ASTNode *> _statements);
    void addStatement(ASTNode * _statement);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    bool isStatement() const;
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~While();
    //
};

/* ============================================================================
 *
 *                              DoWhile
 *  Execute statements, check condition, loop if true.
 *
 * ===========================================================================*/

struct DoWhile : ASTNode {
    DoWhile();

    ASTNode * conditional;
    std::vector<ASTNode *> statements;

    enum eBitFlags E_BIT_FLAGS();

    ASTNode * getConditional() const;
    void setConditional(ASTNode * _conditional);
    std::vector<ASTNode *> & getStatements();
    void setStatements(std::vector<ASTNode *> _statements);
    void addStatement(ASTNode * _statement);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    bool isStatement() const;
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~DoWhile();
    //
};

/* ============================================================================
 *
 *                              Match
 *  Essentially C's switch statement. I chose what are, in my opinion, better
 *  naming constructs for this kind of statement. See docs
 *
 * ===========================================================================*/

struct Match : ASTNode {
    Match();

    ASTNode * expression;
    std::vector<ASTNode *> withs;

    enum eBitFlags E_BIT_FLAGS();

    ASTNode * getExpression() const;
    void setExpression(ASTNode * _expression);
    std::vector<ASTNode *> & getWiths();
    void setWiths(std::vector<ASTNode *> _withs);
    void addWith(ASTNode * _with);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    bool isStatement() const;
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~Match();
    //
};

/* ============================================================================
 *
 *                              With
 *  Equivalent of C's case. Creates a jumping point for Match statements. Can
 *  only exist in Match statements
 *
 * ===========================================================================*/

struct With : ASTNode {
    With();

    ASTNode * expression;
    std::vector<ASTNode *> statements;

    enum eBitFlags E_BIT_FLAGS_AND(WITH_ELSE);

    ASTNode * getExpression() const;
    void setExpression(ASTNode * _expression);
    std::vector<ASTNode *> & getStatements();
    void setStatements(std::vector<ASTNode *> _statements);
    void addStatement(ASTNode * _statement);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    bool isStatement() const;
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~With();
    //
};

/* ============================================================================
 *
 *                              TemplateDefineList
 *  This node is used to define the meta-programming parameters that facilitate
 *  templates for types and procedures in bJou.
 *  '!([TemplateDefineList]) ...'
 *
 * ===========================================================================*/

struct TemplateDefineList : ASTNode {
    TemplateDefineList();

    std::vector<ASTNode *> elements;

    std::vector<ASTNode *> & getElements();
    void setElements(std::vector<ASTNode *> _elements);
    void addElement(ASTNode * _element);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~TemplateDefineList();
    //
};

struct TemplateDefineElement : ASTNode {
    TemplateDefineElement();

    std::string name;

    enum eBitFlags E_BIT_FLAGS();

    std::string & getName();
    void setName(std::string _name);
};

/* ============================================================================
 *
 *                        TemplateDefineTypeDescriptor
 *  Node for information about what kind of type can qualify for a template
 *  instantiaion. Most of the time, it is just a placeholder name, but there can
 *  be constraints. For example, '!(T from Base)' will only accept types
 *  of or derived from Base as template instantiators. You can also have
 *  multiple bounds on types like '!(T from int | float)'. See docs for
 *  more info.
 *
 * ===========================================================================*/

struct TemplateDefineTypeDescriptor : TemplateDefineElement {
    TemplateDefineTypeDescriptor();

    std::vector<ASTNode *> bounds;

    std::vector<ASTNode *> & getBounds();
    void setBounds(std::vector<ASTNode *> _bounds);
    void addBound(ASTNode * _bound);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual ~TemplateDefineTypeDescriptor();
    //
};

/* ============================================================================
 *
 *                              TemplateDefineVariadicTypeArgs
 *  Template definitions can include a template for variable argument
 *  expansion in the form '!(...argsT)' where argsT can be used to
 *  represent one or more type arguments. See docs
 *
 * ===========================================================================*/

struct TemplateDefineVariadicTypeArgs : TemplateDefineElement {
    TemplateDefineVariadicTypeArgs();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual ~TemplateDefineVariadicTypeArgs();
    //
};

/* ============================================================================
 *
 *                              TemplateDefineExpression
 *  Template definitions can include a param that represents a consant
 *  expression in the form '<[expr]>' where exprT can be used in place of an
 *  expression. Type checking is done when instantiated. These cannot be
 *  aquired from any inferrence and MUST be specified in a verbose
 *  instantiation.
 *
 * ===========================================================================*/

struct TemplateDefineExpression : TemplateDefineElement {
    TemplateDefineExpression();

    ASTNode * varDecl;

    ASTNode * getVarDecl() const;
    void setVarDecl(ASTNode * _varDecl);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual ~TemplateDefineExpression();
    //
};

/* ============================================================================
 *
 *                              TemplateInstantiation
 *  Node containing Declarators or Expressions to satisfy a template
 *  definition and create the desired construct based on that definition.
 *
 * ===========================================================================*/

struct TemplateInstantiation : ASTNode {
    TemplateInstantiation();

    std::vector<ASTNode *> elements;

    enum eBitFlags E_BIT_FLAGS();

    std::vector<ASTNode *> & getElements();
    void setElements(std::vector<ASTNode *> _elements);
    void addElement(ASTNode * _element);

    // Node interface
    void unwrap(std::vector<ASTNode *> & terminals);
    ASTNode * clone();
    void desugar();
    virtual void analyze(bool force = false);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~TemplateInstantiation();
    //
};

/* ============================================================================
 *
 *                            TemplateAlias
 *  Definition of a template alias type
 *
 * ===========================================================================*/

struct TemplateAlias : ASTNode {
    TemplateAlias();

    ASTNode * _template;
    ASTNode * templateDef;

    enum eBitFlags E_BIT_FLAGS();

    ASTNode * getTemplate() const;
    void setTemplate(ASTNode * __template);
    ASTNode * getTemplateDef() const;
    void setTemplateDef(ASTNode * _templateDef);

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual ~TemplateAlias();
    //
};

/* ============================================================================
 *
 *                              TemplateStruct
 *  Definition of a template data type
 *
 * ===========================================================================*/

struct TemplateStruct : ASTNode {
    TemplateStruct();

    ASTNode * _template;
    ASTNode * templateDef;
    std::vector<ASTNode*> through_procs;

    enum eBitFlags E_BIT_FLAGS();

    ASTNode * getTemplate() const;
    void setTemplate(ASTNode * __template);
    ASTNode * getTemplateDef() const;
    void setTemplateDef(ASTNode * _templateDef);

    // Node interface
    const Type * getType();
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    void dump(std::ostream & stream, unsigned int level = 0,
              bool dumpCT = false);
    virtual ~TemplateStruct();
    //
};

/* ============================================================================
 *
 *                              TemplateProc
 *  Definition of a template procedure
 *
 * ===========================================================================*/

struct TemplateProc : ASTNode {
    TemplateProc();

    std::string lookupName;

    ASTNode * _template;
    ASTNode * templateDef;

    enum eBitFlags E_BIT_FLAGS_AND(IS_TYPE_MEMBER, FROM_THROUGH_TEMPLATE);

    std::string & getLookupName();
    void setLookupName(std::string _loopupName);
    ASTNode * getTemplate() const;
    void setTemplate(ASTNode * __template);
    ASTNode * getTemplateDef() const;
    void setTemplateDef(ASTNode * _templateDef);

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    void dump(std::ostream & stream, unsigned int level = 0,
              bool dumpCT = false);
    virtual ~TemplateProc();
    //
};

// ~~~~~ SLComment ~~~~~

struct SLComment : ASTNode {
    SLComment();

    std::string contents;

    enum eBitFlags E_BIT_FLAGS();

    std::string & getContents();
    void setContents(std::string _contents);

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~SLComment();
    //
};

// ~~~~~ ModuleDeclaration ~~~~~

struct ModuleDeclaration : ASTNode {
    ModuleDeclaration();

    std::string identifier;

    enum eBitFlags E_BIT_FLAGS();

    std::string & getIdentifier();
    void setIdentifier(std::string _identifier);

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~ModuleDeclaration();
    //
};

struct IgnoreNode : ASTNode {
    IgnoreNode();

    enum eBitFlags E_BIT_FLAGS();

    // Node interface
    virtual void analyze(bool force = false);
    ASTNode * clone();
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void * generate(BackEnd & backEnd, bool flag = false);
    void dump(std::ostream & stream, unsigned int level = 0,
              bool dumpCT = false);
    virtual ~IgnoreNode();
    //
};

// ~~~~~ MacroUse ~~~~~

struct MacroUse : ASTNode {
    MacroUse();

    std::string macroName;
    std::vector<ASTNode *> args;
    std::set<ASTNode *> leaveMeAloneArgs;

    const Type * result_t = nullptr;

    enum eBitFlags E_BIT_FLAGS();

    std::string & getMacroName();
    void setMacroName(std::string _macroName);

    std::vector<ASTNode *> & getArgs();
    void addArg(ASTNode * arg);

    // Node interface
    const Type * getType();
    virtual void analyze(bool force = false);
    ASTNode * clone();
    void unwrap(std::vector<ASTNode *> & terminals);
    virtual void addSymbols(std::string& _mod, Scope * _scope);
    virtual void dump(std::ostream & stream, unsigned int level = 0,
                      bool dumpCT = true);
    virtual ~MacroUse();
    //
};

} // namespace bjou

#endif /* ASTNode_hpp */
