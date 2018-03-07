//
//  EvaluateImpl.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 4/29/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef EvaluateImpl_h
#define EvaluateImpl_h

#include "ASTNode.hpp"
#include "Evaluate.hpp"
#include "Type.hpp"

namespace bjou {
template <typename rT> rT _evalAdd(Val & a, Val & b) {
    if (a.t->isInt() || a.t->isBool()) {
        if (b.t->isInt() || b.t->isBool())
            return a.as_i64 + b.as_i64;
        if (b.t->isFloat())
            return a.as_i64 + b.as_f64;
    } else if (a.t->isFloat()) {
        if (b.t->isFloat())
            return a.as_f64 + b.as_f64;
        if (b.t->isInt() || b.t->isBool())
            return a.as_f64 + b.as_i64;
    }
    internalError("Bad types in evaluation of add expression.");
    return {};
}

template <typename rT> rT _evalSub(Val & a, Val & b) {
    if (a.t->isInt() || a.t->isBool()) {
        if (b.t->isInt() || b.t->isBool())
            return a.as_i64 - b.as_i64;
        if (b.t->isFloat())
            return a.as_i64 - b.as_f64;
    } else if (a.t->isFloat()) {
        if (b.t->isFloat())
            return a.as_f64 - b.as_f64;
        if (b.t->isInt() || b.t->isBool())
            return a.as_f64 - b.as_i64;
    }

    internalError("Bad types in evaluation of sub expression.");
    return {};
}

template <typename rT> rT _evalMult(Val & a, Val & b) {
    if (a.t->isInt() || a.t->isBool()) {
        if (b.t->isInt() || b.t->isBool())
            return a.as_i64 * b.as_i64;
        if (b.t->isFloat())
            return a.as_i64 * b.as_f64;
    } else if (a.t->isFloat()) {
        if (b.t->isFloat())
            return a.as_f64 * b.as_f64;
        if (b.t->isInt() || b.t->isBool())
            return a.as_f64 * b.as_i64;
    }

    internalError("Bad types in evaluation of mult expression.");
    return {};
}

template <typename rT> rT _evalDiv(Val & a, Val & b) {
    if (a.t->isInt() || a.t->isBool()) {
        if (b.t->isInt() || b.t->isBool())
            return a.as_i64 / b.as_i64;
        if (b.t->isFloat())
            return a.as_i64 / b.as_f64;
    } else if (a.t->isFloat()) {
        if (b.t->isFloat())
            return a.as_f64 / b.as_f64;
        if (b.t->isInt() || b.t->isBool())
            return a.as_f64 / b.as_i64;
    }

    internalError("Bad types in evaluation of div expression.");
    return {};
}

template <typename rT> rT _evalMod(Val & a, Val & b) {
    if (a.t->isInt() || a.t->isBool()) {
        if (b.t->isInt() || b.t->isBool())
            return a.as_i64 % b.as_i64;
    }

    internalError("Bad types in evaluation of mod expression.");
    return {};
}

template <typename rT> rT _evalNot(Val & a) {
    if (a.t->isInt() || a.t->isBool()) {
        return !a.as_i64;
    } else if (a.t->isFloat()) {
        return !a.as_f64;
    }
    internalError("Bad types in evaluation of add expression.");
    return {};
}

template <typename rT> rT _evalEqu(Val & a, Val & b) {
    if (a.t->isInt() || a.t->isBool()) {
        if (b.t->isInt() || b.t->isBool())
            return a.as_i64 == b.as_i64;
        if (b.t->isFloat())
            return a.as_i64 == b.as_f64;
    } else if (a.t->isFloat()) {
        if (b.t->isFloat())
            return a.as_f64 == b.as_f64;
        if (b.t->isInt() || b.t->isBool())
            return a.as_f64 == b.as_i64;
    }

    internalError("Bad types in evaluation of equ expression.");
    return {};
}

template <typename rT> rT _evalNeq(Val & a, Val & b) {
    if (a.t->isInt() || a.t->isBool()) {
        if (b.t->isInt() || b.t->isBool())
            return a.as_i64 != b.as_i64;
        if (b.t->isFloat())
            return a.as_i64 != b.as_f64;
    } else if (a.t->isFloat()) {
        if (b.t->isFloat())
            return a.as_f64 != b.as_f64;
        if (b.t->isInt() || b.t->isBool())
            return a.as_f64 != b.as_i64;
    }

    internalError("Bad types in evaluation of neq expression.");
    return {};
}
} // namespace bjou

#endif /* EvaluateImpl_h */
