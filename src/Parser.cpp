//
//  Parser.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/9/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "Parser.hpp"
#include "CLI.hpp"
#include "Compile.hpp"
#include "FrontEnd.hpp"
#include "Misc.hpp"
#include "Operator.hpp"

#include <iostream>
#include <sstream>

#include "rlutil.h"

namespace bjou {
// the order of the items in this table is crucial..
// it should be a direct mapping to the enum TokenKind
constexpr const TokenParserFnType tokenParsers[] = {
    parser_generic_token, parser_non_comment, parser_identifier,
    parser_identifier_allow_primative_types, parser_any_char, parser_semicolon,
    parser_under_score, parser_comma, parser_ellipsis, parser_colon,
    parser_dbl_colon, parser_back_slash, parser_dot_colon, parser_vert,
    parser_hash, parser_thick_arrow, parser_dollar, parser_question,
    parser_dbl_question, parser_template_begin, parser_kwd_const,
    parser_kwd_type, parser_kwd_abstract, parser_kwd_extends,
    parser_kwd_interface, parser_kwd_implements, parser_kwd_from,
    parser_kwd_enum, parser_kwd_print, parser_kwd_raw, parser_kwd_immut,
    parser_kwd_coerce, parser_kwd_this, parser_kwd_ref, parser_kwd_delete,
    parser_kwd_namespace, parser_kwd_import, parser_kwd_module,
    parser_kwd_alias, parser_kwd_operator, parser_kwd_return, parser_kwd_if,
    parser_kwd_else, parser_kwd_while, parser_kwd_do, parser_kwd_for,
    parser_kwd_foreach, parser_kwd_in, parser_kwd_match, parser_kwd_with,
    parser_kwd_break, parser_kwd_continue, parser_l_curly_brace,
    parser_r_curly_brace, parser_l_sqr_bracket, parser_r_sqr_bracket,
    parser_l_paren, parser_r_paren, parser_integer, parser_floating_pt,
    parser_char_literal, parser_string_literal, parser_kwd_true,
    parser_kwd_false, parser_kwd_nothing, parser_kwd_as, parser_dot,
    parser_arrow, parser_exclam, parser_kwd_sizeof, parser_amp, parser_tilde,
    parser_at, parser_kwd_not, parser_kwd_new, parser_kwd_proc,
    parser_kwd_extern, parser_kwd_some, parser_mult, parser_div, parser_mod,
    parser_plus, parser_minus, parser_lss, parser_leq, parser_gtr, parser_geq,
    parser_equ, parser_neq, parser_and, parser_kwd_and, parser_or,
    parser_kwd_or, parser_assign, parser_plus_eq, parser_min_eq, parser_mult_eq,
    parser_div_eq, parser_mod_eq, parser_var_decl_beg,
    // parser_constant_decl_beg, // @const
    parser_sl_comment_beg, parser_ml_comment_beg, parser_ml_comment_end,
    parser_end_of_line};

#define GOOD_IDX(buff, p) ((p) < (buff).viewSize())
#define IS_C(buff, p, c) ((p) < (buff).viewSize() && (buff)[(p)] == (c))
#define IN_CHAR_RANGE(buff, p, first, last)                                    \
    ((p) < (buff).viewSize() &&                                                \
     ((buff)[(p)] >= (first) && (buff)[(p)] <= (last)))
#define IS_AZ(buff, p)                                                         \
    ((p) < (buff).viewSize() && (((buff)[(p)]) >= 'A' && ((buff)[(p)]) <= 'Z'))
#define IS_az(buff, p)                                                         \
    ((p) < (buff).viewSize() && (((buff)[(p)]) >= 'a' && ((buff)[(p)]) <= 'z'))
#define IS_aZ(buff, p)                                                         \
    ((p) < (buff).viewSize() &&                                                \
     ((((buff)[(p)]) >= 'a' && ((buff)[(p)]) <= 'z') ||                        \
      (((buff)[(p)]) >= 'A' && ((buff)[(p)]) <= 'Z')))
#define IS_09(buff, p)                                                         \
    ((p) < (buff).viewSize() && (((buff)[(p)]) >= '0' && ((buff)[(p)]) <= '9'))
#define IS_aZ09(buff, p)                                                       \
    ((p) < (buff).viewSize() &&                                                \
     (((((buff)[(p)]) >= 'a' && ((buff)[(p)]) <= 'z') ||                       \
       (((buff)[(p)]) >= 'A' && ((buff)[(p)]) <= 'Z')) ||                      \
      (((buff)[(p)]) >= '0' && ((buff)[(p)]) <= '9')))
#define IS_SPACE(buff, p) ((p) < (buff).viewSize() && (isspace((buff)[(p)])))

const char * kwds[] = {
    "type",    "abstract", "interface", "implements", "extends", "from",
    "enum",    "print",    "raw",       "immut",      "delete",  "namespace",
    "import",  "alias",    "operator",  "coerce",     "this",    "ref",

    "return",  "if",       "else",      "while",      "do",      "for",
    "foreach", "in",       "match",     "with",       "break",   "continue",

    "true",    "false",    "nothing",

    "as",      "sizeof",   "not",       "new",        "proc",    "extern",
    "some",    "and",      "or"};

MaybeString parse_kwd(StringViewableBuffer & buff, const char * kwd) {
    char * c = (char *)kwd;
    size_t p = 0;
    while (*c) {
        if (!IS_C(buff, p, *c))
            return MaybeString();
        c++;
        p++;
    }
    if (IS_aZ09(buff, p) || IS_C(buff, p, '_'))
        return MaybeString();
    return MaybeString(kwd);
}

MaybeString parse_punc(StringViewableBuffer & buff, const char * punc) {
    char * c = (char *)punc;
    size_t p = 0;
    while (*c) {
        if (!IS_C(buff, p, *c))
            return MaybeString();
        c++;
        p++;
    }
    return MaybeString(punc);
}

MaybeString parser_generic_token(StringViewableBuffer & buff) {
    size_t p = 0;
    if (GOOD_IDX(buff, p) && !IS_aZ09(buff, p) && !IS_C(buff, p, '_') &&
        !IS_SPACE(buff, p))
        return MaybeString(buff.substr(0, 1));
    else
        while (GOOD_IDX(buff, p) &&
               (IS_aZ09(buff, p) || IS_C(buff, p, '_'))) //! IS_SPACE(buff, p))
            p += 1;
    return MaybeString(buff.substr(0, p));
}

MaybeString parser_non_comment(StringViewableBuffer & buff) {
    // do we still need this?
    return MaybeString();
}

MaybeString parser_identifier(StringViewableBuffer & buff) {
    bool has_non_underscore = false;
    size_t p = 0;
    if (!(IS_aZ(buff, p) || IS_C(buff, p, '_')))
        return MaybeString();
    has_non_underscore |= !IS_C(buff, p, '_');
    p += 1;
    while (IS_aZ09(buff, p) || IS_C(buff, p, '_')) {
        has_non_underscore |= !IS_C(buff, p, '_');
        p += 1;
    }
    while (IS_C(buff, p, '\''))
        p += 1;
    std::string s = buff.substr(0, p);
    if (is_kwd(s) || is_primative_typename(s) || !has_non_underscore)
        return MaybeString();
    return MaybeString(s);
}

MaybeString
parser_identifier_allow_primative_types(StringViewableBuffer & buff) {
    bool has_non_underscore = false;
    size_t p = 0;
    if (!(IS_aZ(buff, p) || IS_C(buff, p, '_')))
        return MaybeString();
    has_non_underscore |= !IS_C(buff, p, '_');
    p += 1;
    while (IS_aZ09(buff, p) || IS_C(buff, p, '_')) {
        has_non_underscore |= !IS_C(buff, p, '_');
        p += 1;
    }
    while (IS_C(buff, p, '\''))
        p += 1;
    std::string s = buff.substr(0, p);
    if (is_kwd(s) || !has_non_underscore)
        return MaybeString();
    return MaybeString(s);
}

MaybeString parser_any_char(StringViewableBuffer & buff) {
    if (!buff.viewSize())
        return MaybeString();
    return MaybeString(std::string(1, buff[0]));
}

MaybeString parser_semicolon(StringViewableBuffer & buff) {
    return parse_punc(buff, ";");
}
MaybeString parser_under_score(StringViewableBuffer & buff) {
    return parse_punc(buff, "_");
}
MaybeString parser_comma(StringViewableBuffer & buff) {
    return parse_punc(buff, ",");
}
MaybeString parser_ellipsis(StringViewableBuffer & buff) {
    return parse_punc(buff, "...");
}
MaybeString parser_colon(StringViewableBuffer & buff) {
    return parse_punc(buff, ":");
}
MaybeString parser_dbl_colon(StringViewableBuffer & buff) {
    return parse_punc(buff, "::");
}
MaybeString parser_back_slash(StringViewableBuffer & buff) {
    return parse_punc(buff, "\\");
}
MaybeString parser_vert(StringViewableBuffer & buff) {
    return parse_punc(buff, "|");
}
MaybeString parser_dot_colon(StringViewableBuffer & buff) {
    return parse_punc(buff, ".:");
}
MaybeString parser_hash(StringViewableBuffer & buff) {
    return parse_punc(buff, "#");
}
MaybeString parser_thick_arrow(StringViewableBuffer & buff) {
    return parse_punc(buff, "=>");
}
MaybeString parser_dollar(StringViewableBuffer & buff) {
    return parse_punc(buff, "$");
}
MaybeString parser_question(StringViewableBuffer & buff) {
    return parse_punc(buff, "?");
}
MaybeString parser_dbl_question(StringViewableBuffer & buff) {
    return parse_punc(buff, "??");
}
MaybeString parser_template_begin(StringViewableBuffer & buff) {
    size_t p = 0;
    if (!IS_C(buff, p, '!'))
        return MaybeString();
    p += 1;
    while (IS_SPACE(buff, p))
        p += 1;
    if (!IS_C(buff, p, '('))
        return MaybeString();
    p += 1;
    return MaybeString(buff.substr(0, p));
}
MaybeString parser_kwd_const(StringViewableBuffer & buff) {
    return parse_kwd(buff, "const");
}
MaybeString parser_kwd_type(StringViewableBuffer & buff) {
    return parse_kwd(buff, "type");
}
MaybeString parser_kwd_abstract(StringViewableBuffer & buff) {
    return parse_kwd(buff, "abstract");
}
MaybeString parser_kwd_extends(StringViewableBuffer & buff) {
    return parse_kwd(buff, "extends");
}
MaybeString parser_kwd_interface(StringViewableBuffer & buff) {
    return parse_kwd(buff, "interface");
}
MaybeString parser_kwd_implements(StringViewableBuffer & buff) {
    return parse_kwd(buff, "implements");
}
MaybeString parser_kwd_from(StringViewableBuffer & buff) {
    return parse_kwd(buff, "from");
}
MaybeString parser_kwd_enum(StringViewableBuffer & buff) {
    return parse_kwd(buff, "enum");
}
MaybeString parser_kwd_print(StringViewableBuffer & buff) {
    return parse_kwd(buff, "print");
}
MaybeString parser_kwd_raw(StringViewableBuffer & buff) {
    return parse_kwd(buff, "raw");
}
MaybeString parser_kwd_immut(StringViewableBuffer & buff) {
    return parse_kwd(buff, "immut");
}
MaybeString parser_kwd_coerce(StringViewableBuffer & buff) {
    return parse_kwd(buff, "coerce");
}
MaybeString parser_kwd_this(StringViewableBuffer & buff) {
    return parse_kwd(buff, "this");
}
MaybeString parser_kwd_ref(StringViewableBuffer & buff) {
    return parse_kwd(buff, "ref");
}
MaybeString parser_kwd_delete(StringViewableBuffer & buff) {
    return parse_kwd(buff, "delete");
}
MaybeString parser_kwd_namespace(StringViewableBuffer & buff) {
    return parse_kwd(buff, "namespace");
}
MaybeString parser_kwd_import(StringViewableBuffer & buff) {
    return parse_kwd(buff, "import");
}
MaybeString parser_kwd_module(StringViewableBuffer & buff) {
    return parse_kwd(buff, "module");
}
MaybeString parser_kwd_alias(StringViewableBuffer & buff) {
    return parse_kwd(buff, "alias");
}
MaybeString parser_kwd_operator(StringViewableBuffer & buff) {
    return parse_kwd(buff, "operator");
}
MaybeString parser_kwd_return(StringViewableBuffer & buff) {
    return parse_kwd(buff, "return");
}
MaybeString parser_kwd_if(StringViewableBuffer & buff) {
    return parse_kwd(buff, "if");
}
MaybeString parser_kwd_else(StringViewableBuffer & buff) {
    return parse_kwd(buff, "else");
}
MaybeString parser_kwd_while(StringViewableBuffer & buff) {
    return parse_kwd(buff, "while");
}
MaybeString parser_kwd_do(StringViewableBuffer & buff) {
    return parse_kwd(buff, "do");
}
MaybeString parser_kwd_for(StringViewableBuffer & buff) {
    return parse_kwd(buff, "for");
}
MaybeString parser_kwd_foreach(StringViewableBuffer & buff) {
    return parse_kwd(buff, "foreach");
}
MaybeString parser_kwd_in(StringViewableBuffer & buff) {
    return parse_kwd(buff, "in");
}
MaybeString parser_kwd_match(StringViewableBuffer & buff) {
    return parse_kwd(buff, "match");
}
MaybeString parser_kwd_with(StringViewableBuffer & buff) {
    return parse_kwd(buff, "with");
}
MaybeString parser_kwd_break(StringViewableBuffer & buff) {
    return parse_kwd(buff, "break");
}
MaybeString parser_kwd_continue(StringViewableBuffer & buff) {
    return parse_kwd(buff, "continue");
}
MaybeString parser_l_curly_brace(StringViewableBuffer & buff) {
    return parse_punc(buff, "{");
}
MaybeString parser_r_curly_brace(StringViewableBuffer & buff) {
    return parse_punc(buff, "}");
}
MaybeString parser_l_sqr_bracket(StringViewableBuffer & buff) {
    return parse_punc(buff, "[");
}
MaybeString parser_r_sqr_bracket(StringViewableBuffer & buff) {
    return parse_punc(buff, "]");
}
MaybeString parser_l_paren(StringViewableBuffer & buff) {
    return parse_punc(buff, "(");
}
MaybeString parser_r_paren(StringViewableBuffer & buff) {
    return parse_punc(buff, ")");
}
MaybeString parser_integer(StringViewableBuffer & buff) {
    size_t p = 0;
    if (IS_C(buff, p, '-')) {
        p += 1;
        if (!IS_09(buff, p))
            return MaybeString();
        p += 1;
    }
    while (IS_09(buff, p))
        p += 1;
    if (p == 0)
        return MaybeString();
    return MaybeString(buff.substr(0, p));
}
MaybeString parser_floating_pt(StringViewableBuffer & buff) {
    size_t p = 0;
    if (IS_C(buff, p, '-')) {
        p += 1;
        if (!IS_09(buff, p))
            return MaybeString();
        p += 1;
    }
    while (IS_09(buff, p))
        p += 1;
    if (!IS_C(buff, p, '.'))
        return MaybeString();
    p += 1;
    if (!IS_09(buff, p))
        return MaybeString();
    p += 1;
    while (IS_09(buff, p))
        p += 1;
    return MaybeString(buff.substr(0, p));
}
MaybeString parser_char_literal(StringViewableBuffer & buff) {
    size_t p = 0;
    if (!IS_C(buff, p, '\''))
        return MaybeString();
    p += 1;
    if (IS_C(buff, p, '\\'))
        p += 1;
    p += 1;
    if (!IS_C(buff, p, '\''))
        return MaybeString();
    p += 1;
    return MaybeString(buff.substr(0, p));
}
MaybeString parser_string_literal(StringViewableBuffer & buff) {
    size_t p = 0;
    if (!IS_C(buff, p, '\"'))
        return MaybeString();
    p += 1;
    while (true) {
        if (IS_C(buff, p, '\"')) {
            p += 1;
            break;
        } else if (IS_C(buff, p, '\\')) {
            p += 1;
        }
        if (p < buff.viewSize())
            p += 1;
        else
            return MaybeString();
    }
    return MaybeString(buff.substr(0, p));
}
MaybeString parser_kwd_true(StringViewableBuffer & buff) {
    return parse_kwd(buff, "true");
}
MaybeString parser_kwd_false(StringViewableBuffer & buff) {
    return parse_kwd(buff, "false");
}
MaybeString parser_kwd_nothing(StringViewableBuffer & buff) {
    return parse_kwd(buff, "nothing");
}
MaybeString parser_kwd_as(StringViewableBuffer & buff) {
    return parse_kwd(buff, "as");
}
MaybeString parser_dot(StringViewableBuffer & buff) {
    return parse_punc(buff, ".");
}
MaybeString parser_arrow(StringViewableBuffer & buff) {
    return parse_punc(buff, "->");
}
MaybeString parser_exclam(StringViewableBuffer & buff) {
    return parse_punc(buff, "!");
}
MaybeString parser_kwd_sizeof(StringViewableBuffer & buff) {
    return parse_kwd(buff, "sizeof");
}
MaybeString parser_amp(StringViewableBuffer & buff) {
    return parse_punc(buff, "&");
}
MaybeString parser_tilde(StringViewableBuffer & buff) {
    return parse_punc(buff, "~");
}
MaybeString parser_at(StringViewableBuffer & buff) {
    return parse_punc(buff, "@");
}
MaybeString parser_kwd_not(StringViewableBuffer & buff) {
    return parse_kwd(buff, "not");
}
MaybeString parser_kwd_new(StringViewableBuffer & buff) {
    return parse_kwd(buff, "new");
}
MaybeString parser_kwd_proc(StringViewableBuffer & buff) {
    return parse_kwd(buff, "proc");
}
MaybeString parser_kwd_extern(StringViewableBuffer & buff) {
    return parse_kwd(buff, "extern");
}
MaybeString parser_kwd_some(StringViewableBuffer & buff) {
    return parse_kwd(buff, "some");
}
MaybeString parser_mult(StringViewableBuffer & buff) {
    return parse_punc(buff, "*");
}
MaybeString parser_div(StringViewableBuffer & buff) {
    return parse_punc(buff, "/");
}
MaybeString parser_mod(StringViewableBuffer & buff) {
    return parse_punc(buff, "%");
}
MaybeString parser_plus(StringViewableBuffer & buff) {
    return parse_punc(buff, "+");
}
MaybeString parser_minus(StringViewableBuffer & buff) {
    return parse_punc(buff, "-");
}
MaybeString parser_lss(StringViewableBuffer & buff) {
    return parse_punc(buff, "<");
}
MaybeString parser_leq(StringViewableBuffer & buff) {
    return parse_punc(buff, "<=");
}
MaybeString parser_gtr(StringViewableBuffer & buff) {
    return parse_punc(buff, ">");
}
MaybeString parser_geq(StringViewableBuffer & buff) {
    return parse_punc(buff, ">=");
}
MaybeString parser_equ(StringViewableBuffer & buff) {
    return parse_punc(buff, "==");
}
MaybeString parser_neq(StringViewableBuffer & buff) {
    return parse_punc(buff, "!=");
}
MaybeString parser_and(StringViewableBuffer & buff) {
    return parse_punc(buff, "&&");
}
MaybeString parser_kwd_and(StringViewableBuffer & buff) {
    return parse_kwd(buff, "and");
}
MaybeString parser_or(StringViewableBuffer & buff) {
    return parse_punc(buff, "||");
}
MaybeString parser_kwd_or(StringViewableBuffer & buff) {
    return parse_kwd(buff, "or");
}
MaybeString parser_assign(StringViewableBuffer & buff) {
    return parse_punc(buff, "=");
}
MaybeString parser_plus_eq(StringViewableBuffer & buff) {
    return parse_punc(buff, "+=");
}
MaybeString parser_min_eq(StringViewableBuffer & buff) {
    return parse_punc(buff, "-=");
}
MaybeString parser_mult_eq(StringViewableBuffer & buff) {
    return parse_punc(buff, "*=");
}
MaybeString parser_div_eq(StringViewableBuffer & buff) {
    return parse_punc(buff, "/=");
}
MaybeString parser_mod_eq(StringViewableBuffer & buff) {
    return parse_punc(buff, "%=");
}
MaybeString parser_var_decl_beg(StringViewableBuffer & buff) {
    // if (tokenParsers[CONSTANT_DECL_BEG](buff)) // @const
    // return MaybeString();
    size_t p = 0;
    auto m_identifier = tokenParsers[IDENTIFIER](buff);
    std::string identifier;
    if (m_identifier.assignTo(identifier))
        p += identifier.size();
    else
        return MaybeString();
    while (IS_SPACE(buff, p))
        p += 1;
    if (!IS_C(buff, p, ':'))
        return MaybeString();
    p += 1;
    if (IS_C(buff, p, ':')) // watch out for namespaces
        return MaybeString();
    return MaybeString(buff.substr(0, p));
}
MaybeString parser_constant_decl_beg(StringViewableBuffer & buff) {
    size_t p = 0;
    auto m_identifier = tokenParsers[IDENTIFIER](buff);
    std::string identifier;
    if (m_identifier.assignTo(identifier))
        p += identifier.size();
    else
        return MaybeString();
    while (IS_SPACE(buff, p))
        p += 1;
    if (!IS_C(buff, p, ':'))
        return MaybeString();
    p += 1;
    if (!IS_C(buff, p, ':'))
        return MaybeString();
    p += 1;
    return MaybeString(buff.substr(0, p));
}
MaybeString parser_sl_comment_beg(StringViewableBuffer & buff) {
    return parse_punc(buff, "#");
}
MaybeString parser_ml_comment_beg(StringViewableBuffer & buff) {
    return MaybeString();
}
MaybeString parser_ml_comment_end(StringViewableBuffer & buff) {
    return MaybeString();
}
MaybeString parser_end_of_line(StringViewableBuffer & buff) {
    size_t p = 0;
    while (IS_C(buff, p, ' ') || IS_C(buff, p, '\t'))
        p += 1;
    if (!IS_C(buff, p, '\n'))
        return MaybeString();
    p += 1;
    return MaybeString(buff.substr(0, p));
}

bool is_kwd(std::string & s) {
    // exception for 'this' because it can be used in
    // other places as a regular identifier
    return s != "this" && s_in_a(s.c_str(), kwds);
}

bool is_primative_typename(std::string & s) {
    return (compilation->frontEnd.primativeTypeTable.count(s) > 0);
}

Parser::Parser(const char * c_str, bool start) : buff(c_str) {
    std::stringstream ss;
    ss << "<const char* " << (void *)c_str << ">";
    currentContext.filename = ss.str();
    currentContext.begin = {1, 1};
    currentContext.end = {1, 1};

    prepare();
    if (start)
        parseCommon();
}

Parser::Parser(std::string & str, bool start) : buff(str) {
    std::stringstream ss;
    ss << "<std::string " << (void *)&str << ">";
    currentContext.filename = ss.str();
    currentContext.begin = {1, 1};
    currentContext.end = {1, 1};

    prepare();
    if (start)
        parseCommon();
}

Parser::Parser(std::ifstream & file, const std::string & fname, bool start)
    : buff(file) {
    assert(buff.end() != NULL);

    currentContext.filename = fname;
    currentContext.begin = {1, 1};
    currentContext.end = {1, 1};

    prepare();
    if (start)
        parseCommon();
}

Parser::~Parser() {}

void Parser::parseCommon() {
    static ModuleDeclaration * module_declared = nullptr;

    while (buff.viewSize() > 0) {
        clean();
        MaybeASTNode m_node = parseTopLevelNode();
        ASTNode * node = nullptr;
        if (!m_node.assignTo(node))
            errornext(*this, "Unexpected token.");

        else if (node->nodeKind == ASTNode::MODULE_DECL) {
            moduleCheck(compilation->frontEnd.AST, module_declared,
                        (ModuleDeclaration *)node);
            module_declared = (ModuleDeclaration *)node;
        }

        node->replace = rpget<replacementPolicy_Global_Node>();

        // we are single-threaded..
        compilation->frontEnd.AST.push_back(node);
    }
    compilation->frontEnd.n_lines += n_lines;
}

void Parser::prepare() {
    removeTrailingSpace();
    clean();
}

void Parser::clean() {
    if (!buff.viewSize())
        return;
    // if we don't actually end up cleaning anything, we
    // don't want to clobber the old info in justCleanedContext
    // so we will save it and only commit to a change if
    // we do actually clean anything
    Context save_currentContext = currentContext;
    Context save_justCleanedContext = justCleanedContext;
    justCleanedContext.start(&currentContext);
    bool altered = false;
    while (IS_SPACE(buff, 0)) {
        if (IS_C(buff, 0, '\n')) {
            n_lines += 1;
            currentContext.end.line += 1;
            currentContext.end.character = 1;
        } else
            currentContext.end.character += 1;
        buff.advance(1);
        justCleanedContext.finish(&currentContext);
        altered = true;
    }
    if (!altered)
        justCleanedContext = save_justCleanedContext;
    save_justCleanedContext = justCleanedContext;
    MaybeASTNode m_comment = parseSLComment();
    ASTNode * comment;
    if (m_comment.assignTo(comment)) {
        clean();
        delete comment;
    }
    currentContext.begin = save_currentContext.begin;
    justCleanedContext.begin = save_justCleanedContext.begin;
}

void Parser::eat(std::string toEat, bool skipClean) {
    buff.advance(toEat.size());
    currentContext.end.character += toEat.size();
    currentContext.begin = currentContext.end;
    if (!skipClean)
        clean();
}

void Parser::removeTrailingSpace() {
    size_t p = 1;
    while (IS_SPACE(buff, buff.viewSize() - p))
        p += 1;
    buff.pullBack(p - 1);
}

MaybeString Parser::text(std::string txt, bool skipEat, bool skipClean) {
    auto m_match = parse_kwd(buff, txt.c_str());
    std::string match;
    if (m_match.assignTo(match)) {
        if (!skipEat)
            eat(match, skipClean);
        else if (!skipClean)
            clean();
    }
    return m_match;
}

bjou::Maybe<std::string> Parser::optional(TokenKind tok, bool skipEat,
                                          bool skipClean) {
    auto m_match = tokenParsers[tok](buff);
    std::string match;
    if (m_match.assignTo(match)) {
        if (!skipEat)
            eat(match, skipClean);
        else if (!skipClean)
            clean();
        if (tok == END_OF_LINE) {
            // newlines won't be taken by clean() if they are specifically
            // looked for but the currentContext still needs to be updated
            currentContext.end.line += 1;
            currentContext.end.character = 1;
        }
    }
    return m_match;
}

// There is also a Parser::expect defined in Parser.hpp (it's a template) that
// takes continuations for reporting errors just thought you'd like to know
std::string Parser::expect(TokenKind tok, std::string err, bool skipEat,
                           bool skipClean) {
    auto m_match = optional(tok, skipEat, skipClean);
    std::string match;
    if (!m_match.assignTo(match))
        errornext(*this, "Expected " + err + ".");
    return match;
}

MaybeASTNode Parser::parseDeclarator(bool base_only) {
    auto m_use = parseMacroUse();
    if (m_use)
        return m_use;

    Declarator * result = nullptr;
    Context context;
    context.start(&currentContext);

    bool tuple_sub_err = false;

    std::vector<std::string> specs;
    while (true) {
        MaybeString m_spec;
        std::string spec;
        (m_spec = optional(KWD_RAW)) || (m_spec = optional(KWD_IMMUT));
        if (m_spec.assignTo(spec))
            specs.push_back(spec);
        else
            break;
    }

    MaybeASTNode m_identifier;

    if (!base_only && optional(LSS)) { // Procedure
        expect(L_PAREN, "'('");
        ProcedureDeclarator * procDeclarator = new ProcedureDeclarator();
        bool vararg = false;
        while (!optional(R_PAREN)) {
            if (optional(ELLIPSIS)) {
                vararg = true;
                procDeclarator->setFlag(ProcedureDeclarator::IS_VARARG, true);
            } else {
                if (vararg)
                    errornext(
                        *this,
                        "Vararg ('...') denotation must be the last argument.");
                MaybeASTNode m_paramDeclarator = parseDeclarator();
                ASTNode * paramDeclarator = nullptr;
                if (!m_paramDeclarator.assignTo(paramDeclarator))
                    errornext(*this, "Expected type declarator in argument "
                                     "list for procedure type declarator.");
                procDeclarator->addParamDeclarator(paramDeclarator);
            }
            if (!optional(COMMA)) {
                expect(R_PAREN, "')'");
                break;
            }
        }
        if (!optional(COLON)) {
            procDeclarator->setRetDeclarator(newVoidDeclarator());
        } else {
            MaybeASTNode m_retDeclarator = parseDeclarator();
            ASTNode * retDeclarator = nullptr;
            if (!m_retDeclarator.assignTo(retDeclarator))
                errornext(*this, "Expected return type declarator for "
                                 "prodecure type declarator.");
            procDeclarator->setRetDeclarator(retDeclarator);
        }
        expect(GTR, "'>'");
        result = procDeclarator;
        context.finish(&currentContext, &justCleanedContext);
        result->setContext(context);
    } else if (!base_only && optional(L_PAREN)) { // Tuple
        TupleDeclarator * tupleDeclarator = new TupleDeclarator();
        int n_subs = 0;
        while (!optional(R_PAREN)) {
            MaybeASTNode m_subDeclarator = parseDeclarator();
            ASTNode * subDeclarator = nullptr;
            if (!m_subDeclarator.assignTo(subDeclarator))
                errornext(*this, "Expected type declarator as sub type in "
                                 "tuple declarator.");
            tupleDeclarator->addSubDeclarator(subDeclarator);
            n_subs += 1;
            if (!optional(COMMA)) {
                expect(R_PAREN, "')'");
                break;
            }
        }
        if (n_subs < 2)
            tuple_sub_err = true;
        result = tupleDeclarator;
        context.finish(&currentContext, &justCleanedContext);
        result->setContext(context);
    } else if ((m_identifier = parseQualifiedIdentifier(true))) { // Base
        Declarator * baseDeclarator = nullptr;
        ASTNode * identifier = nullptr;

        m_identifier.assignTo(identifier);
        BJOU_DEBUG_ASSERT(identifier);

        baseDeclarator = new Declarator();
        baseDeclarator->setIdentifier(identifier);

        // if (optional(TEMPLATE_BEGIN, true)) {
        if (optional(DOLLAR, true)) {
            MaybeASTNode m_templateInst = parseTemplateInst();
            ASTNode * templateInst = nullptr;
            if (m_templateInst.assignTo(templateInst))
                baseDeclarator->setTemplateInst(templateInst);
            else
                errornext(*this, "Expected template instantiation after '!'.");
        }

        result = baseDeclarator;
        context.finish(&currentContext, &justCleanedContext);
        result->setContext(context);
    } else if (optional(UNDER_SCORE)) {
        result = new PlaceholderDeclarator;
        context.finish(&currentContext, &justCleanedContext);
        result->setContext(context);
    } else
        return MaybeASTNode();

    // Array, Pointer, Maybe, Ref
    //                array/slice - [         pointer - *
    //                maybe       - ?         ref     - ref
    while (!base_only &&
           (optional(L_SQR_BRACKET, true) || optional(MULT, true) ||
            optional(QUESTION, true) || optional(KWD_REF, true))) {
        if (optional(L_SQR_BRACKET)) {
            if (optional(ELLIPSIS)) {
                result = new DynamicArrayDeclarator(result);
            } else if (optional(R_SQR_BRACKET, true)) {
                result = new SliceDeclarator(result);
            } else {
                MaybeASTNode m_expr = parseExpression();
                ASTNode * expr = nullptr;
                if (!m_expr.assignTo(expr)) {
                    errornext(*this, "Expected static array length expression.",
                              true, "use '[]' do declare slice",
                              "or '[...]' to declare dynamic array");
                }
                result = new ArrayDeclarator(result, expr);
            }
            expect(R_SQR_BRACKET, "']'");
            context.finish(&currentContext, &justCleanedContext);
            result->setContext(context);
        } else if (optional(MULT)) {
            result = new PointerDeclarator(result);
            context.finish(&currentContext, &justCleanedContext);
            result->setContext(context);
        } else if (optional(QUESTION)) {
            result = new MaybeDeclarator(result);
            context.finish(&currentContext, &justCleanedContext);
            result->setContext(context);
        } else if (optional(KWD_REF)) {
            result = new RefDeclarator(result);
            context.finish(&currentContext, &justCleanedContext);
            result->setContext(context);
        }
    }

    BJOU_DEBUG_ASSERT(result);

    if (tuple_sub_err)
        errorl(result->getContext(),
               "Tuple types must have at least 2 sub types.");

    return MaybeASTNode(result);
}

ASTNode * Parser::newVoidDeclarator() {
    Declarator * result = new Declarator();
    // result->getContext().start(&currentContext);
    Identifier * identifier = new Identifier();
    identifier->setUnqualified(compilation->frontEnd.getBuiltinVoidTypeName());
    result->setIdentifier(identifier);
    // result->getContext().finish(&currentContext, &justCleanedContext);
    return result;
}

MaybeASTNode Parser::parseTemplateDefineTypeDescriptor() {
    // always try parseTemplateDefineExpression() first!!!
    TemplateDefineTypeDescriptor * result = new TemplateDefineTypeDescriptor();
    result->getContext().start(&currentContext);

    MaybeString m_name = optional(IDENTIFIER);
    std::string name;
    if (!m_name.assignTo(name)) {
        delete result;
        return MaybeASTNode();
    }
    result->setName(name);
    if (optional(KWD_FROM)) {
        while (true) {
            MaybeASTNode m_decl = parseDeclarator();
            ASTNode * decl = nullptr;
            if (!m_decl.assignTo(decl))
                errornext(
                    *this,
                    "Expected type declarator in template type descriptor.",
                    true, "as template bound after 'from'");
            result->addBound(decl);
            if (!optional(VERT))
                break;
        }
    }
    result->getContext().finish(&currentContext, &justCleanedContext);

    return MaybeASTNode(result);
}

MaybeASTNode Parser::parseTemplateDefineExpression() {
    if (optional(VAR_DECL_BEG, true)) {
        TemplateDefineExpression * result = new TemplateDefineExpression();
        result->getContext().start(&currentContext);

        MaybeASTNode m_varDecl = parseVariableDeclaration();
        ASTNode * varDecl = nullptr;
        if (!m_varDecl.assignTo(varDecl)) {
            delete result;
            return MaybeASTNode();
        }

        result->setVarDecl(varDecl);

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseTemplateDefineVariadicTypeArgs() {
    // always try parseTemplateDefineExpression() first!!!
    if (optional(ELLIPSIS, true)) {
        TemplateDefineVariadicTypeArgs * result =
            new TemplateDefineVariadicTypeArgs();
        result->getContext().start(&currentContext);

        eat("...");

        result->setName(expect(IDENTIFIER, "identifier"));

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseTemplateDef() {
    if (optional(DOLLAR, true)) {
        TemplateDefineList * result = new TemplateDefineList();
        result->getContext().start(&currentContext);

        eat("$");

        if (optional(L_PAREN)) {
            while (true) {
                MaybeASTNode m_element;
                ASTNode * element = nullptr;
                (m_element = parseTemplateDefineExpression()) ||
                    (m_element = parseTemplateDefineTypeDescriptor()) ||
                    (m_element = parseTemplateDefineVariadicTypeArgs());

                if (!m_element.assignTo(element))
                    errornext(*this,
                              "Invalid item in template definition list.");

                result->addElement(element);
                if (!optional(COMMA))
                    break;
            }

            if (result->getElements().size() == 0)
                errorl(result->getContext(),
                       "Empty template definition list not allowed.");

            expect(R_PAREN, "')'");
        } else {
            MaybeASTNode m_element;
            ASTNode * element = nullptr;
            m_element = parseTemplateDefineTypeDescriptor();

            if (!m_element.assignTo(element))
                errornext(*this, "Expected template element identifier.");

            result->addElement(element);
        }

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseTemplateInst() {
    TemplateInstantiation * result = new TemplateInstantiation();
    result->getContext().start(&currentContext);

    expect(DOLLAR, "'$'");

    if (optional(L_PAREN)) {
        while (!optional(R_PAREN, true)) {
            MaybeASTNode m_element;
            ASTNode * element = nullptr;

            if (optional(L_SQR_BRACKET)) {
                m_element = parseExpression();
                if (!m_element.assignTo(element))
                    errornext(*this, "Invalid expression argument in template "
                                     "instantiation.");
                expect(R_SQR_BRACKET, "']'");
            } else {
                m_element = parseDeclarator();
                if (!m_element.assignTo(element))
                    errornext(
                        *this, "Invalid argument in template instantiation.",
                        true,
                        "Note: Expression arguments should be surrounded by "
                        "brackets '([expr])'");
            }
            BJOU_DEBUG_ASSERT(element);
            result->addElement(element);
            if (!optional(COMMA))
                break;
        }
        expect(R_PAREN, "')'");
    } else {
        MaybeASTNode m_element;
        ASTNode * element = nullptr;

        m_element = parseDeclarator(/* base only =*/true);
        if (!m_element.assignTo(element))
            errornext(*this,
                      "Invalid expression argument in template instantiation.");
        BJOU_DEBUG_ASSERT(element);
        result->addElement(element);
    }

    result->getContext().finish(&currentContext, &justCleanedContext);

    return MaybeASTNode(result);
}

MaybeASTNode Parser::parseQualifiedIdentifier(bool allow_primative_typenames) {
    MaybeString m_identifier =
        optional(allow_primative_typenames ? IDENTIFIER_ALLOW_PRIMATIVE_TYPES
                                           : IDENTIFIER,
                 true);
    std::string identifier;

    if (m_identifier.assignTo(identifier)) {
        Identifier * result = new Identifier();
        result->getContext().start(&currentContext);

        expect(allow_primative_typenames ? IDENTIFIER_ALLOW_PRIMATIVE_TYPES
                                         : IDENTIFIER,
               "");

        while (optional(DBL_COLON)) {
            result->addNamespace(identifier);
            identifier = expect(IDENTIFIER, "identifier", false, false,
                                "for qualified identifier");
        }
        result->setUnqualified(identifier);

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }

    return MaybeASTNode();
}

MaybeASTNode Parser::parseExpression(int minPrecedence) {
    MaybeASTNode m_primary = parseOperand();
    ASTNode * primary = nullptr;

    if (!m_primary.assignTo(primary))
        return MaybeASTNode();

    m_primary = parseExpression_r(primary, minPrecedence);

    if (!m_primary)
        return MaybeASTNode();

    return m_primary;
}

static BinaryExpression * newBinaryExpressionFromOp(std::string & op) {
    if (op == "()")
        return new CallExpression;
    else if (op == "[]")
        return new SubscriptExpression;
    else if (op == ".")
        return new AccessExpression;
    else if (op == "->")
        return new InjectExpression;
    else if (op == "*")
        return new MultExpression;
    else if (op == "/")
        return new DivExpression;
    else if (op == "%")
        return new ModExpression;
    else if (op == "+")
        return new AddExpression;
    else if (op == "-")
        return new SubExpression;
    else if (op == "<")
        return new LssExpression;
    else if (op == "<=")
        return new LeqExpression;
    else if (op == ">")
        return new GtrExpression;
    else if (op == ">=")
        return new GeqExpression;
    else if (op == "==")
        return new EquExpression;
    else if (op == "!=")
        return new NeqExpression;
    else if (op == "&&" || op == "and")
        return new LogAndExpression;
    else if (op == "||" || op == "or")
        return new LogOrExpression;
    else if (op == "??")
        return new MaybeAssignExpression;
    else if (op == "=")
        return new AssignmentExpression;
    else if (op == "*=")
        return new MultAssignExpression;
    else if (op == "/=")
        return new DivAssignExpression;
    else if (op == "%=")
        return new ModAssignExpression;
    else if (op == "+=")
        return new AddAssignExpression;
    else if (op == "-=")
        return new SubAssignExpression;
    else
        internalError("bad operator '" + op + "'");
    return nullptr;
}

static UnaryPreExpression * newUnaryPreExpressionFromOp(std::string & op) {
    if (op == "!" || op == "not")
        return new NotExpression;
    else if (op == "new")
        return new NewExpression;
    else if (op == "delete")
        return new DeleteExpression;
    else if (op == "sizeof")
        return new SizeofExpression;
    else if (op == "proc")
        return new ProcLiteral;
    else if (op == "extern")
        return new ExternLiteral;
    else if (op == "some")
        return new SomeLiteral;
    else if (op == "@")
        return new DerefExpression;
    else if (op == "&")
        return new AddressExpression;
    else
        internalError("bad operator '" + op + "'");
    return nullptr;
}

static UnaryPostExpression * newUnaryPostExpressionFromOp(std::string & op) {
    return nullptr;
}

MaybeASTNode Parser::parseExpression_r(ASTNode * left, int minPrecedence) {
    // based on algorithm found here:
    // https://en.wikipedia.org/wiki/Operator-precedence_parser#Pseudo-code

    MaybeString m_op, m_lookahead;
    std::string op, lookahead;
    MaybeASTNode m_right;
    Expression * right = nullptr;
    Expression * result = nullptr;

    /*
        lookahead := peek next token
        while lookahead is a binary operator whose precedence is >=
       min_precedence op := lookahead
     */

    while (true) {
        m_op = parseBinaryOperator(true);
        if (!(m_op.assignTo(op) && precedence(op.c_str()) >= minPrecedence))
            break;
        /*
            advance to next token
            rhs := parse_primary ()
         */
        if (op == "[]") {
            expect(L_SQR_BRACKET, "'['");
            m_right = parseOperand();
            if (m_right.assignTo(right)) {
                m_right = parseExpression_r(right, 0);
                m_right.assignTo(right);
                BJOU_DEBUG_ASSERT(right);
                right->getContext().finish(&currentContext,
                                           &justCleanedContext);
                expect(R_SQR_BRACKET, "']'");
            }
        } else if (op == "()") {
            expect(L_PAREN, "'('");
            m_right = parseArgList();
            if (!m_right) //.assignTo(right)) // this won't work because we have
                          // to force an ArgList where an Expression should go
                return MaybeASTNode();
            // force it
            ASTNode * argList = nullptr;
            m_right.assignTo(argList);
            BJOU_DEBUG_ASSERT(argList);
            right = (Expression *)argList;

            expect(
                R_PAREN, "')'", false, false,
                optional(COLON)
                    ? "precede procedure declarations with the keyword 'proc'"
                    : "to end procedure call");
            right->getContext().finish(&currentContext, &justCleanedContext);
        } else {
            eat(op);
            if (binary(op.c_str())) {
                m_right = parseOperand();
                if (!m_right.assignTo(right))
                    errornext(*this, "Missing operand to binary expression.");
            }
        }

        /*
             lookahead := peek next token
             while lookahead is a binary operator whose precedence is greater
             than op's, or a right-associative operator
             whose precedence is equal to op's
         */

        while (true) {
            m_lookahead = parseBinaryOperator(true);
            if (! // good luck reading this
                (m_lookahead.assignTo(lookahead) &&
                 (precedence(lookahead.c_str()) > precedence(op.c_str()) ||
                  (rightAssoc(lookahead.c_str()) &&
                   precedence(lookahead.c_str()) == precedence(op.c_str())))))
                break;
            /*
                rhs := parse_expression_1 (rhs, lookahead's precedence)
             */
            m_right = parseExpression_r(right, precedence(lookahead.c_str()));
            if (!m_right.assignTo(right))
                errornext(*this, "Missing operand to binary expression.");
        }

        /*
            lhs := the result of applying op with operands lhs and rhs
         */

        // @expr
        /*
        if (op == "()")
            result = new CallExpression();
        else if (op == "=")
            result = new AssignmentExpression();
        else if (op == ".")
            result = new AccessExpression();
        else result = new BinaryExpression();
         */
        result = newBinaryExpressionFromOp(op);

        BJOU_DEBUG_ASSERT(result);
        BJOU_DEBUG_ASSERT(left);
        BJOU_DEBUG_ASSERT(right);

        result->setContents(op);
        result->setLeft(left);
        result->setRight(right);
        result->setContext(left->getContext());
        result->getContext().end = right->getContext().end;
        // result->getContext().finish(&right->getContext());

        // check for postfix
        MaybeString m_postfixOp = parseUnaryPostfixOperator(true);
        std::string postfixOp;
        if (m_postfixOp.assignTo(postfixOp)) {
            result = (Expression *)applyPostfixOp(postfixOp, result,
                                                  (Expression *)left, right,
                                                  m_op, minPrecedence);
            BJOU_DEBUG_ASSERT(result);
        }

        left = result;
    }

    // check for postfix again..
    MaybeString m_postfixOp = parseUnaryPostfixOperator(true);
    result = (Expression *)left; // IMPORTANT
    std::string postfixOp;
    if (m_postfixOp.assignTo(postfixOp)) {
        result = (Expression *)applyPostfixOp(
            postfixOp, result, (Expression *)left, right, m_op, minPrecedence);
        BJOU_DEBUG_ASSERT(result);
    }

    // result->context.finish(&currentContext, &justCleanedContext);

    return MaybeASTNode(result);
}

MaybeString Parser::parseBinaryOperator(bool skipEat) {
    const TokenKind opOrder[] = {
        DOT,     ARROW,       L_SQR_BRACKET, L_PAREN, PLUS_EQ, MIN_EQ,
        MULT_EQ, DIV_EQ,      MOD_EQ,        MULT,    DIV,     MOD,
        PLUS,    MINUS,       LEQ,           LSS,     GEQ,     GTR,
        EQU,     NEQ,         ASSIGN,        AND,     KWD_AND, OR,
        KWD_OR,  DBL_QUESTION};

    int line = currentContext.begin.line;

    MaybeString m_op;

    for (TokenKind tok : opOrder) {
        m_op = optional(tok, true);
        std::string op, result;
        if (m_op.assignTo(op)) {
            result = op;
            if (tok == L_SQR_BRACKET) {
                result += ']';
            } else if (tok == L_PAREN) {
                // calls must be on the same line
                if (line != currentContext.end.line)
                    return MaybeString();
                result += ')';
            }
            if (!skipEat)
                eat(op);
            return MaybeString(result);
        }
    }

    return MaybeString();
}

MaybeString Parser::parseUnaryPrefixOperator(bool skipEat) {
    const TokenKind opOrder[] = {EXCLAM,     KWD_NOT,  KWD_NEW,    KWD_DELETE,
                                 KWD_SIZEOF, KWD_PROC, KWD_EXTERN, KWD_SOME,
                                 AT,         AMP,      TILDE};

    for (TokenKind tok : opOrder) {
        MaybeString m_result = optional(tok, skipEat);
        if (m_result)
            return m_result;
    }

    return MaybeString();
}

MaybeString Parser::parseUnaryPostfixOperator(bool skipEat) {
    const TokenKind opOrder[] = {KWD_AS, QUESTION};

    // quick-fix so that we don't mistakenly grab one ? from a ??
    if (optional(DBL_QUESTION, true))
        return MaybeString();

    for (TokenKind tok : opOrder) {
        MaybeString m_result = optional(tok, skipEat);
        if (m_result)
            return m_result;
    }

    return MaybeString();
}

ASTNode * Parser::applyPostfixOp(std::string & postfixOp, Expression * expr,
                                 Expression * left, Expression * right,
                                 MaybeString & m_binOp, int minPrecedence) {
    /*
         Here, we try to skip applying a postfix op if the current binop is of a
       higher precedence (access, call, etc..). Otherwise, if there is no binop
       or if we've reached the end of the expression and the current binop is
       higher precedence (see Operator.hpp for this number), we apply the
       postfix to the whole thing. If it is of lower precedence, we only apply
       it to the immediate sub expression.
     */

    Expression * result = expr;
    std::string binOp;
    m_binOp.assignTo(binOp);
    eat(postfixOp);
    UnaryPostExpression * postfix =
        postfixOp == "as" ? new AsExpression()
                          : nullptr; // new UnaryPostExpression(); // @expr
    postfix->setContents(postfixOp);

    if (!m_binOp || (minPrecedence == 0 && precedence(binOp.c_str()) > 7)) {
        postfix->setLeft(expr);
        postfix->getContext().start(&expr->getContext());
        postfix->getContext().begin = expr->getContext().begin;
        if (postfixOp == "as") {
            MaybeASTNode m_decl = parseDeclarator();
            ASTNode * decl = nullptr;
            if (!m_decl.assignTo(decl))
                errornext(*this,
                          "Expected type declarator for 'as' type cast.");
            postfix->setRight(decl);
        }
        postfix->getContext().finish(&currentContext, &justCleanedContext);
        result = postfix;
    } else if (m_binOp && precedence(binOp.c_str()) < 8) {
        postfix->setLeft(right);
        postfix->getContext().start(&right->getContext());
        postfix->getContext().begin = right->getContext().begin;
        result->setRight(postfix);
        if (postfixOp == "as") {
            MaybeASTNode m_decl = parseDeclarator();
            ASTNode * decl = nullptr;
            if (!m_decl.assignTo(decl))
                errornext(*this,
                          "Expected type declarator for 'as' type cast.");
            ((Expression *)result->getRight())->setRight(decl);
        }
        postfix->getContext().finish(&currentContext, &justCleanedContext);
    }

    return result;
}

MaybeASTNode Parser::parseTerminatingExpression() {
    auto m_use = parseMacroUse();
    if (m_use)
        return m_use;

    Expression * result = nullptr;
    Context context;
    context.start(&currentContext);

    for (TokenKind kind : {KWD_TRUE, KWD_FALSE, KWD_NOTHING, FLOATING_PT,
                           INTEGER, CHAR_LITERAL, STRING_LITERAL}) {
        MaybeString m_val = optional(kind);
        std::string val;
        if (m_val.assignTo(val)) {
            switch (kind) {
            case KWD_TRUE:
            case KWD_FALSE:
                result = new BooleanLiteral();
                break;
            case KWD_NOTHING:
                result = new NothingLiteral();
                break;
            case FLOATING_PT:
                result = new FloatLiteral();
                break;
            case INTEGER:
                result = new IntegerLiteral();
                break;
            case CHAR_LITERAL:
                result = new CharLiteral();
                break;
            case STRING_LITERAL:
                result = new StringLiteral();
                break;
            default:
                internalError("This should be impossible.");
                break;
            }
            result->setFlag(Expression::TERMINAL, true);

            result->setContents(val);
            context.end = currentContext.end;
            result->setContext(context);
            result->getContext().finish(&context, &justCleanedContext);

            return MaybeASTNode(result);
        }
    }

    MaybeASTNode m_node;
    Expression * node = nullptr;

    if ((m_node = parseQualifiedIdentifier())) {
        m_node.assignTo(node);
        BJOU_DEBUG_ASSERT(node);
        // if (optional(TEMPLATE_BEGIN, true)) {
        if (optional(DOLLAR, true)) {
            MaybeASTNode m_templateInst = parseTemplateInst();
            ASTNode * templateInst = nullptr;
            if (!m_templateInst.assignTo(templateInst))
                errornext(*this, "Expected template instantiation after '!'.");
            node->setRight(templateInst);
        }
        node->setFlag(Expression::TERMINAL, true);
    } else if ((m_node = parseInitializerList()) ||
               (m_node = parseParentheticalExpressionOrTuple()) ||
               (m_node = parseSliceOrDynamicArrayExpression()) ||
               (m_node = parseLenExpression())) {
        m_node.assignTo(node);
        BJOU_DEBUG_ASSERT(node);
    } else
        return MaybeASTNode();

    return MaybeASTNode(node);
}

MaybeASTNode Parser::parseParentheticalExpressionOrTuple() {
    Context contextBegin;
    contextBegin.start(&currentContext);
    if (optional(L_PAREN)) {
        MaybeASTNode m_expression = parseExpression();
        Expression * expression = nullptr;
        if (!m_expression.assignTo(expression)) {
            if (optional(R_PAREN)) {
                contextBegin.finish(&currentContext, &justCleanedContext);
                errorl(contextBegin, "Empty set of parentheses.");
            } else
                errornext(*this, "Invalid expression.");
        }
        if (optional(COMMA)) { // tuple
            TupleLiteral * tuple = new TupleLiteral();
            tuple->setContext(contextBegin);
            tuple->addSubExpression(expression);

            // more expressions
            while (!optional(R_PAREN)) {
                expression = nullptr;
                m_expression = parseExpression();
                if (!m_expression.assignTo(expression))
                    errornext(*this, "Expected tuple sub-expression.");
                BJOU_DEBUG_ASSERT(expression);
                tuple->addSubExpression(expression);
                if (!optional(COMMA)) {
                    expect(R_PAREN, "')'");
                    break;
                }
            }
            tuple->getContext().finish(&currentContext, &justCleanedContext);
            return MaybeASTNode(tuple);
        } else {
            expect(R_PAREN, "')'");
            expression->setContext(contextBegin);
            expression->getContext().finish(&currentContext,
                                            &justCleanedContext);
            expression->setFlag(Expression::PAREN, true);
            return MaybeASTNode(expression);
        }
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseInitializerList() {
    if (optional(L_CURLY_BRACE, true)) {
        InitializerList * result = new InitializerList();
        result->getContext().start(&currentContext);

        eat("{");

        if (optional(R_CURLY_BRACE)) {
            result->getContext().finish(&currentContext, &justCleanedContext);
            errorl(result->getContext(), "Empty initializer list.");
        } else {
            MaybeASTNode m_objDeclarator = parseDeclarator();
            ASTNode * objDeclarator = nullptr;

            if (m_objDeclarator.assignTo(objDeclarator)) {
                expect(COLON, "':'");
                result->setObjDeclarator(objDeclarator);
                while (!optional(R_CURLY_BRACE)) {
                    expect(DOT,
                           "'.<member name>' for type member initialization");
                    std::string name = expect(IDENTIFIER, "type member name");
                    expect(ASSIGN, "'=' for type member initialization");
                    MaybeASTNode m_expression = parseExpression();
                    ASTNode * expression = nullptr;
                    if (!m_expression.assignTo(expression))
                        errornext(*this,
                                  "Invalid expression in initializer list.");
                    result->addMemberName(name);
                    result->addExpression(expression);
                    if (!optional(COMMA)) {
                        expect(R_CURLY_BRACE, "'}'");
                        break;
                    }
                }
            } else {
                while (!optional(R_CURLY_BRACE)) {
                    MaybeASTNode m_expression = parseExpression();
                    ASTNode * expression = nullptr;
                    if (!m_expression.assignTo(expression))
                        errornext(
                            *this,
                            "Invalid expression in array initializer list.");
                    result->addExpression(expression);
                    if (!optional(COMMA)) {
                        expect(R_CURLY_BRACE, "'}'");
                        break;
                    }
                }
            }
            result->getContext().finish(&currentContext, &justCleanedContext);
            return MaybeASTNode(result);
        }
    }

    return MaybeASTNode();
}

MaybeASTNode Parser::parseSliceOrDynamicArrayExpression() {
    if (optional(L_SQR_BRACKET, true)) {
        Context context;
        context.start(&currentContext);

        eat("[");

        if (optional(R_SQR_BRACKET)) {
            context.finish(&currentContext, &justCleanedContext);
            errorl(context, "Empty slice or dynamic array expression.");
        } else if (optional(ELLIPSIS)) {
            DynamicArrayExpression * result = new DynamicArrayExpression;
            result->setContext(context);
            ASTNode * decl = nullptr;
            MaybeASTNode m_decl;

            m_decl = parseDeclarator();
            if (!m_decl.assignTo(decl))
                errornext(
                    *this,
                    "Expected type declarator in dynamic array expression.",
                    true, "dynamic array expression syntax: '[...type]'");

            result->setTypeDeclarator(decl);

            expect(R_SQR_BRACKET, "']'");

            result->getContext().finish(&currentContext, &justCleanedContext);
            return MaybeASTNode(result);
        } else {
            SliceExpression * result = new SliceExpression();
            result->setContext(context);
            ASTNode * expr = nullptr;
            MaybeASTNode m_expr;

            m_expr = parseExpression();
            if (!m_expr.assignTo(expr))
                errornext(*this, "Invalid source in slice expression.", true,
                          "slice syntax: '[ source, start_index:length ]'");
            result->setSrc(expr);

            expect(COMMA, "','");

            m_expr = parseExpression();
            if (!m_expr.assignTo(expr))
                errornext(*this, "Invalid starting index in slice expression.",
                          true,
                          "slice syntax: '[ source, start_index:length ]'");
            result->setStart(expr);

            expect(COLON, "':'");

            m_expr = parseExpression();
            if (!m_expr.assignTo(expr))
                errornext(*this, "Invalid length in slice expression.", true,
                          "slice syntax: '[ source, start_index:length ]'");
            result->setLength(expr);

            expect(R_SQR_BRACKET, "']'");

            result->getContext().finish(&currentContext, &justCleanedContext);
            return MaybeASTNode(result);
        }
    }

    return MaybeASTNode();
}

MaybeASTNode Parser::parseLenExpression() {
    if (!optional(OR, true) && optional(VERT, true)) {
        LenExpression * result = new LenExpression();
        result->getContext().start(&currentContext);

        eat("|");

        ASTNode * expr = nullptr;
        MaybeASTNode m_expr;

        m_expr = parseExpression();
        if (!m_expr.assignTo(expr))
            errornext(*this, "Invalid expression in cardinality expression.");
        result->setExpr(expr);

        expect(VERT, "'|'");

        result->getContext().finish(&currentContext, &justCleanedContext);
        return MaybeASTNode(result);
    }

    return MaybeASTNode();
}

MaybeASTNode Parser::parseOperand() {
    Expression * result = nullptr;
    Context context;
    context.start(&currentContext);

    MaybeString m_op = parseUnaryPrefixOperator(true);
    std::string op;

    if (m_op.assignTo(op)) {
        if (op == "new") {
            result = new NewExpression();
            eat(op);
            MaybeASTNode m_decl = parseDeclarator();
            ASTNode * decl = nullptr;
            if (!m_decl.assignTo(decl))
                errornext(*this, "Expected type declarator.", true,
                          "after '" + op + "'");
            result->setRight(decl);
            MaybeASTNode m_more_expr = parseExpression_r(result, 9);
            m_more_expr.assignTo(result);
            BJOU_DEBUG_ASSERT(result);
        } else if (op == "delete") {
            result = new DeleteExpression();
            eat(op);
            MaybeASTNode m_expression = parseExpression();
            ASTNode * expression = nullptr;
            if (!m_expression.assignTo(expression))
                errornext(*this, "Expected expression.", true,
                          "after '" + op + "'");
            result->setRight(expression);
            MaybeASTNode m_more_expr = parseExpression_r(result, 9);
            m_more_expr.assignTo(result);
            BJOU_DEBUG_ASSERT(result);
        } else if (op == "sizeof") {
            result = new SizeofExpression();
            eat(op);
            MaybeASTNode m_decl = parseDeclarator();
            ASTNode * decl = nullptr;
            if (!m_decl.assignTo(decl))
                errornext(*this, "Expected type declarator.", true,
                          "after '" + op + "'");
            result->setRight(decl);
            MaybeASTNode m_more_expr = parseExpression_r(result, 9);
            m_more_expr.assignTo(result);
            BJOU_DEBUG_ASSERT(result);
        } else if (op == "proc") {
            result = new ProcLiteral();
            MaybeASTNode m_proc = parseProc();
            ASTNode * proc = nullptr;
            if (!m_proc.assignTo(proc))
                errornext(*this, "Expected procedure literal after 'proc'.");
            if (proc->getFlag(ASTNode::IS_TEMPLATE)) {
                TemplateProc * templateProc = (TemplateProc *)proc;
                errorl(templateProc->getTemplateDef()->getContext(),
                       "Template procedures are not allowed as part of an "
                       "expression.");
            }
            result->setRight(proc);
            MaybeASTNode m_more_expr = parseExpression_r(result, 9);
            m_more_expr.assignTo(result);
            BJOU_DEBUG_ASSERT(result);
        } else if (op == "extern") {
            result = new ExternLiteral();
            MaybeASTNode m_externSig = parseExternSig();
            ASTNode * externSig = nullptr;
            if (!m_externSig.assignTo(externSig))
                errornext(*this,
                          "Expected procedure signature after 'extern'.");
            result->setRight(externSig);
            MaybeASTNode m_more_expr = parseExpression_r(result, 9);
            m_more_expr.assignTo(result);
            BJOU_DEBUG_ASSERT(result);
        } else if (op == "some") {
            eat(op);
            result = new SomeLiteral();
            MaybeASTNode m_expression = parseExpression();
            ASTNode * expression = nullptr;
            if (!m_expression.assignTo(expression))
                errornext(*this, "Expected expression after 'some'.");
            result->setRight(expression);
            MaybeASTNode m_more_expr = parseExpression_r(result, 9);
            m_more_expr.assignTo(result);
            BJOU_DEBUG_ASSERT(result);
        } else {
            eat(op);
            MaybeASTNode m_operand = parseOperand();
            ASTNode * operand = nullptr;
            if (!m_operand.assignTo(operand))
                return MaybeASTNode();
            result = newUnaryPreExpressionFromOp(op);
            // result = new UnaryPreExpression(); // @expr
            result->setRight(operand);
            MaybeASTNode m_more_expr = parseExpression_r(result->getRight(), 9);
            ASTNode * more_expr = nullptr;
            m_more_expr.assignTo(more_expr);
            BJOU_DEBUG_ASSERT(more_expr);
            result->setRight(more_expr);
        }
        result->setContents(op);
        context.finish(&currentContext, &justCleanedContext);
        result->setContext(context);
    } else {
        MaybeASTNode term = parseTerminatingExpression();
        return term;
    }

    return MaybeASTNode(result);
}

MaybeASTNode Parser::parseArgList() {
    ArgList * result = new ArgList();
    result->getContext().start(&currentContext);

    if (!optional(R_PAREN, true)) {
        while (true) {
            if (!optional(R_PAREN, true)) {
                MaybeASTNode m_expr = parseExpression();
                ASTNode * expr = nullptr;
                if (!m_expr.assignTo(expr)) {
                    delete result;
                    return MaybeASTNode();
                }
                result->addExpression(expr);
            }
            if (!optional(COMMA))
                break;
        }
    }
    result->context.finish(&currentContext, &justCleanedContext);

    return MaybeASTNode(result);
}

MaybeASTNode Parser::parseTopLevelNode() {
    static ASTNode * module_declared = nullptr;
    ASTNode * mod_check = nullptr;

    MaybeASTNode m_node;
    (m_node = parseModuleDeclaration()) || (m_node = parseNamespace()) ||
        (m_node = parseProc()) || (m_node = parseExternSig()) ||
        (m_node = parseType()) || (m_node = parseInterfaceDef()) ||
        (m_node = parseEnum()) || (m_node = parseConstant()) ||
        (m_node = parseVariableDeclaration()) || (m_node = parseAlias()) ||
        (m_node = parseStatement()) || (m_node = parseImport());

    ASTNode * node = nullptr;
    if (m_node.assignTo(node)) {
        if (node->nodeKind == ASTNode::STRUCT)
            compilation->frontEnd.structs.push_back(node);
        else if (node->nodeKind == ASTNode::INTERFACE_DEF)
            compilation->frontEnd.ifaceDefs.push_back(node);
    }

    return m_node;
}

MaybeASTNode AsyncParser::parseTopLevelNode() {
    static ASTNode * module_declared = nullptr;
    ASTNode * mod_check = nullptr;

    MaybeASTNode m_node;
    (m_node = parseModuleDeclaration()) || (m_node = parseNamespace()) ||
        (m_node = parseProc()) || (m_node = parseExternSig()) ||
        (m_node = parseType()) || (m_node = parseInterfaceDef()) ||
        (m_node = parseEnum()) || (m_node = parseConstant()) ||
        (m_node = parseVariableDeclaration()) || (m_node = parseAlias()) ||
        (m_node = parseStatement()) || (m_node = parseImport());

    ASTNode * node = nullptr;
    if (m_node.assignTo(node)) {
        if (node->nodeKind == ASTNode::STRUCT)
            structs.push_back(node);
        else if (node->nodeKind == ASTNode::INTERFACE_DEF)
            ifaceDefs.push_back(node);
    }

    return m_node;
}

MaybeASTNode ImportParser::parseTopLevelNode() {
    static ASTNode * module_declared = nullptr;
    ASTNode * mod_check = nullptr;

    MaybeASTNode m_node;
    (m_node = parseModuleDeclaration()) || (m_node = parseNamespace()) ||
        (m_node = parseProc()) || (m_node = parseExternSig()) ||
        (m_node = parseType()) || (m_node = parseInterfaceDef()) ||
        (m_node = parseEnum()) || (m_node = parseConstant()) ||
        (m_node = parseVariableDeclaration()) || (m_node = parseAlias()) ||
        (m_node = parseStatement()) || (m_node = parseImport());

    ASTNode * node = nullptr;
    if (m_node.assignTo(node)) {
        if (node->nodeKind == ASTNode::STRUCT)
            structs.push_back(node);
        else if (node->nodeKind == ASTNode::INTERFACE_DEF)
            ifaceDefs.push_back(node);
    }

    return m_node;
}

MaybeASTNode Parser::parseImport() {
    if (optional(KWD_IMPORT, true)) {
        Import * result = new Import();
        result->getContext().start(&currentContext);

        eat("import");

        MaybeString m_module = optional(IDENTIFIER);
        std::string module;
        if (!m_module.assignTo(module)) {
            m_module = optional(STRING_LITERAL);
            if (!m_module.assignTo(module))
                errornext(*this, "Expected module identifier or path.");
            result->setFlag(Import::FROM_PATH, true);
        }
        result->setModule(module);

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseModuleDeclaration() {
    if (optional(KWD_MODULE, true)) {
        ModuleDeclaration * result = new ModuleDeclaration();
        result->getContext().start(&currentContext);

        eat("module");

        std::string identifier = expect(IDENTIFIER, "module identifier");
        result->setIdentifier(identifier);

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseNamespace() {
    if (optional(KWD_NAMESPACE, true)) {
        Namespace * result = new Namespace();
        result->getContext().start(&currentContext);

        eat("namespace");

        result->setName(expect(IDENTIFIER, "namespace identifier"));
        expect(L_CURLY_BRACE, "'{'");
        while (true) {
            MaybeASTNode m_subNode = parseTopLevelNode();
            ASTNode * subNode = nullptr;
            if (!m_subNode.assignTo(subNode))
                break;

            result->addNode(subNode);
        }
        expect(R_CURLY_BRACE, "'}'", false, false,
               "to close namespace '" + result->getName() + "'");

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseType() {
    if (optional(KWD_ABSTRACT, true) || optional(KWD_TYPE, true)) {
        Context context, nameContext;
        context.start(&currentContext);
        bool abstract = optional(KWD_ABSTRACT);

        expect(KWD_TYPE, "'type'");

        nameContext.start(&currentContext);
        std::string name = expect(IDENTIFIER, "type identifier");
        nameContext.finish(&currentContext, &justCleanedContext);

        MaybeASTNode m_templateDef = parseTemplateDef();
        ASTNode * templateDef = nullptr;
        m_templateDef.assignTo(
            templateDef); // we don't really care what this returns

        if (optional(ASSIGN)) {
            MaybeASTNode m_aliasDeclarator = parseDeclarator();
            ASTNode * aliasDeclarator = nullptr;
            if (m_aliasDeclarator.assignTo(aliasDeclarator)) {
                if (abstract)
                    errorl(nameContext, "Only struct types can be abstract.",
                           true, "'" + name + "' is a type alias.");
                Alias * result = new Alias();
                TemplateAlias * templateResult = new TemplateAlias();
                result->setContext(context);
                templateResult->setContext(context);
                result->setName(name);
                result->setNameContext(nameContext);
                templateResult->setNameContext(nameContext);
                result->setDeclarator(aliasDeclarator);

                result->getContext().finish(&currentContext,
                                            &justCleanedContext);

                if (templateDef) {
                    templateResult->getContext().finish(&currentContext,
                                                        &justCleanedContext);
                    templateResult->setTemplateDef(templateDef);
                }

                if (templateResult->getTemplateDef()) {
                    templateResult->setTemplate(result);
                    return MaybeASTNode(templateResult);
                }

                delete templateResult;

                return MaybeASTNode(result);
            }
        } else {
            Struct * result = new Struct();
            TemplateStruct * templateResult = new TemplateStruct();

            result->setFlag(Struct::IS_ABSTRACT, abstract);

            result->setContext(context);
            templateResult->setContext(context);
            result->setName(name);
            result->setNameContext(nameContext);
            templateResult->setNameContext(nameContext);

            if (optional(KWD_EXTENDS)) {
                MaybeASTNode m_extends = parseDeclarator();
                ASTNode * extends = nullptr;
                if (!m_extends.assignTo(extends))
                    errornext(*this,
                              "Expected type declarator after 'extends'.");
                result->setExtends(extends);
            }

            if (result->getExtends())
                expect(L_CURLY_BRACE, "'{'");
            else
                expect(L_CURLY_BRACE, "'{' for struct type or a valid "
                                      "declarator for an alias type");

            while (true) {
                MaybeASTNode m_member = parseVariableDeclaration();
                ASTNode * member = nullptr;
                if (m_member.assignTo(member)) {
                    result->addMemberVarDecl(member);
                } else {
                    m_member = parseConstant();
                    if (m_member.assignTo(member)) {
                        result->addConstantDecl(member);
                    } else {
                        m_member = parseProc();
                        if (m_member.assignTo(member)) {
                            if (member->getFlag(ASTNode::IS_TEMPLATE))
                                result->addMemberTemplateProc(member);
                            else
                                result->addMemberProc(member);
                        } else {
                            MaybeASTNode m_interfaceImpl = parseInterfaceImpl();
                            ASTNode * interfaceImpl = nullptr;
                            if (m_interfaceImpl.assignTo(interfaceImpl)) {
                                result->addInterfaceImpl(interfaceImpl);
                            } else
                                break;
                        }
                    }
                }
            }
            expect(R_CURLY_BRACE, "'}'");

            result->getContext().finish(&currentContext, &justCleanedContext);
            templateResult->getContext().finish(&currentContext,
                                                &justCleanedContext);

            if (templateDef)
                templateResult->setTemplateDef(templateDef);

            if (templateResult->getTemplateDef()) {
                templateResult->setTemplate(result);
                return MaybeASTNode(templateResult);
            }

            delete templateResult;

            return MaybeASTNode(result);
        }
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseInterfaceDef() {
    if (optional(KWD_INTERFACE, true)) {
        InterfaceDef * result = new InterfaceDef();
        result->getContext().start(&currentContext);

        eat("interface");

        result->setName(expect(IDENTIFIER, "interface identifier"));

        expect(L_CURLY_BRACE, "'{'");
        while (true) {
            MaybeASTNode m_proc = parseProc(/*parse_body =*/false);
            ASTNode * _proc = nullptr;
            if (m_proc.assignTo(_proc)) {
                Procedure * proc = (Procedure *)_proc;
                proc->setFlag(Procedure::IS_INTERFACE_DECL, true);
                result->addProc(proc->getName(), _proc);
            } else
                break;
        }
        expect(R_CURLY_BRACE, "'}'");

        result->getContext().finish(&currentContext, &justCleanedContext);
        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseInterfaceImpl() {
    if (optional(KWD_IMPLEMENTS, true)) {
        InterfaceImplementation * result = new InterfaceImplementation();
        result->getContext().start(&currentContext);

        eat("implements");

        MaybeASTNode m_qualIdentifier = parseQualifiedIdentifier();
        ASTNode * qualIdentifier = nullptr;
        if (!m_qualIdentifier.assignTo(qualIdentifier))
            errornext(*this, "Expected identifier referring to an interface.",
                      true, "after 'implements'");
        result->setIdentifier(qualIdentifier);

        if (optional(L_CURLY_BRACE)) {
            while (true) {
                MaybeASTNode m_proc = parseProc();
                ASTNode * _proc = nullptr;

                if (!m_proc.assignTo(_proc))
                    break;

                Procedure * proc = (Procedure *)_proc;
                proc->setFlag(Procedure::IS_INTERFACE_IMPL, true);

                result->addProc(proc->getName(), _proc);
            }
            expect(R_CURLY_BRACE, "'}'");
        } else
            result->setFlag(InterfaceImplementation::PUNT_TO_EXTENSION, true);

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseEnum() {
    if (optional(KWD_ENUM, true)) {
        Enum * result = new Enum();
        result->getContext().start(&currentContext);

        eat("enum");

        result->getNameContext().start(&currentContext);
        result->setName(expect(IDENTIFIER, "enum identifier"));
        result->getNameContext().finish(&currentContext, &justCleanedContext);

        expect(L_CURLY_BRACE, "'{'");
        while (true) {
            result->addIdentifier(expect(IDENTIFIER, "identifier"));
            if (!optional(COMMA))
                break;
        }
        expect(R_CURLY_BRACE, "'}'");

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseAlias() {
    if (optional(KWD_ALIAS, true)) {
        Alias * result = new Alias;
        result->getContext().start(&currentContext);

        eat("alias");

        result->setName(expect(IDENTIFIER, "alias identifier"));

        MaybeASTNode m_declarator = parseDeclarator();
        ASTNode * declarator = nullptr;
        if (!m_declarator.assignTo(declarator))
            errornext(*this, "Expected valid type declarator for type alias '" +
                                 result->getName() + "'.");

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseConstant() {
    // this uses a special case parsing function so that we don't have to do any
    // backtracking
    if (optional(KWD_CONST, true)) {
        Constant * result = new Constant();
        result->getContext().start(&currentContext);

        eat("const");

        result->getNameContext().start(&currentContext);
        result->setName(expect(IDENTIFIER, "constant name"));
        result->getNameContext().finish(&currentContext, &justCleanedContext);

        eat(":");

        MaybeASTNode m_initialization;
        ASTNode * initialization = nullptr;

        if (optional(ASSIGN)) {
            m_initialization = parseExpression();
            if (!m_initialization.assignTo(initialization))
                errornext(*this,
                          "Invalid expression in initialization of constant '" +
                              result->getName() + "'.");
            result->setInitialization(initialization);
        } else {
            MaybeASTNode m_declarator = parseDeclarator();
            ASTNode * declarator = nullptr;
            if (!m_declarator.assignTo(declarator))
                errornext(
                    *this,
                    "Expected type declarator in declaration of constant '" +
                        result->getName() + "'.");
            result->setTypeDeclarator(declarator);

            if (optional(ASSIGN)) {
                m_initialization = parseExpression();
                if (!m_initialization.assignTo(initialization))
                    errornext(
                        *this,
                        "Invalid expression in initialization of constant '" +
                            result->getName() + "'.");
                result->setInitialization(initialization);
            }
        }

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();

    // @const

    /*// this uses a special case parsing function so that we don't have to do
    any backtracking if (optional(CONSTANT_DECL_BEG, true)) { Constant * result
    = new Constant(); result->getContext().start(&currentContext);

        result->getNameContext().start(&currentContext);
        result->setName(expect(IDENTIFIER, "constant identifier"));
        result->getNameContext().finish(&currentContext, &justCleanedContext);

        eat("::");

        MaybeASTNode m_initialization = parseExpression();
        ASTNode * initialization = nullptr;
        if (!m_initialization.assignTo(initialization))
            errornext(*this, "Invalid expression in initialization of constant
    '" + result->getName() + "'."); result->setInitialization(initialization);

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
     */
}

MaybeASTNode Parser::parseVariableDeclaration() {
    // this uses a special case parsing function so that we don't have to do any
    // backtracking
    if (optional(VAR_DECL_BEG, true)) {
        VariableDeclaration * result = new VariableDeclaration();
        result->getContext().start(&currentContext);

        result->getNameContext().start(&currentContext);
        result->setName(expect(IDENTIFIER, "variable identifier"));
        result->getNameContext().finish(&currentContext, &justCleanedContext);

        eat(":");

        MaybeASTNode m_initialization;
        ASTNode * initialization = nullptr;

        if (optional(ASSIGN)) {
            m_initialization = parseExpression();
            if (!m_initialization.assignTo(initialization))
                errornext(*this,
                          "Invalid expression in initialization of variable '" +
                              result->getName() + "'.");
            result->setInitialization(initialization);
        } else {
            MaybeASTNode m_declarator = parseDeclarator();
            ASTNode * declarator = nullptr;
            if (!m_declarator.assignTo(declarator))
                errornext(
                    *this,
                    "Expected type declarator in declaration of variable '" +
                        result->getName() + "'.");
            result->setTypeDeclarator(declarator);

            if (optional(ASSIGN)) {
                m_initialization = parseExpression();
                if (!m_initialization.assignTo(initialization))
                    errornext(
                        *this,
                        "Invalid expression in initialization of variable '" +
                            result->getName() + "'.");
                result->setInitialization(initialization);
            }
        }

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseThis() {
    if (optional(KWD_THIS, true)) {
        This * result = new This;
        result->getContext().start(&currentContext);

        eat("this");

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseProc(bool parse_body) {
    if (optional(KWD_PROC, true)) {
        Procedure * result = new Procedure();
        result->getContext().start(&currentContext);
        TemplateProc * templateResult = new TemplateProc();
        templateResult->getContext().start(&currentContext);

        Context anonContext;
        anonContext.start(&currentContext);
        eat("proc");
        anonContext.finish(&currentContext, &justCleanedContext);

        result->getNameContext().start(&currentContext);

        MaybeString m_identifier = optional(IDENTIFIER);
        result->getNameContext().finish(&currentContext, &justCleanedContext);
        std::string identifier;
        if (!m_identifier.assignTo(identifier)) {
            result->getNameContext() = anonContext;
            identifier = compilation->frontEnd.makeUID("anon_p");
            MaybeASTNode m_badDef = parseTemplateDef();
            ASTNode * badDef = nullptr;
            if (m_badDef.assignTo(badDef))
                errorl(badDef->getContext(),
                       "Anonymous procedures cannot be templates.");
            result->setFlag(Procedure::IS_ANONYMOUS, true);
        }
        result->setName(identifier);

        MaybeASTNode m_templateDef = parseTemplateDef();
        ASTNode * templateDef = nullptr;
        m_templateDef.assignTo(
            templateDef); // don't care about the return value of this
        if (templateDef)
            templateResult->setTemplateDef(templateDef);

        expect(L_PAREN, result->getFlag(Procedure::IS_ANONYMOUS)
                            ? "identifier or '(' for an anonymous procedure"
                            : "'('");

        if (!optional(R_PAREN, true)) {
            while (true) {
                MaybeASTNode m_param = parseVariableDeclaration();
                ASTNode * param = nullptr;
                if (m_param.assignTo(param)) {
                    if (!((VariableDeclaration *)param)->getTypeDeclarator())
                        errorl(
                            param->getContext(),
                            "Procedure parameters must be explicitly typed.");
                    param->setFlag(VariableDeclaration::IS_PROC_PARAM, true);
                } else {
                    m_param = parseThis();
                    if (!m_param.assignTo(param))
                        errornext(*this,
                                  "Expected procedure parameter declaration.");
                }

                result->addParamVarDeclaration(param);
                if (!optional(COMMA))
                    break;
            }
        }
        expect(R_PAREN, "')'");

        if (optional(COLON)) {
            MaybeASTNode m_retDeclarator = parseDeclarator();
            ASTNode * retDeclarator = nullptr;
            if (!m_retDeclarator.assignTo(retDeclarator))
                errornext(*this, "Expected return type declarator for "
                                 "procedure declaration.");
            result->setRetDeclarator(retDeclarator);
        } else
            result->setRetDeclarator(newVoidDeclarator());

        MaybeASTNode m_statement;
        ASTNode * statement;
        if (optional(L_CURLY_BRACE, true)) {
            if (!parse_body)
                errornext(*this, "Did not expect procedure body.");

            eat("{");
            while (true) {
                statement = nullptr;
                m_statement = parseStatement();
                if (!m_statement.assignTo(statement))
                    break;
                result->addStatement(statement);
            }
            expect(R_CURLY_BRACE, "'}'");
        } else if (parse_body) {
            statement = nullptr;
            m_statement = parseStatement();
            if (!m_statement.assignTo(statement))
                errornext(*this, "Expected statement as body of '" +
                                     result->getName() + "'.");
            result->addStatement(statement);
        }

        result->getContext().finish(&currentContext, &justCleanedContext);
        templateResult->getContext().finish(&currentContext,
                                            &justCleanedContext);

        if (templateResult->getTemplateDef()) {
            templateResult->setTemplate(result);
            templateResult->setNameContext(result->getNameContext());
            return templateResult;
        }

        delete templateResult;

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseExternSig() {
    if (optional(KWD_EXTERN, true)) {
        Procedure * result = new Procedure();
        result->getContext().start(&currentContext);
        result->setFlag(Procedure::IS_EXTERN, true);

        eat("extern");

        result->getNameContext().start(&currentContext);

        result->setName(expect(IDENTIFIER, "external procedure identifier"));

        result->getNameContext().finish(&currentContext, &justCleanedContext);

        expect(L_PAREN, "'('");
        while (true) {
            if (optional(R_PAREN, true))
                break;
            if (optional(ELLIPSIS)) {
                result->setFlag(Procedure::IS_VARARG, true);
            } else {
                if (result->getFlag(Procedure::IS_VARARG))
                    errornext(*this, "An external procedure may only use "
                                     "variadic arguments ('...') as the last "
                                     "argument in the signature.");
                VariableDeclaration * param = new VariableDeclaration();
                param->setName(
                    compilation->frontEnd.makeUID(result->getName() + "_arg"));
                param->getContext().start(&currentContext);
                MaybeASTNode m_declarator = parseDeclarator();
                ASTNode * declarator = nullptr;
                if (!m_declarator.assignTo(declarator))
                    errornext(*this, "Expected type declarator for external "
                                     "procedure argument type.");
                param->setTypeDeclarator(declarator);
                param->getContext().finish(&currentContext,
                                           &justCleanedContext);
                result->setFlag(VariableDeclaration::IS_PROC_PARAM, true);
                result->addParamVarDeclaration(param);
            }
            if (!optional(COMMA))
                break;
        }
        expect(R_PAREN, "')'");

        if (optional(COLON)) {
            MaybeASTNode m_retDeclarator = parseDeclarator();
            ASTNode * retDeclarator = nullptr;
            if (!m_retDeclarator.assignTo(retDeclarator))
                errornext(*this, "Expected return type declarator for external "
                                 "procedure return type.");
            result->setRetDeclarator(retDeclarator);
        } else
            result->setRetDeclarator(newVoidDeclarator());

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseStatement() {
    MaybeASTNode m_statement;

    (m_statement = parseConstant()) ||
        (m_statement = parseVariableDeclaration()) ||
        (m_statement = parseExpression()) || (m_statement = parseBreak()) ||
        (m_statement = parseContinue()) || (m_statement = parseReturn()) ||
        (m_statement = parsePrint()) || (m_statement = parseIf()) ||
        (m_statement = parseFor()) || (m_statement = parseForeach()) ||
        (m_statement = parseWhile()) || (m_statement = parseDoWhile()) ||
        (m_statement = parseConstant()) || (m_statement = parseMatch());

    return m_statement;
}

MaybeASTNode Parser::parseIf() {
    if (optional(KWD_IF, true)) {
        If * result = new If();
        result->getContext().start(&currentContext);

        eat("if");

        MaybeASTNode m_conditional = parseExpression();
        ASTNode * conditional = nullptr;
        if (!m_conditional.assignTo(conditional))
            errornext(*this,
                      "Invalid conditional expression for 'if' statement.");
        result->setConditional(conditional);

        MaybeASTNode m_statement;
        ASTNode * statement;
        if (optional(L_CURLY_BRACE)) {
            while (true) {
                statement = nullptr;
                m_statement = parseStatement();
                if (!m_statement.assignTo(statement))
                    break;
                result->addStatement(statement);
            }
            expect(R_CURLY_BRACE, "'}'");
        } else {
            statement = nullptr;
            statement = nullptr;
            m_statement = parseStatement();
            if (!m_statement.assignTo(statement))
                errornext(*this,
                          "Expected statement as body in 'if' statement.");
            result->addStatement(statement);
        }

        result->getContext().finish(&currentContext, &justCleanedContext);

        MaybeASTNode m__else = parseElse();
        ASTNode * _else = nullptr;
        m__else.assignTo(_else); // don't care
        if (_else)
            result->setElse(_else);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseElse() {
    if (optional(KWD_ELSE, true)) {
        Else * result = new Else();
        result->getContext().start(&currentContext);

        eat("else");

        MaybeASTNode m_statement;
        ASTNode * statement;
        if (optional(L_CURLY_BRACE)) {
            while (true) {
                statement = nullptr;
                m_statement = parseStatement();
                if (!m_statement.assignTo(statement))
                    break;
                result->addStatement(statement);
            }
            expect(R_CURLY_BRACE, "'}'");
        } else {
            statement = nullptr;
            statement = nullptr;
            m_statement = parseStatement();
            if (!m_statement.assignTo(statement))
                errornext(*this,
                          "Expected statement as body in 'else' statement.");
            result->addStatement(statement);
        }

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseFor() {
    if (optional(KWD_FOR, true)) {
        For * result = new For();
        result->getContext().start(&currentContext);

        eat("for");

        MaybeASTNode m_initialization;
        ASTNode * initialization;
        while (!optional(SEMICOLON, true)) {
            initialization = nullptr;
            (m_initialization = parseVariableDeclaration()) ||
                (m_initialization = parseExpression());
            if (!m_initialization.assignTo(initialization))
                errornext(*this, "Invalid initialization in 'for' loop.");
            result->addInitialization(initialization);
            if (!optional(COMMA))
                break;
        }

        expect(SEMICOLON, "';'");

        MaybeASTNode m_conditional = parseExpression();
        ASTNode * conditional = nullptr;
        if (!m_conditional.assignTo(conditional))
            errornext(*this, "Invalid conditional expression for 'for' loop.");
        result->setConditional(conditional);

        expect(SEMICOLON, "';'");

        MaybeASTNode m_afterthought;
        ASTNode * afterthought;
        while (!optional(SEMICOLON, true)) {
            afterthought = nullptr;
            m_afterthought = parseExpression();
            if (!m_afterthought.assignTo(afterthought))
                errornext(*this, "Invalid expression in 'for' loop.");
            result->addAfterthought(afterthought);
            if (!optional(COMMA))
                break;
        }

        MaybeASTNode m_statement;
        ASTNode * statement;
        if (optional(L_CURLY_BRACE)) {
            while (true) {
                statement = nullptr;
                m_statement = parseStatement();
                if (!m_statement.assignTo(statement))
                    break;
                result->addStatement(statement);
            }
            expect(R_CURLY_BRACE, "'}'");
        } else {
            statement = nullptr;
            statement = nullptr;
            m_statement = parseStatement();
            if (!m_statement.assignTo(statement))
                errornext(*this, "Expected statement as body in 'for' loop.");
            result->addStatement(statement);
        }

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseForeach() {
    if (optional(KWD_FOREACH, true)) {
        Foreach * result = new Foreach();
        result->getContext().start(&currentContext);

        eat("foreach");

        if (optional(KWD_REF))
            result->setFlag(Foreach::TAKE_REF, true);

        Context identContext;
        identContext.start(&currentContext);
        std::string ident = expect(IDENTIFIER, "identifier");
        identContext.finish(&currentContext, &justCleanedContext);

        result->setIdent(ident);
        result->setIdentContext(identContext);

        expect(KWD_IN, "'in'");

        MaybeASTNode m_expression;
        ASTNode * expression = nullptr;
        m_expression = parseExpression();
        if (!m_expression.assignTo(expression))
            if (!m_expression.assignTo(expression))
                errornext(*this, "Invalid expression in 'foreach' loop.");
        result->setExpression(expression);

        MaybeASTNode m_statement;
        ASTNode * statement;
        if (optional(L_CURLY_BRACE)) {
            while (true) {
                statement = nullptr;
                m_statement = parseStatement();
                if (!m_statement.assignTo(statement))
                    break;
                result->addStatement(statement);
            }
            expect(R_CURLY_BRACE, "'}'");
        } else {
            statement = nullptr;
            statement = nullptr;
            m_statement = parseStatement();
            if (!m_statement.assignTo(statement))
                errornext(*this,
                          "Expected statement as body in 'foreach' loop.");
            result->addStatement(statement);
        }

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseWhile() {
    if (optional(KWD_WHILE, true)) {
        While * result = new While();
        result->getContext().start(&currentContext);

        eat("while");

        MaybeASTNode m_conditional = parseExpression();
        ASTNode * conditional = nullptr;
        if (!m_conditional.assignTo(conditional))
            errornext(*this,
                      "Invalid conditional expression for 'while' loop.");
        result->setConditional(conditional);

        MaybeASTNode m_statement;
        ASTNode * statement;
        if (optional(L_CURLY_BRACE)) {
            while (true) {
                statement = nullptr;
                m_statement = parseStatement();
                if (!m_statement.assignTo(statement))
                    break;
                result->addStatement(statement);
            }
            expect(R_CURLY_BRACE, "'}'");
        } else {
            statement = nullptr;
            statement = nullptr;
            m_statement = parseStatement();
            if (!m_statement.assignTo(statement))
                errornext(*this, "Expected statement as body in 'while' loop.");
            result->addStatement(statement);
        }

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseDoWhile() {
    if (optional(KWD_DO, true)) {
        DoWhile * result = new DoWhile();
        result->getContext().start(&currentContext);

        eat("do");

        MaybeASTNode m_statement;
        ASTNode * statement;
        if (optional(L_CURLY_BRACE)) {
            while (true) {
                statement = nullptr;
                m_statement = parseStatement();
                if (!m_statement.assignTo(statement))
                    break;
                result->addStatement(statement);
            }
            expect(R_CURLY_BRACE, "'}'");
        } else {
            statement = nullptr;
            statement = nullptr;
            m_statement = parseStatement();
            if (!m_statement.assignTo(statement))
                errornext(*this, "Expected statement as body in 'while' loop.");
            result->addStatement(statement);
        }

        expect(KWD_WHILE, "'while'");

        MaybeASTNode m_conditional = parseExpression();
        ASTNode * conditional = nullptr;
        if (!m_conditional.assignTo(conditional))
            errornext(*this,
                      "Invalid conditional expression for 'while' loop.");
        result->setConditional(conditional);

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseMatch() {
    if (optional(KWD_MATCH, true)) {
        Match * result = new Match();
        result->getContext().start(&currentContext);

        eat("match");

        MaybeASTNode m_expression = parseExpression();
        ASTNode * expression = nullptr;
        if (!m_expression.assignTo(expression))
            errornext(*this, "Invalid expression for 'match'.");
        result->setExpression(expression);

        expect(L_CURLY_BRACE, "'{'");
        MaybeASTNode m_with;
        ASTNode * with;
        while (true) {
            with = nullptr;
            m_with = parseWith();
            if (!m_with.assignTo(with))
                break;
            result->addWith(with);
        }
        expect(R_CURLY_BRACE, "'}'");

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseWith() {
    if (optional(KWD_WITH, true)) {
        With * result = new With;
        result->getContext().start(&currentContext);
        result->setFlag(With::WITH_ELSE, true);

        eat("with");

        if (!optional(KWD_ELSE)) {
            result->setFlag(With::WITH_ELSE, false);
            MaybeASTNode m_expression = parseExpression();
            ASTNode * expression = nullptr;
            if (!m_expression.assignTo(expression))
                errornext(*this, "Invalid expression following 'with'.");
            result->setExpression(expression);
        }

        expect(COLON, "':'");

        MaybeASTNode m_statement;
        ASTNode * statement;
        if (optional(L_CURLY_BRACE)) {
            while (true) {
                statement = nullptr;
                m_statement = parseStatement();
                if (!m_statement.assignTo(statement))
                    break;
                result->addStatement(statement);
            }
            expect(R_CURLY_BRACE, "'}'");
        } else {
            statement = nullptr;
            statement = nullptr;
            m_statement = parseStatement();
            if (!m_statement.assignTo(statement))
                errornext(*this, "Expected statement as body in 'while' loop.");
            result->addStatement(statement);
        }

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parsePrint() {
    if (optional(KWD_PRINT, true)) {
        Print * result = new Print();
        result->getContext().start(&currentContext);

        eat("print");

        MaybeASTNode m_args = parseArgList();
        ASTNode * args = nullptr;
        if (!m_args.assignTo(args))
            errornext(*this, "Expected argument list for 'print'.");
        result->setArgs(args);

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseReturn() {
    if (optional(KWD_RETURN, true)) {
        Return * result = new Return();
        result->getContext().start(&currentContext);

        eat("return", true);

        if (buff.viewSize() > 0 && !optional(END_OF_LINE)) {
            clean();
            MaybeASTNode m_expression = parseExpression();
            ASTNode * expression = nullptr;
            if (!m_expression.assignTo(expression))
                errornext(*this, "Invalid expression in return statement.",
                          true,
                          "Note: returning without a value must not have "
                          "anything following 'return' on the same line");
            result->setExpression(expression);
            result->getContext().finish(&currentContext, &justCleanedContext);
        } else {
            result->getContext().finish(&currentContext, &justCleanedContext);
            result->getContext().end.character -= 1; // acount for the newline
            clean();
        }

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseBreak() {
    if (optional(KWD_BREAK, true)) {
        Break * result = new Break();
        result->getContext().start(&currentContext);

        eat("break");

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseContinue() {
    if (optional(KWD_CONTINUE, true)) {
        Continue * result = new Continue();
        result->getContext().start(&currentContext);

        eat("continue");

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

MaybeASTNode Parser::parseMacroUse() {
    if (optional(BACK_SLASH, true)) {
        MacroUse * result = new MacroUse();
        result->getContext().start(&currentContext);

        eat("\\");

        result->setMacroName(expect(IDENTIFIER, "macro name"));

        expect(L_CURLY_BRACE, "'{'");
        while (true) {
            ASTNode * arg = nullptr;
            MaybeASTNode m_arg = parseMacroArg();
            if (!m_arg.assignTo(arg))
                break;
            result->addArg(arg);

            // if (!optional(COMMA))
            // break;
        }
        expect(R_CURLY_BRACE, "'}'");

        result->getContext().finish(&currentContext, &justCleanedContext);

        return MaybeASTNode(result);
    }

    return MaybeASTNode();
}

MaybeASTNode Parser::parseMacroArg() {
    MaybeASTNode m_node;
    (m_node = parseTopLevelNode()) || (m_node = parseDeclarator());
    return m_node;
}

MaybeASTNode Parser::parseSLComment() {
    if (optional(HASH, true, true)) {
        Context saveJustCleaned = justCleanedContext;
        SLComment * result = new SLComment();
        eat("#", true);
        const char * newlineIt = std::find(buff.begin(), buff.end(), '\n');
        std::string contents;
        if (newlineIt == buff.end())
            contents = buff;
        else
            contents = buff.substr(0, newlineIt - buff.begin());
        eat(contents, true);
        result->setContents(contents);
        justCleanedContext = saveJustCleaned;
        return MaybeASTNode(result);
    }
    return MaybeASTNode();
}

void Parser::moduleCheck(std::vector<ASTNode *> & AST,
                         ModuleDeclaration * module_declared,
                         ModuleDeclaration * mod_decl) {
    if (module_declared) {
        errorl(mod_decl->getContext(),
               "This compilation unit has already been declared as a module.",
               false);
        errorl(module_declared->getContext(),
               "Previous module declaration here.");
    }

    if (!AST.empty() && mod_decl != AST.front())
        errorl(mod_decl->getContext(), "Module declarations must be at the "
                                       "beginning of the file/compilation "
                                       "unit.");

    module_declared = mod_decl;
}

AsyncParser::AsyncParser(const char * c_str) : Parser(c_str, false) {}
AsyncParser::AsyncParser(std::string & str) : Parser(str, false) {}
AsyncParser::AsyncParser(std::ifstream & file, const std::string & fname)
    : Parser(file, fname, false) {}

void AsyncParser::parseCommon() {
    static ModuleDeclaration * module_declared = nullptr;

    while (buff.viewSize() > 0) {
        clean();
        MaybeASTNode m_node = parseTopLevelNode();
        ASTNode * node = nullptr;
        if (!m_node.assignTo(node))
            errornext(*this, "Unexpected token.");

        else if (node->nodeKind == ASTNode::MODULE_DECL) {
            moduleCheck(nodes, module_declared, (ModuleDeclaration *)node);
            module_declared = (ModuleDeclaration *)node;
        }

        node->replace = rpget<replacementPolicy_Global_Node>();

        nodes.push_back(node);
    }
}

milliseconds AsyncParser::operator()() {
    auto start = Clock::now();
    parseCommon();
    return duration_cast<milliseconds>(Clock::now() - start);
}

ImportParser::ImportParser(const char * c_str)
    : Parser(c_str, false), mod_decl(nullptr) {}
ImportParser::ImportParser(std::string & str)
    : Parser(str, false), mod_decl(nullptr) {}
ImportParser::ImportParser(std::ifstream & file, const std::string & fname)
    : Parser(file, fname, false), mod_decl(nullptr) {}

void ImportParser::parseCommon() {
    while (buff.viewSize() > 0) {
        clean();
        MaybeASTNode m_node = parseTopLevelNode();
        ASTNode * node = nullptr;
        if (!m_node.assignTo(node))
            errornext(*this, "Unexpected token.");
        if (node->nodeKind == ASTNode::MODULE_DECL) {
            bool once = (bool)mod_decl;

            moduleCheck(nodes, mod_decl, (ModuleDeclaration *)node);
            mod_decl = (ModuleDeclaration *)node;
            // Notice that we DON'T push the module decl into 'nodes' here

            if (!once) {
                once = true;
                return;
            }
        }

        node->replace = rpget<replacementPolicy_Global_Node>();

        nodes.push_back(node);
    }
}

void ImportParser::Dispose() {
    for (ASTNode * node : nodes)
        delete node;
}

milliseconds ImportParser::operator()() {
    auto start = Clock::now();
    parseCommon();
    return duration_cast<milliseconds>(Clock::now() - start);
}
} // namespace bjou
