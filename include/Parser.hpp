//
//  Parser.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/9/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Parser_hpp
#define Parser_hpp

#include "ASTNode.hpp"
#include "Context.hpp"
#include "Maybe.hpp"
#include "StringViewableBuffer.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace bjou {
typedef bjou::Maybe<std::string> MaybeString;
typedef bjou::Maybe<ASTNode *> MaybeASTNode;
typedef MaybeString (*TokenParserFnType)(StringViewableBuffer & buff);

MaybeString parse_kwd(StringViewableBuffer & buff, const char * kwd);
MaybeString parse_punc(StringViewableBuffer & buff, const char * punc);

MaybeString parser_generic_token(StringViewableBuffer & buff);
MaybeString parser_non_comment(StringViewableBuffer & buff);
MaybeString parser_identifier(StringViewableBuffer & buff);
MaybeString
parser_identifier_allow_primative_types(StringViewableBuffer & buff);
MaybeString parser_any_char(StringViewableBuffer & buff);
MaybeString parser_semicolon(StringViewableBuffer & buff);
MaybeString parser_under_score(StringViewableBuffer & buff);
MaybeString parser_comma(StringViewableBuffer & buff);
MaybeString parser_ellipsis(StringViewableBuffer & buff);
MaybeString parser_colon(StringViewableBuffer & buff);
MaybeString parser_dbl_colon(StringViewableBuffer & buff);
MaybeString parser_back_slash(StringViewableBuffer & buff);
MaybeString parser_dot_colon(StringViewableBuffer & buff);
MaybeString parser_vert(StringViewableBuffer & buff);
MaybeString parser_hash(StringViewableBuffer & buff);
MaybeString parser_thick_arrow(StringViewableBuffer & buff);
MaybeString parser_dollar(StringViewableBuffer & buff);
MaybeString parser_question(StringViewableBuffer & buff);
MaybeString parser_dbl_question(StringViewableBuffer & buff);
MaybeString parser_template_begin(StringViewableBuffer & buff);
MaybeString parser_kwd_const(StringViewableBuffer & buff);
MaybeString parser_kwd_type(StringViewableBuffer & buff);
MaybeString parser_kwd_abstract(StringViewableBuffer & buff);
MaybeString parser_kwd_extends(StringViewableBuffer & buff);
MaybeString parser_kwd_interface(StringViewableBuffer & buff);
MaybeString parser_kwd_implements(StringViewableBuffer & buff);
MaybeString parser_kwd_from(StringViewableBuffer & buff);
MaybeString parser_kwd_enum(StringViewableBuffer & buff);
MaybeString parser_kwd_print(StringViewableBuffer & buff);
MaybeString parser_kwd_raw(StringViewableBuffer & buff);
MaybeString parser_kwd_immut(StringViewableBuffer & buff);
MaybeString parser_kwd_coerce(StringViewableBuffer & buff);
MaybeString parser_kwd_this(StringViewableBuffer & buff);
MaybeString parser_kwd_ref(StringViewableBuffer & buff);
MaybeString parser_kwd_delete(StringViewableBuffer & buff);
MaybeString parser_kwd_namespace(StringViewableBuffer & buff);
MaybeString parser_kwd_import(StringViewableBuffer & buff);
MaybeString parser_kwd_module(StringViewableBuffer & buff);
MaybeString parser_kwd_alias(StringViewableBuffer & buff);
MaybeString parser_kwd_operator(StringViewableBuffer & buff);
MaybeString parser_kwd_return(StringViewableBuffer & buff);
MaybeString parser_kwd_if(StringViewableBuffer & buff);
MaybeString parser_kwd_else(StringViewableBuffer & buff);
MaybeString parser_kwd_while(StringViewableBuffer & buff);
MaybeString parser_kwd_do(StringViewableBuffer & buff);
MaybeString parser_kwd_for(StringViewableBuffer & buff);
MaybeString parser_kwd_foreach(StringViewableBuffer & buff);
MaybeString parser_kwd_in(StringViewableBuffer & buff);
MaybeString parser_kwd_match(StringViewableBuffer & buff);
MaybeString parser_kwd_with(StringViewableBuffer & buff);
MaybeString parser_kwd_break(StringViewableBuffer & buff);
MaybeString parser_kwd_continue(StringViewableBuffer & buff);
MaybeString parser_l_curly_brace(StringViewableBuffer & buff);
MaybeString parser_r_curly_brace(StringViewableBuffer & buff);
MaybeString parser_l_sqr_bracket(StringViewableBuffer & buff);
MaybeString parser_r_sqr_bracket(StringViewableBuffer & buff);
MaybeString parser_l_paren(StringViewableBuffer & buff);
MaybeString parser_r_paren(StringViewableBuffer & buff);
MaybeString parser_integer(StringViewableBuffer & buff);
MaybeString parser_floating_pt(StringViewableBuffer & buff);
MaybeString parser_char_literal(StringViewableBuffer & buff);
MaybeString parser_string_literal(StringViewableBuffer & buff);
MaybeString parser_kwd_true(StringViewableBuffer & buff);
MaybeString parser_kwd_false(StringViewableBuffer & buff);
MaybeString parser_kwd_nothing(StringViewableBuffer & buff);
MaybeString parser_kwd_as(StringViewableBuffer & buff);
MaybeString parser_dot(StringViewableBuffer & buff);
MaybeString parser_arrow(StringViewableBuffer & buff);
MaybeString parser_exclam(StringViewableBuffer & buff);
MaybeString parser_kwd_sizeof(StringViewableBuffer & buff);
MaybeString parser_amp(StringViewableBuffer & buff);
MaybeString parser_tilde(StringViewableBuffer & buff);
MaybeString parser_at(StringViewableBuffer & buff);
MaybeString parser_kwd_not(StringViewableBuffer & buff);
MaybeString parser_kwd_new(StringViewableBuffer & buff);
MaybeString parser_kwd_proc(StringViewableBuffer & buff);
MaybeString parser_kwd_extern(StringViewableBuffer & buff);
MaybeString parser_kwd_some(StringViewableBuffer & buff);
MaybeString parser_mult(StringViewableBuffer & buff);
MaybeString parser_div(StringViewableBuffer & buff);
MaybeString parser_mod(StringViewableBuffer & buff);
MaybeString parser_plus(StringViewableBuffer & buff);
MaybeString parser_minus(StringViewableBuffer & buff);
MaybeString parser_lss(StringViewableBuffer & buff);
MaybeString parser_leq(StringViewableBuffer & buff);
MaybeString parser_gtr(StringViewableBuffer & buff);
MaybeString parser_geq(StringViewableBuffer & buff);
MaybeString parser_equ(StringViewableBuffer & buff);
MaybeString parser_neq(StringViewableBuffer & buff);
MaybeString parser_and(StringViewableBuffer & buff);
MaybeString parser_kwd_and(StringViewableBuffer & buff);
MaybeString parser_or(StringViewableBuffer & buff);
MaybeString parser_kwd_or(StringViewableBuffer & buff);
MaybeString parser_assign(StringViewableBuffer & buff);
MaybeString parser_plus_eq(StringViewableBuffer & buff);
MaybeString parser_min_eq(StringViewableBuffer & buff);
MaybeString parser_mult_eq(StringViewableBuffer & buff);
MaybeString parser_div_eq(StringViewableBuffer & buff);
MaybeString parser_mod_eq(StringViewableBuffer & buff);
MaybeString parser_var_decl_beg(StringViewableBuffer & buff);
// MaybeString parser_constant_decl_beg(StringViewableBuffer& buff); // @const
MaybeString parser_sl_comment_beg(StringViewableBuffer & buff);
MaybeString parser_ml_comment_beg(StringViewableBuffer & buff);
MaybeString parser_ml_comment_end(StringViewableBuffer & buff);
MaybeString parser_end_of_line(StringViewableBuffer & buff);

