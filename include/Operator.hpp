//
//  Operator.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/8/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Operator_h
#define Operator_h

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
    {"as", 10, true, LEFT},       {"?", 9, true, LEFT},
    {"()", 9, false, LEFT},       {"[]", 9, false, LEFT},
    {".", 9, false, LEFT},        {"->", 9, false, LEFT},
    {"!", 7, true, RIGHT},        {"not", 7, true, RIGHT},
    {"sizeof", 7, true, RIGHT},   {"&", 7, true, RIGHT},
    {"@", 7, true, RIGHT},        {"new", 7, true, RIGHT},
    {"proc", 7, true, RIGHT},     {"extern", 7, true, RIGHT},
    {"operator", 7, true, RIGHT}, {"some", 7, true, RIGHT},
    {"*", 6, false, LEFT},        {"/", 6, false, LEFT},
    {"%", 6, false, LEFT},        {"+", 5, false, LEFT},
    {"-", 5, false, LEFT},        {"<", 4, false, LEFT},
    {"<=", 4, false, LEFT},       {">", 4, false, LEFT},
    {">=", 4, false, LEFT},       {"==", 3, false, LEFT},
    {"!=", 3, false, LEFT},       {"&&", 2, false, LEFT},
    {"and", 2, false, LEFT},      {"||", 2, false, LEFT},
    {"or", 2, false, LEFT},       {"??", 1, false, RIGHT},
    {"=", 1, false, RIGHT},       {"*=", 1, false, RIGHT},
    {"/=", 1, false, RIGHT},      {"%=", 1, false, RIGHT},
    {"+=", 1, false, RIGHT},      {"-=", 1, false, RIGHT}};

bool same(const char * x, const char * y);

const OpInfo * getOpInfo(const char * op, const OpInfo * info);

int precedence(const char * op);

bool unary(const char * op);

bool binary(const char * op);

bool rightAssoc(const char * op);

bool leftAssoc(const char * op);
} // namespace bjou

#endif /* Operator_h */
