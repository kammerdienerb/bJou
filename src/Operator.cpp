//
//  Operator.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/11/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "Operator.hpp"

namespace bjou {
    bool same(const char * x, const char * y) {
        return !*x && !*y ? true : (*x == *y && same(x + 1, y + 1));
    }
    
    const OpInfo * getOpInfo(const char * op, const OpInfo * info) {
        return same(op, info->op) ? info : getOpInfo(op, info + 1);
    }
    
    int precedence(const char * op) {
        return getOpInfo(op, OpMap)->precedence;
    }
    
    bool unary(const char * op) {
        return getOpInfo(op, OpMap)->unary;
    }
    
    bool binary(const char * op) {
        return !getOpInfo(op, OpMap)->unary;
    }
    
    bool rightAssoc(const char * op) {
        return getOpInfo(op, OpMap)->associativity == RIGHT;
    }
    
    bool leftAssoc(const char * op) {
        return getOpInfo(op, OpMap)->associativity == LEFT;
    }
}