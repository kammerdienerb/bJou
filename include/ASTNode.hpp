//
//  ASTNode.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/4/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef ASTNode_hpp
#define ASTNode_hpp

#include "Context.hpp"
#include "Range.hpp"
#include "Type.hpp"
#include "Evaluate.hpp"
#include "Misc.hpp"
#include "CLI.hpp"
#include "Maybe.hpp"
#include "ReplacementPolicy.hpp"

#include <vector>
#include <unordered_map>
#include <map>

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
    
// @refactor: with the addition of NodeKind, some of E_BIT_FLAGS are redundant and should be removed/replaced
    
#define E_BIT_FLAGS() { ANALYZED, IS_TEMPLATE, SYMBOL_OVERWRITE, HAS_TOP_LEVEL_RETURN, IGNORE_GEN }
#define E_BIT_FLAGS_AND(...) { ANALYZED, IS_TEMPLATE, SYMBOL_OVERWRITE, HAS_TOP_LEVEL_RETURN, IGNORE_GEN, __VA_ARGS__ }
    
    
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
            SUBSCRIPT_EXPRESSION,
            CALL_EXPRESSION,
            ACCESS_EXPRESSION,
            INJECT_EXPRESSION,
            UNARY_PRE_EXPRESSION,
            NOT_EXPRESSION,
            DEREF_EXPRESSION,
            ADDRESS_EXPRESSION,
            NEW_EXPRESSION,
            DELETE_EXPRESSION,
            SIZEOF_EXPRESSION,
            UNARY_POST_EXPRESSION,
            AS_EXPRESSION,
            IDENTIFIER,
            INITIALZER_LIST,
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
            _END_EXPRESSIONS,
            DECLARATOR,
            ARRAY_DECLARATOR,
            DYNAMIC_ARRAY_DECLARATOR,
            POINTER_DECLARATOR,
            MAYBE_DECLARATOR,
            TUPLE_DECLARATOR,
            PROCEDURE_DECLARATOR,
            PLACEHOLDER_DECLARATOR,
            _END_DECLARATORS,
            CONSTANT,
            VARIABLE_DECLARATION,
            ALIAS,
            STRUCT,
            INTERFACE_DEF,
            INTERFACE_IMPLEMENTATION,
            ENUM,
            ARG_LIST,
            THIS,
            PROCEDURE,
            NAMESPACE,
            IMPORT,
            PRINT,
            RETURN,
            BREAK,
            CONTINUE,
            IF,
            ELSE,
            FOR,
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
            MODULE_DECL
        };
        
        