enum TokenKind {
    GENERIC_TOKEN,
    NON_COMMENT,
    IDENTIFIER,
    IDENTIFIER_ALLOW_PRIMATIVE_TYPES,
    ANY_CHAR,
    SEMICOLON,
    UNDER_SCORE,
    COMMA,
    ELLIPSIS,
    COLON,
    DBL_COLON,
    BACK_SLASH,
    DOT_COLON,
    VERT,
    HASH,
    THICK_ARROW,
    DOLLAR,
    QUESTION,
    DBL_QUESTION,
    TEMPLATE_BEGIN,
    KWD_CONST,
    KWD_TYPE,
    KWD_ABSTRACT,
    KWD_EXTENDS,
    KWD_INTERFACE,
    KWD_IMPLEMENTS,
    KWD_FROM,
    KWD_ENUM,
    KWD_PRINT,
    KWD_RAW,
    KWD_IMMUT,
    KWD_COERCE,
    KWD_THIS,
    KWD_REF,
    KWD_DELETE,
    KWD_NAMESPACE,
    KWD_IMPORT,
    KWD_MODULE,
    KWD_ALIAS,
    KWD_OPERATOR,
    KWD_RETURN,
    KWD_IF,
    KWD_ELSE,
    KWD_WHILE,
    KWD_DO,
    KWD_FOR,
    KWD_FOREACH,
    KWD_IN,
    KWD_MATCH,
    KWD_WITH,
    KWD_BREAK,
    KWD_CONTINUE,
    L_CURLY_BRACE,
    R_CURLY_BRACE,
    L_SQR_BRACKET,
    R_SQR_BRACKET,
    L_PAREN,
    R_PAREN,
    INTEGER,
    FLOATING_PT,
    CHAR_LITERAL,
    STRING_LITERAL,
    KWD_TRUE,
    KWD_FALSE,
    KWD_NOTHING,
    KWD_AS,
    DOT,
    ARROW,
    EXCLAM,
    KWD_SIZEOF,
    AMP,
    TILDE,
    AT,
    KWD_NOT,
    KWD_NEW,
    KWD_PROC,
    KWD_EXTERN,
    KWD_SOME,
    MULT,
    DIV,
    MOD,
    PLUS,
    MINUS,
    LSS,
    LEQ,
    GTR,
    GEQ,
    EQU,
    NEQ,
    AND,
    KWD_AND,
    OR,
    KWD_OR,
    ASSIGN,
    PLUS_EQ,
    MIN_EQ,
    MULT_EQ,
    DIV_EQ,
    MOD_EQ,
    VAR_DECL_BEG, // CONSTANT_DECL_BEG, // @const
    SL_COMMENT_BEG,
    ML_COMMENT_BEG,
    ML_COMMENT_END,
    END_OF_LINE
};

