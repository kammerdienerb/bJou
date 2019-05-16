//
//  Evaluate.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 4/29/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "Evaluate.hpp"
#include "ASTNode.hpp"
#include "Compile.hpp"
#include "EvaluateImpl.hpp"
#include "FrontEnd.hpp"

#include <string>
#include <sstream>

namespace bjou {
ASTNode * Val::toExpr() {
    if (t->isBool()) {
        BooleanLiteral * b = new BooleanLiteral;
        b->setContents(as_i64 ? "true" : "false");
        return b;
    } else if (t->isInt()) {
        IntType * int_t = (IntType*)t;
        IntegerLiteral * i = new IntegerLiteral;
        if (int_t->sign == Type::Sign::SIGNED) {
            i->setContents(std::to_string(as_i64));
            i->getContents() += "i";
        } else {
            i->setContents(std::to_string((uint64_t)as_i64));
            i->getContents() += "u";
        }
        i->getContents() += std::to_string(int_t->width);
        return i;
    } else if (t->isEnum()) {
        IntegerLiteral * i = new IntegerLiteral;
        i->setContents(std::to_string(as_i64) + "u64");
        return i;
    } else if (t->isFloat()) {
        FloatLiteral * f = new FloatLiteral;
        f->setContents(std::to_string(as_f64));
        return f;
    } else if (t->isPointer()) {
        PointerType * pt = (PointerType *)t;
        const Type * u = pt->under();
        if (u->isChar()) {
            StringLiteral * s = new StringLiteral;
            s->setContents(as_string);
            return s;
        } else {
            std::stringstream ss;
            ss << std::hex << *((uintptr_t*)&as_i64);
            IntegerLiteral * i = new IntegerLiteral;
            i->setContents(ss.str());
            AsExpression * as = new AsExpression;
            as->setContents("as");
            as->setLeft(i);
            as->setRight(t->getGenericDeclarator());
            return as;
        }
    } else if (t->isStruct()) {
        // @incomplete
    } else
        internalError("Could not convert Val to Expression.");
    return nullptr;
}

Val evalAdd(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt() || t->isBool()) {
        result.as_i64 = _evalAdd<int64_t>(a, b);
    } else if (t->isFloat()) {
        result.as_f64 = _evalAdd<double>(a, b);
    } else
        internalError("Could not evaluate add expression.");

    return result;
}

Val evalSub(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt() || t->isBool()) {
        result.as_i64 = _evalSub<int64_t>(a, b);
    } else if (t->isFloat()) {
        result.as_f64 = _evalSub<double>(a, b);
    } else
        internalError("Could not evaluate sub expression.");

    return result;
}

Val evalBSHL(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt())
        result.as_i64 = _evalBSHL<int64_t>(a, b);
    else
        internalError("Could not evaluate BSHL expression.");

    return result;
}

Val evalBSHR(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt())
        result.as_i64 = _evalBSHR<int64_t>(a, b);
    else
        internalError("Could not evaluate BSHR expression.");

    return result;
}

Val evalMult(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt() || t->isBool()) {
        result.as_i64 = _evalMult<int64_t>(a, b);
    } else if (t->isFloat()) {
        result.as_f64 = _evalMult<double>(a, b);
    } else
        internalError("Could not evaluate mult expression.");

    return result;
}

Val evalDiv(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt() || t->isBool()) {
        result.as_i64 = _evalDiv<int64_t>(a, b);
    } else if (t->isFloat()) {
        result.as_f64 = _evalDiv<double>(a, b);
    } else
        internalError("Could not evaluate div expression.");

    return result;
}

Val evalMod(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt() || t->isBool()) {
        result.as_i64 = _evalMod<int64_t>(a, b);
    } else
        internalError("Could not evaluate mod expression.");

    return result;
}

Val evalNot(Val & a, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt() || t->isBool()) {
        result.as_i64 = _evalNot<int64_t>(a);
    } else if (t->isFloat()) {
        result.as_f64 = _evalNot<double>(a);
    } else
        internalError("Could not evaluate not expression.");

    return result;
}

Val evalBNEG(Val & a, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt())
        result.as_i64 = _evalBNEG<int64_t>(a);
    else
        internalError("Could not evaluate bneg expression.");

    return result;
}

Val evalEqu(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt() || t->isBool()) {
        result.as_i64 = _evalEqu<int64_t>(a, b);
    } else if (t->isFloat()) {
        result.as_f64 = _evalEqu<double>(a, b);
    } else
        internalError("Could not evaluate equ expression.");

    return result;
}

Val evalNeq(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt() || t->isBool()) {
        result.as_i64 = _evalNeq<int64_t>(a, b);
    } else if (t->isFloat()) {
        result.as_f64 = _evalNeq<double>(a, b);
    } else
        internalError("Could not evaluate neq expression.");

    return result;
}

Val evalLogAnd(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt() || t->isBool()) {
        result.as_i64 = _evalLogAnd<int64_t>(a, b);
    } else if (t->isFloat()) {
        result.as_f64 = _evalLogAnd<double>(a, b);
    } else
        internalError("Could not evaluate log and expression.");

    return result;
}

Val evalLogOr(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt() || t->isBool()) {
        result.as_i64 = _evalLogOr<int64_t>(a, b);
    } else if (t->isFloat()) {
        result.as_f64 = _evalLogOr<double>(a, b);
    } else
        internalError("Could not evaluate log or expression.");

    return result;
}

Val evalBAND(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt())
        result.as_i64 = _evalBAND<int64_t>(a, b);
    else
        internalError("Could not evaluate band expression.");

    return result;
}

Val evalBOR(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt())
        result.as_i64 = _evalBOR<int64_t>(a, b);
    else
        internalError("Could not evaluate bor expression.");

    return result;
}

Val evalBXOR(Val & a, Val & b, const Type * t) {
    Val result;
    result.t = t;

    if (t->isInt())
        result.as_i64 = _evalBXOR<int64_t>(a, b);
    else
        internalError("Could not evaluate bxor expression.");

    return result;
}

} // namespace bjou