#define IS_DECLARATOR(node) ((node)->nodeKind >= ASTNode::DECLARATOR && (node)->nodeKind < ASTNode::_END_DECLARATORS)
#define IS_EXPRESSION(node) ((node)->nodeKind >= ASTNode::EXPRESSION && (node)->nodeKind < ASTNode::_END_EXPRESSIONS)
        
        
        NodeKind nodeKind;
        
        int flags;
        Context context;
        Context nameContext;
        Scope * scope;
        ASTNode * parent;
        replacementPolicy * replace;
        
        enum eBitFlags E_BIT_FLAGS();
        
        int getFlags() const;
        void setFlags(int _flags);
        int getFlag(int flag) const;
        void setFlag(int flag, bool _val);
        void printFlags();
        
        Context& getContext();
        void setContext(Context _context);
        
        Context& getNameContext();
        void setNameContext(Context _context);
        
        Scope * getScope() const;
        void setScope(Scope * _scope);
        
        // Node interface
        virtual void analyze(bool force = false) = 0;
        virtual void addSymbols(Scope * _scope) = 0;
		virtual void unwrap(std::vector<ASTNode*>& terminals);
        virtual ASTNode * clone() = 0;
		virtual const Type * getType();
        virtual bool isStatement() const;
        // virtual void serializePass1(Serializer& ser) = 0;
        // virtual void serializePass2(Serializer& ser) = 0;
        
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        
        virtual ~ASTNode() = 0;
        //
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
        
        enum eBitFlags E_BIT_FLAGS_AND(PAREN, TERMINAL, IDENT);
        
        std::string& getContents();
        void setContents(std::string _contents);
        void setType(const Type * _type);
        ASTNode * getLeft() const;
        void setLeft(ASTNode * _left);
        ASTNode * getRight() const;
        void setRight(ASTNode * _right);
        
        virtual bool isConstant() = 0;
        virtual Val eval();
        
        // Node interface
		virtual void unwrap(std::vector<ASTNode*>& terminals);
        virtual ASTNode * clone() = 0;
        virtual void analyze(bool force = false) = 0;
        virtual void addSymbols(Scope * _scope);

        virtual const Type * getType();
        virtual bool isStatement() const;
        
        virtual ~Expression();
        //
    };
    
    template <typename T>
    static inline T * ExpressionClone(T * expr) {
        T * c = new T(*expr);
        
        if (expr->getLeft())
            c->setLeft(expr->getLeft()->clone());
        if (expr->getRight())
            c->setRight(expr->getRight()->clone());
        
        return c;
    }
    
    
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        // virtual ~SubExpression();
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        
        // Node interface
        virtual void analyze(bool force = false);
        virtual ASTNode * clone();
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        
        // Node interface
        virtual void analyze(bool force = false);
        virtual ASTNode * clone();
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        
        // Node interface
        virtual void analyze(bool force = false);
        virtual ASTNode * clone();
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        
        // Node interface
        virtual void analyze(bool force = false);
        virtual ASTNode * clone();
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        // virtual ~LogOrExpression();
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
        
        enum eBitFlags E_BIT_FLAGS_AND(PAREN, TERMINAL, IDENT, INTERFACE_CALL);
        
        Procedure * resolved_proc;
        
        bool isConstant();
        
        // Node interface
		        virtual void analyze(bool force = false);
        virtual ASTNode * clone();
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
       
        enum eBitFlags E_BIT_FLAGS_AND(PAREN, TERMINAL, IDENT, UFC);

        CallExpression * injection;
        
        CallExpression * nextCall();
        int handleInterfaceSpecificCall();
        int handleThroughTemplate();
        int handleAccessThroughDeclarator(bool force);
        int handleContainerAccess();
        bool handleInjection();
        
        bool isConstant();
        
        // Node interface
		        virtual void analyze(bool force = false);
        virtual ASTNode * clone();
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        // virtual ~AccessExpression();
        //
    };
    
    
    /* ============================================================================
     *
     *                               InjectExpression
     *  Expression trees made from the binary call operator '->'.
     *
     * ===========================================================================*/
    
    
    struct InjectExpression : BinaryExpression {
        InjectExpression();
        
        bool isConstant();
        
        // Node interface
        virtual void analyze(bool force = false);
        virtual ASTNode * clone();
        // virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        
        // Node interface
        virtual void analyze(bool force = false);
        ASTNode * clone();
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        
        // Node interface
        virtual void analyze(bool force = false);
        ASTNode * clone();
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        // virtual ~NotExpression();
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        // virtual ~AddressExpression();
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
        
        // Node interface
		        virtual void analyze(bool force = false);
        ASTNode * clone();
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        // virtual ~AsExpression();
        //
    };
    
    
    
    /* ============================================================================
     *
     *                              Identifier
     *  Extension of Expression. Fields, flags, access, etc. MUST start
     *  with the same items as Expression (in the same order). Represents 
     *  identifiers that can be qualified with namespaces. Used in symbol lookup.
     *
     * ===========================================================================*/
    
    struct Identifier : Expression {
        Identifier();
        
        std::string unqualified;
        std::string qualified;
        std::vector<std::string> namespaces;
        enum eBitFlags E_BIT_FLAGS_AND(PAREN, TERMINAL, IDENT, PREVIOUSLY_QUALIFIED, DIRECT_PROC_REF);
        
        std::string& getUnqualified();
        void setUnqualified(std::string _unqualified);
        std::vector<std::string>& getNamespaces();
        void setNamespaces(std::vector<std::string> _namespaces);
        void addNamespace(std::string _nspace);
        
        bool isConstant();
        Val eval();

        // Node interface
		        virtual void analyze(bool force = false);
        ASTNode * clone();
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        
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
        std::vector<ASTNode*> expressions;
        
        ASTNode * getObjDeclarator() const;
        void setObjDeclarator(ASTNode * _decl);
        std::vector<std::string>& getMemberNames();
        void setMemberNames(std::vector<std::string> _memberNamess);
        void addMemberName(std::string memberName);
        std::vector<ASTNode*>& getExpressions();
        void setExpressions(std::vector<ASTNode*> _expressions);
        void addExpression(ASTNode * expression);
        
        bool isConstant();
        
        // Node interface
        void unwrap(std::vector<ASTNode*>& terminals);
		        virtual void analyze(bool force = false);
        ASTNode * clone();
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        virtual void addSymbols(Scope * _scope);
        virtual ~InitializerList();
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        
        // Node interface
        virtual void analyze(bool force = false);
        ASTNode * clone();
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void addSymbols(Scope * _scope);
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        // virtual void addSymbols(Scope * _scope);
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        
        std::vector<ASTNode*> subExpressions;
        
        std::vector<ASTNode*>& getSubExpressions();
        void setSubExpressions(std::vector<ASTNode*> _subExpressions);
        void addSubExpression(ASTNode * _subExpression);
        
        bool isConstant();
        
        // Node interface
        virtual void addSymbols(Scope * _scope);
        virtual void analyze(bool force = false);
        ASTNode * clone();
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        // virtual ~NothingLiteral();
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
        
        enum eBitFlags E_BIT_FLAGS_AND(IMPLIES_COMPLETE);
        
        ASTNode * getIdentifier() const;
        void setIdentifier(ASTNode * _identifier);
        ASTNode * getTemplateInst() const;
        void setTemplateInst(ASTNode * _templateInst);
        std::vector<std::string>& getTypeSpecifiers();
        void setTypeSpecifiers(std::vector<std::string> _specifiers);
        void addTypeSpecifier(std::string specifier);
        
        // Node interface
		void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual const Type * getType();
		virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        virtual ~Declarator();
        //
        
        // Declarator interface
        virtual std::string mangleSymbol();
        virtual std::string mangleAndPrefixSymbol();
        virtual const ASTNode * getBase() const;
        //
    };
    
    
    template <typename T>
    static inline T * DeclaratorClone(T * decl) {
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
		void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        void addSymbols(Scope * _scope);
        virtual void analyze(bool force = false);
        virtual const Type * getType();
		virtual ~ArrayDeclarator();
        //
        
        // Declarator interface
        virtual std::string mangleSymbol();
        virtual std::string mangleAndPrefixSymbol();
        virtual const ASTNode * getBase() const;
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
        void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        void addSymbols(Scope * _scope);
        virtual void analyze(bool force = false);
        virtual const Type * getType();
        virtual ~DynamicArrayDeclarator();
        //
        
        // Declarator interface
        virtual std::string mangleSymbol();
        virtual std::string mangleAndPrefixSymbol();
        virtual const ASTNode * getBase() const;
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
        void addSymbols(Scope * _scope);
		void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual const Type * getType();
        virtual ~PointerDeclarator();
        //
        
        // Declarator interface
        virtual std::string mangleSymbol();
        virtual std::string mangleAndPrefixSymbol();
        virtual const ASTNode * getBase() const;
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
        void addSymbols(Scope * _scope);
		void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual const Type * getType();
        virtual ~MaybeDeclarator();
        //
        
        // Declarator interface
        virtual std::string mangleSymbol();
        virtual std::string mangleAndPrefixSymbol();
        virtual const ASTNode * getBase() const;
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
        
        std::vector<ASTNode*> subDeclarators;
        
        std::vector<ASTNode*>&  getSubDeclarators();
        void setSubDeclarators(std::vector<ASTNode*> _subDeclarators);
        void addSubDeclarator(ASTNode * _subDeclarator);
        
        // Node interface
        void addSymbols(Scope * _scope);
		void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual const Type * getType();
        virtual ~TupleDeclarator();
        //
        
        // Declarator interface
        virtual std::string mangleSymbol();
        virtual std::string mangleAndPrefixSymbol();
        virtual const ASTNode * getBase() const;
        //
    };
    
    
    
    /* ============================================================================
     *
     *                              ProcedureDeclarator
     *  Describes a procedure type. In the bJou language, procedures are
     *  'first-class citizens'. A declarator for a procedure taking no arguments and
     *  returning nothing is simply '<()>'. A declarator describing a procedure taking
     *  two ints and returning an int looks like '<(int, int) -> int>'. A procedure
     *  that has the same signature as C's printf is '<(immut char*, ...) -> int>'. This
     *  data structure describes any of these procedure types.
     *
     * ===========================================================================*/
    
    
    
    struct ProcedureDeclarator : Declarator {
        ProcedureDeclarator();
        
        std::vector<ASTNode*> paramDeclarators;
        ASTNode * retDeclarator;
        
        enum eBitFlags E_BIT_FLAGS_AND(IMPLIES_COMPLETE, IS_VARARG);
        
        std::vector<ASTNode*>&  getParamDeclarators();
        void setParamDeclarators(std::vector<ASTNode*> _paramDeclarators);
        void addParamDeclarator(ASTNode * paramDeclarator);
        ASTNode * getRetDeclarator() const;
        void setRetDeclarator(ASTNode * _retDeclarator);
        
        // Node interface
        void addSymbols(Scope * _scope);
		void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual const Type * getType();
        virtual ~ProcedureDeclarator();
        //
        
        // Declarator interface
        virtual std::string mangleSymbol();
        virtual std::string mangleAndPrefixSymbol();
        virtual const ASTNode * getBase() const;
        //
    };
    
    
    
    /* ============================================================================
     *
     *                              PlaceholderDeclarator
     *  Describes a temporarily unknown type. Used in InterfaceDefs.
     *
     * ===========================================================================*/
    
    
    
    struct PlaceholderDeclarator : Declarator {
        PlaceholderDeclarator();
        
        // Node interface
        void addSymbols(Scope * _scope);
        void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual const Type * getType();
        virtual ~PlaceholderDeclarator();
        //
        
        // Declarator interface
        virtual std::string mangleSymbol();
        virtual std::string mangleAndPrefixSymbol();
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
        std::string mangledName;
        ASTNode * typeDeclarator;
        ASTNode * initialization;

        enum eBitFlags E_BIT_FLAGS_AND(C_BUILTIN_MACRO, IS_TYPE_MEMBER);
        
        std::string& getName();
        void setName(std::string _name);
        std::string& getMangledName();
        void setMangledName(std::string _mangledName);
        ASTNode * getTypeDeclarator() const;
        void setTypeDeclarator(ASTNode * _typeDeclarator);
        ASTNode * getInitialization() const;
        void setInitialization(ASTNode * _initialization);
        
        // Node interface
		void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        virtual bool isStatement() const;
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
        std::string mangledName;
        ASTNode * typeDeclarator;
        ASTNode * initialization;
        
        enum eBitFlags E_BIT_FLAGS_AND(IS_EXTERN, IS_TYPE_MEMBER, IS_PROC_PARAM);
        
        std::string& getName();
        void setName(std::string _name);
        std::string& getMangledName();
        void setMangledName(std::string _mangledName);
        ASTNode * getTypeDeclarator() const;
        void setTypeDeclarator(ASTNode * _typeDeclarator);
        ASTNode * getInitialization() const;
        void setInitialization(ASTNode * _initialization);
        
        // Node interface
		const Type * getType();
		bool isStatement() const;
		void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        std::string mangledName;
        ASTNode * declarator;
        
        enum eBitFlags E_BIT_FLAGS();
        
        std::string& getName();
        void setName(std::string _name);
        std::string& getMangledName();
        void setMangledName(std::string _mangledName);
        ASTNode * getDeclarator() const;
        void setDeclarator(ASTNode * _declarator);
        
        // Node interface
		const Type * getType();
		void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
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
        std::string mangledName;
        ASTNode * extends;
        std::vector<ASTNode*> memberVarDecls;
        std::vector<ASTNode*> constantDecls;
        std::vector<ASTNode*> memberProcs;
        std::vector<ASTNode*> memberTemplateProcs;
        std::vector<ASTNode*> interfaceImpls;
        
        ASTNode * inst; // not owned!
        
        unsigned int n_interface_procs;
        
        enum eBitFlags E_BIT_FLAGS_AND(IS_ABSTRACT, IS_C_UNION);
        
        std::string& getName();
        void setName(std::string _name);
        std::string& getMangledName();
        void setMangledName(std::string _mangledName);
        ASTNode * getExtends() const;
        void setExtends(ASTNode * _extends);
        std::vector<ASTNode*>& getMemberVarDecls();
        void setMemberVarDecls(std::vector<ASTNode*> _memberVarDecls);
        void addMemberVarDecl(ASTNode * memberVarDecl);
        std::vector<ASTNode*>& getConstantDecls();
        void setConstantDecls(std::vector<ASTNode*> _constantDecls);
        void addConstantDecl(ASTNode * constantDecl);
        std::vector<ASTNode*>& getMemberProcs();
        void setMemberProcs(std::vector<ASTNode*> _memberProcs);
        void addMemberProc(ASTNode * memberProc);
        std::vector<ASTNode*>& getMemberTemplateProcs();
        void setMemberTemplateProcs(std::vector<ASTNode*> _memberTemplateProcs);
        void addMemberTemplateProc(ASTNode * memberTemplateProc);
        std::vector<ASTNode*>& getInterfaceImpls();
        void setInterfaceImpls(std::vector<ASTNode*> _interfaceImpls);
        void addInterfaceImpl(ASTNode * _interfaceImpl);
        
        std::vector<ASTNode*> getAllInterfaceImplsSorted();
        
        void preDeclare(Scope * _scope);
        
        // Node interface
		const Type * getType();
		void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        virtual ~Struct();
        //
    };
    
    
    
    /* ============================================================================
     *
     *                              InterfaceDef
     *  Represents the definition of an interface which types may implement.
     *
     * ===========================================================================*/
    
    struct InterfaceDef : ASTNode {
        InterfaceDef();
        
        std::string name;
        std::string mangledName;
        std::map<std::string, std::vector<ASTNode*> > procs;
        
        enum eBitFlags E_BIT_FLAGS();
        
        std::string& getName();
        void setName(std::string _name);
        std::string& getMangledName();
        void setMangledName(std::string _mangledName);
        std::map<std::string, std::vector<ASTNode*> >& getProcs();
        void setProcs(std::map<std::string, std::vector<ASTNode*> > _procs);
        void addProcs(std::string key, std::vector<ASTNode*> vals);
        void addProc(std::string key, ASTNode * val);
        
        unsigned int totalProcs();
        
        // Node interface
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        virtual ~InterfaceDef();
        //
    };
    
    
    
    
    
    /* ============================================================================
     *
     *                              InterfaceImplementation
     *  Represents an implementation of an interface within a type definition.
     *
     * ===========================================================================*/
    
    
    struct InterfaceImplementation : ASTNode {
        InterfaceImplementation();
        
        ASTNode * identifier;
        std::map<std::string, std::vector<ASTNode*> > procs;
        
        enum eBitFlags E_BIT_FLAGS_AND(PUNT_TO_EXTENSION);
        
        ASTNode * getIdentifier() const;
        void setIdentifier(ASTNode * _identifier);
        std::map<std::string, std::vector<ASTNode*> >& getProcs();
        void setProcs(std::map<std::string, std::vector<ASTNode*> > _procs);
        void addProcs(std::string key, std::vector<ASTNode*> vals);
        void addProc(std::string key, ASTNode * val);
        
        InterfaceDef * getInterfaceDef();
        
        // Node interface
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        virtual ~InterfaceImplementation();
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
        std::string mangledName;
        std::vector<std::string> identifiers;
        
        enum eBitFlags E_BIT_FLAGS();
        
        std::string& getName();
        void setName(std::string _name);
        std::string& getMangledName();
        void setMangledName(std::string _mangledName);
        std::vector<std::string>& getIdentifiers();
        void setIdentifiers(std::vector<std::string> _identifiers);
        void addIdentifier(std::string _identifier);
        
        // Node interface
		const Type * getType();
        virtual void analyze(bool force = false);
        ASTNode * clone();
        virtual void addSymbols(Scope * _scope);
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
        
        std::vector<ASTNode*> expressions;
        
        enum eBitFlags E_BIT_FLAGS();
        
        std::vector<ASTNode*>& getExpressions();
        void setExpressions(std::vector<ASTNode*> _expressions);
        void addExpression(ASTNode * _expression);
		void addExpressionToFront(ASTNode * _expression);
        
        // Node interface
        virtual void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
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
        virtual void addSymbols(Scope * _scope);
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
        std::string mangledName;
        std::vector<ASTNode*> paramVarDeclarations;
        ASTNode * retDeclarator;
        ASTNode * procDeclarator;
        std::vector<ASTNode*> statements;
        
        ASTNode * inst; // not owned!
        
        enum eBitFlags E_BIT_FLAGS_AND(IS_ANONYMOUS, IS_TEMPLATE_DERIVED, IS_EXTERN, IS_VARARG, IS_TYPE_MEMBER, IS_INTERFACE_DECL, IS_INTERFACE_IMPL);
        
        std::string& getName();
        void setName(std::string _name);
        std::string& getMangledName();
        void setMangledName(std::string _mangledName);
        std::vector<ASTNode*>& getParamVarDeclarations();
        void setParamVarDeclarations(std::vector<ASTNode*> _paramVarDeclarations);
        void addParamVarDeclaration(ASTNode * _paramVarDeclaration);
        ASTNode * getRetDeclarator() const;
        void setRetDeclarator(ASTNode * _retDeclarator);
        ASTNode * getProcDeclarator() const;
        void setProcDeclarator(ASTNode * _procDeclarator);
        std::vector<ASTNode*>& getStatements();
        void setStatements(std::vector<ASTNode*> _statements);
        void addStatement(ASTNode * _statement);
        
        Struct * getParentStruct();
        void desugarThis();
        
        // Node interface
		const Type * getType();
		void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        
        virtual ~Procedure();
        //
    };
    
    
    
    /* ============================================================================
     *
     *                              Namespace
     *  The Namespace node represents a grouping of Symbols under a common name.
     *
     * ===========================================================================*/
    
    
    struct Namespace : ASTNode {
        Namespace();
        
        std::string name;
        std::vector<ASTNode*> nodes;
        
        Scope * thisNamespaceScope;
        
        enum eBitFlags E_BIT_FLAGS();
        
        std::string& getName();
        void setName(std::string _name);
        std::vector<ASTNode*>& getNodes();
        void setNodes(std::vector<ASTNode*> _nodes);
        void addNode(ASTNode * _node);
        
        std::vector<ASTNode*> gather();
        
        void preDeclare(Scope * _scope);
        
        // Node interface
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        
        virtual ~Namespace();
        //
    };
    
    
    
    /* ============================================================================
     *
     *                              Import
     *  Import is the statement that directs the compiler to import a module and
     *  link to its binaries. These statements are only allowed at the global level.
     *
     * ===========================================================================*/
    
    struct Import : ASTNode {
        Import();
        
        std::string module;
        
        enum eBitFlags E_BIT_FLAGS_AND(FROM_PATH);
        
        std::string& getModule();
        void setModule(std::string _module);
        
        // Node interface
        virtual void analyze(bool force = false);
        ASTNode * clone();
        virtual void addSymbols(Scope * _scope);
        virtual ~Import();
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
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
				void unwrap(std::vector<ASTNode*>& terminals);
 		bool isStatement() const;
       ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        
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
virtual void addSymbols(Scope * _scope);
		virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        virtual void addSymbols(Scope * _scope);
		virtual void * generate(BackEnd& backEnd, bool flag = false);
        virtual ~Continue();
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
        std::vector<ASTNode*> statements;
        ASTNode * _else;
        
        enum eBitFlags E_BIT_FLAGS();
        
        ASTNode * getConditional() const;
        void setConditional(ASTNode * _conditional);
        std::vector<ASTNode*>& getStatements();
        void setStatements(std::vector<ASTNode*> _statements);
        void addStatement(ASTNode * _statement);
        ASTNode * getElse() const;
        void setElse(ASTNode * __else);
        
        // Node interface
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
		bool isStatement() const;
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        
        std::vector<ASTNode*> statements;
        
        enum eBitFlags E_BIT_FLAGS();
        
        std::vector<ASTNode*>& getStatements();
        void setStatements(std::vector<ASTNode*> _statements);
        void addStatement(ASTNode * _statement);
        
        // Node interface
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
		bool isStatement() const;
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
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
        
        std::vector<ASTNode*> initializations;
        ASTNode * conditional;
        std::vector<ASTNode*> afterthoughts;
        std::vector<ASTNode*> statements;
        
        enum eBitFlags E_BIT_FLAGS();
        
        std::vector<ASTNode*>& getInitializations();
        void setInitializations(std::vector<ASTNode*> _initializations);
        void addInitialization(ASTNode * _initialization);
        ASTNode * getConditional() const;
        void setConditional(ASTNode * _conditional);
        std::vector<ASTNode*>& getAfterthoughts();
        void setAfterthoughts(std::vector<ASTNode*> _afterthoughts);
        void addAfterthought(ASTNode * afterthought);
        std::vector<ASTNode*>& getStatements();
        void setStatements(std::vector<ASTNode*> _statements);
        void addStatement(ASTNode * _statement);
        
        // Node interface
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
		bool isStatement() const;
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        virtual void * generate(BackEnd& backEnd, bool flag = false);
        virtual ~For();
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
        std::vector<ASTNode*> statements;
        
        enum eBitFlags E_BIT_FLAGS();
        
        ASTNode * getConditional() const;
        void setConditional(ASTNode * _conditional);
        std::vector<ASTNode*>& getStatements();
        void setStatements(std::vector<ASTNode*> _statements);
        void addStatement(ASTNode * _statement);
        
        // Node interface
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
		bool isStatement() const;
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        virtual void * generate(BackEnd& backEnd, bool flag = false);
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
        std::vector<ASTNode*> statements;
        
        enum eBitFlags E_BIT_FLAGS();
        
        ASTNode * getConditional() const;
        void setConditional(ASTNode * _conditional);
        std::vector<ASTNode*>& getStatements();
        void setStatements(std::vector<ASTNode*> _statements);
        void addStatement(ASTNode * _statement);
        
        // Node interface
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
		bool isStatement() const;
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
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
        std::vector<ASTNode*> withs;
        
        enum eBitFlags E_BIT_FLAGS();
        
        ASTNode * getExpression() const;
        void setExpression(ASTNode * _expression);
        std::vector<ASTNode*>& getWiths();
        void setWiths(std::vector<ASTNode*> _withs);
        void addWith(ASTNode * _with);
        
        // Node interface
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
		bool isStatement() const;
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
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
        std::vector<ASTNode*> statements;
        
        enum eBitFlags E_BIT_FLAGS_AND(WITH_ELSE);
        
        ASTNode * getExpression() const;
        void setExpression(ASTNode * _expression);
        std::vector<ASTNode*>& getStatements();
        void setStatements(std::vector<ASTNode*> _statements);
        void addStatement(ASTNode * _statement);
        
        // Node interface
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
		bool isStatement() const;
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        virtual ~With();
        //
    };
    
    /* ============================================================================
     *
     *                              TemplateDefineList
     *  This node is used to define the meta-programming parameters that facilitate
     *  templates for types, procedures, and interfaces in bJou. 
     *  '!([TemplateDefineList]) ...'
     *
     * ===========================================================================*/
    
    struct TemplateDefineList : ASTNode {
        TemplateDefineList();
        
        std::vector<ASTNode*> elements;
        
        std::vector<ASTNode*>& getElements();
        void setElements(std::vector<ASTNode*> _elements);
        void addElement(ASTNode * _element);
        
        // Node interface
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
        virtual ~TemplateDefineList();
        //
    };
    
    struct TemplateDefineElement : ASTNode {
        TemplateDefineElement();
        
        std::string name;
        
        enum eBitFlags E_BIT_FLAGS();
        
        std::string& getName();
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

        std::vector<ASTNode*> bounds;
        
        std::vector<ASTNode*>& getBounds();
        void setBounds(std::vector<ASTNode*> _bounds);
        void addBound(ASTNode * _bound);
        
        // Node interface
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
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
        virtual void addSymbols(Scope * _scope);
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
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
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
        
        std::vector<ASTNode*> elements;
        
        enum eBitFlags E_BIT_FLAGS();
        
        std::vector<ASTNode*>& getElements();
        void setElements(std::vector<ASTNode*> _elements);
        void addElement(ASTNode * _element);
        
        // Node interface
				void unwrap(std::vector<ASTNode*>& terminals);
        ASTNode * clone();
        virtual void analyze(bool force = false);
        virtual void addSymbols(Scope * _scope);
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
        virtual void addSymbols(Scope * _scope);
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
        
        enum eBitFlags E_BIT_FLAGS();
        
        ASTNode * getTemplate() const;
        void setTemplate(ASTNode * __template);
        ASTNode * getTemplateDef() const;
        void setTemplateDef(ASTNode * _templateDef);
        
        // Node interface
        const Type * getType();
        virtual void analyze(bool force = false);
        ASTNode * clone();
        virtual void addSymbols(Scope * _scope);
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
        
        std::string mangledName;
        
        ASTNode * _template;
        ASTNode * templateDef;
        
        enum eBitFlags E_BIT_FLAGS_AND(IS_TYPE_MEMBER, FROM_THROUGH_TEMPLATE);
        
        std::string& getMangledName();
        void setMangledName(std::string _mangledName);
        ASTNode * getTemplate() const;
        void setTemplate(ASTNode * __template);
        ASTNode * getTemplateDef() const;
        void setTemplateDef(ASTNode * _templateDef);
        
        // Node interface
		        virtual void analyze(bool force = false);
        ASTNode * clone();
        virtual void addSymbols(Scope * _scope);
        virtual ~TemplateProc();
        //
    };
    
    
    // ~~~~~ SLComment ~~~~~
    
    
    struct SLComment : ASTNode {
        SLComment();
        
        std::string contents;
        
        enum eBitFlags E_BIT_FLAGS();
        
        std::string& getContents();
        void setContents(std::string _contents);
        
        // Node interface
        virtual void analyze(bool force = false);
        ASTNode * clone();
        virtual void addSymbols(Scope * _scope);
        virtual ~SLComment();
        //
    };
    
    
    // ~~~~~ ModuleDeclaration ~~~~~
    
    
    struct ModuleDeclaration : ASTNode {
        ModuleDeclaration();
        
        std::string identifier;
        
        enum eBitFlags E_BIT_FLAGS();
        
        std::string& getIdentifier();
        void setIdentifier(std::string _identifier);
        
        // Node interface
        virtual void analyze(bool force = false);
        ASTNode * clone();
        virtual void addSymbols(Scope * _scope);
        virtual ~ModuleDeclaration();
        //
    };
}

#endif /* ASTNode_hpp */






