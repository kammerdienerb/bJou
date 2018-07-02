//
//  Operator.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/8/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Operator_h
#define Operator_h

#include "Misc.hpp"

#include <string>

namespace bjou {
enum ASSOCIATIVITY { LEFT, RIGHT };

struct OpInfo {
    const char * op;
    int precedence;
    bool unary;
    ASSOCIATIVITY associativity;
};

const OpInfo OpMap[] = {
    //  char *   int     bool   ASSOCIATIVITY
    //  op    precedence unary  associativity
    {"as", 13, true, LEFT},

#define OP_PREC_BIN_HIGH 12

    {"?", 12, true, LEFT},       {"()", 12, false, LEFT},
    {"[]", 12, false, LEFT},     {".", 12, false, LEFT},
    {"->", 12, false, LEFT},

#define OP_PREC_UN_PRE 11

    {"!", 11, true, RIGHT},      {"not", 11, true, RIGHT},
    {"sizeof", 11, true, RIGHT}, {"&", 11, true, RIGHT},
    {"~", 11, true, RIGHT},      {"@", 11, true, RIGHT},
    {"new", 11, true, RIGHT},    {"proc", 11, true, RIGHT},
    {"extern", 11, true, RIGHT}, {"operator", 11, true, RIGHT},
    {"some", 11, true, RIGHT},   {"bneg", 11, true, RIGHT},

    {"*", 10, false, LEFT},      {"/", 10, false, LEFT},
    {"%", 10, false, LEFT},

    {"+", 9, false, LEFT},       {"-", 9, false, LEFT},

    {"bshl", 8, false, LEFT},    {"bshr", 8, false, LEFT},

    {"<", 7, false, LEFT},       {"<=", 7, false, LEFT},
    {">", 7, false, LEFT},       {">=", 7, false, LEFT},

    {"==", 6, false, LEFT},      {"!=", 6, false, LEFT},

    {"band", 5, false, LEFT},

    {"bxor", 4, false, LEFT},

    {"bor", 3, false, LEFT},

    {"&&", 2, false, LEFT},      {"and", 2, false, LEFT},
    {"||", 2, false, LEFT},      {"or", 2, false, LEFT},

    {"??", 1, false, RIGHT},     {"=", 1, false, RIGHT},
    {"*=", 1, false, RIGHT},     {"/=", 1, false, RIGHT},
    {"%=", 1, false, RIGHT},     {"+=", 1, false, RIGHT},
    {"-=", 1, false, RIGHT}};

bool same(const char * x, const char * y);

const OpInfo * getOpInfo(const char * op, const OpInfo * info);

int precedence(const char * op);

bool unary(const char * op);

bool binary(const char * op);

bool rightAssoc(const char * op);

bool leftAssoc(const char * op);

bool isAssignableOp(std::string & op);
bool isAssignmentOp(std::string & op);

} // namespace bjou

#endif /* Operator_h */