bool is_kwd(std::string & s);
bool is_primative_typename(std::string & s);

struct Parser {
    bjou::StringViewableBuffer buff;
    Context currentContext;
    Context justCleanedContext;

    unsigned int n_lines = 0;

    Parser(const char * c_str, bool start = true);
    Parser(std::string & str, bool start = true);
    Parser(std::ifstream & file, const std::string & fname, bool start = true);

    virtual ~Parser();

    virtual void parseCommon();

    void prepare();

    void clean();
    void eat(std::string toEat, bool skipClean = false);

    void removeTrailingSpace();

    MaybeString text(std::string txt, bool skipEat = false,
                     bool skipClean = false);
    MaybeString optional(TokenKind tok, bool skipEat = false,
                         bool skipClean = false);
    std::string expect(TokenKind tok, std::string err, bool skipEat = false,
                       bool skipClean = false);

    template <typename... continuations>
    std::string expect(TokenKind tok, std::string err, bool skipEat,
                       bool skipClean, continuations... more) {
        auto m_match = optional(tok, skipEat, skipClean);
        std::string match;
        if (!m_match.assignTo(match))
            errornext(*this, "Expected " + err + ".", true, more...);
        return match;
    }

    MaybeASTNode parseDeclarator(bool only_base = false);
    ASTNode * newVoidDeclarator();
    MaybeASTNode parseTemplateDefineTypeDescriptor();
    MaybeASTNode parseTemplateDefineExpression();
    MaybeASTNode parseTemplateDefineVariadicTypeArgs();
    MaybeASTNode parseTemplateDef();
    MaybeASTNode parseTemplateInst();

