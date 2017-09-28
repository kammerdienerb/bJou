//
//  Evaluate.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 4/29/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "Evaluate.hpp"
#include "EvaluateImpl.hpp"
#include "ASTNode.hpp"
#include "Compile.hpp"
#include "FrontEnd.hpp"

#include <string>

namespace bjou {
    ASTNode * Val::toExpr() {
        if (t->enumerableEquivalent()) {
            if (t->size == 1) {
                BooleanLiteral * b = new BooleanLiteral;
                b->setContents(as_i64 ? "true" : "false");
                return b;
            } else {
                IntegerLiteral * i = new IntegerLiteral;
                i->setContents(std::to_string(as_i64));
                return i;
            }
        } else if (t->isFP()) {
            FloatLiteral * f = new FloatLiteral;
            f->setContents(std::to_string(as_f64));
            return f;
        } else if (t->isPointer()) {
            PointerType * pt = (PointerType*)t;
            if (pt->pointer_of->size == 8) {
                StringLiteral * s = new StringLiteral;
                s->setContents(as_string);
                return s;
            } else internalError("Could not convert Val to Expression.");
        } else if (t->isStruct()) {
            // @incomplete
        } else internalError("Could not convert Val to Expression.");
        return nullptr;
    }

    Val evalAdd(Val& a, Val& b, const Type * t) {
        Val result;
        result.t = t;
        
        if (t->enumerableEquivalent()) {
            result.as_i64 = _evalAdd<int64_t>(a, b);
        } else if (t->isFP()) {
            result.as_f64 = _evalAdd<double>(a, b);
        } else internalError("Could not evaluate add expression.");
        
        return result;
    }
    
    Val evalSub(Val& a, Val& b, const Type * t) {
        Val result;
        result.t = t;
        
        if (t->enumerableEquivalent()) {
            result.as_i64 = _evalSub<int64_t>(a, b);
        } else if (t->isFP()) {
            result.as_f64 = _evalSub<double>(a, b);
        } else internalError("Could not evaluate sub expression.");
        
        return result;
    }
    
    Val evalMult(Val& a, Val& b, const Type * t) {
        Val result;
        result.t = t;
        
        if (t->enumerableEquivalent()) {
            result.as_i64 = _evalMult<int64_t>(a, b);
        } else if (t->isFP()) {
            result.as_f64 = _evalMult<double>(a, b);
        } else internalError("Could not evaluate mult expression.");
        
        return result;
    }
    
    Val evalDiv(Val& a, Val& b, const Type * t) {
        Val result;
        result.t = t;
        
        if (t->enumerableEquivalent()) {
            result.as_i64 = _evalDiv<int64_t>(a, b);
        } else if (t->isFP()) {
            result.as_f64 = _evalDiv<double>(a, b);
        } else internalError("Could not evaluate div expression.");
        
        return result;
    }
    
    Val evalMod(Val& a, Val& b, const Type * t) {
        Val result;
        result.t = t;
        
        if (t->enumerableEquivalent()) {
            result.as_i64 = _evalMod<int64_t>(a, b);
        } else internalError("Could not evaluate Mod expression.");
        
        return result;
    }
}