    MaybeASTNode
    parseQualifiedIdentifier(bool allow_primative_typenames = false);

    MaybeASTNode parseExpression(int minPrecedence = 0);
    MaybeASTNode parseExpression_r(ASTNode * left, int minPrecedence);
    MaybeString parseBinaryOperator(bool skipEat = false);
    MaybeString parseUnaryPrefixOperator(bool skipEat = false);
    MaybeString parseUnaryPostfixOperator(bool skipEat = false);
    ASTNode * applyPostfixOp(std::string & postfixOp, Expression * expr,
                             Expression * left, Expression * right,
                             MaybeString & m_binOp, int minPrecedence);
    MaybeASTNode parseTerminatingExpression();
    MaybeASTNode parseParentheticalExpressionOrTuple();
    MaybeASTNode parseInitializerList();
    MaybeASTNode parseSliceOrDynamicArrayExpression();
    MaybeASTNode parseLenExpression();
    MaybeASTNode parseOperand();
    MaybeASTNode parseArgList();

    virtual MaybeASTNode parseTopLevelNode();
    MaybeASTNode parseImport();
    MaybeASTNode parseModuleDeclaration();
    MaybeASTNode parseNamespace();
    MaybeASTNode parseType();
    MaybeASTNode parseInterfaceDef();
    MaybeASTNode parseInterfaceImpl();
    MaybeASTNode parseEnum();
    MaybeASTNode parseAlias();
    MaybeASTNode parseConstant();
    MaybeASTNode parseVariableDeclaration();
    MaybeASTNode parseThis();
    MaybeASTNode parseProc(bool parse_body = true);
    MaybeASTNode parseExternSig();

    MaybeASTNode parseStatement();
    MaybeASTNode parseIf();
    MaybeASTNode parseElse();
    MaybeASTNode parseFor();
    MaybeASTNode parseForeach();
    MaybeASTNode parseWhile();
    MaybeASTNode parseDoWhile();
    MaybeASTNode parseMatch();
    MaybeASTNode parseWith();
    MaybeASTNode parsePrint();
    MaybeASTNode parseReturn();
    MaybeASTNode parseBreak();
    MaybeASTNode parseContinue();

    MaybeASTNode parseMacroUse();
    MaybeASTNode parseMacroArg();

    MaybeASTNode parseSLComment();

    void moduleCheck(std::vector<ASTNode *> & AST,
                     ModuleDeclaration * module_declared,
                     ModuleDeclaration * mod_decl);
};

struct AsyncParser : Parser {
    AsyncParser(const char * c_str);
    AsyncParser(std::string & str);
    AsyncParser(std::ifstream & file, const std::string & fname);

    std::vector<ASTNode *> nodes;
    std::vector<ASTNode *> structs, ifaceDefs;

    void parseCommon();
    MaybeASTNode parseTopLevelNode();
    milliseconds operator()();
};

struct ImportParser : Parser {
    ImportParser(const char * c_str);
    ImportParser(std::string & str);
    ImportParser(std::ifstream & file, const std::string & fname);

    Import * source = nullptr;

    std::vector<ASTNode *> nodes;
    std::vector<ASTNode *> structs, ifaceDefs;
    ModuleDeclaration * mod_decl;

    void parseCommon();
    MaybeASTNode parseTopLevelNode();
    void Dispose();
    milliseconds operator()();
};
} // namespace bjou

#endif /* Parser_hpp */
