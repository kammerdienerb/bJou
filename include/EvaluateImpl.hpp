//
//  EvaluateImpl.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 4/29/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef EvaluateImpl_h
#define EvaluateImpl_h

#include "Evaluate.hpp"
#include "ASTNode.hpp"
#include "Type.hpp"

namespace bjou {
    template <typename rT>
    rT _evalAdd(Val& a, Val& b) {
        if (a.t->enumerableEquivalent()) {
            if (b.t->enumerableEquivalent())
                return a.as_i64 + b.as_i64;
            if (b.t->isFP())
                return a.as_i64 + b.as_f64;
        } else if (a.t->isFP()) {
            if (b.t->isFP())
                return a.as_f64 + b.as_f64;
            if (b.t->enumerableEquivalent())
                return a.as_f64 + b.as_i64;
        }
        internalError("Bad types in evaluation of add expression.");
        return { };
    }
    
    template <typename rT>
    rT _evalSub(Val& a, Val& b) {
        if (a.t->enumerableEquivalent()) {
            if (b.t->enumerableEquivalent())
                return a.as_i64 - b.as_i64;
            if (b.t->isFP())
                return a.as_i64 - b.as_f64;
        } else if (a.t->isFP()) {
            if (b.t->isFP())
                return a.as_f64 - b.as_f64;
            if (b.t->enumerableEquivalent())
                return a.as_f64 - b.as_i64;
        }
        
        internalError("Bad types in evaluation of sub expression.");
        return { };
    }
    
    template <typename rT>
    rT _evalMult(Val& a, Val& b) {
        if (a.t->enumerableEquivalent()) {
            if (b.t->enumerableEquivalent())
                return a.as_i64 * b.as_i64;
            if (b.t->isFP())
                return a.as_i64 * b.as_f64;
        } else if (a.t->isFP()) {
            if (b.t->isFP())
                return a.as_f64 * b.as_f64;
            if (b.t->enumerableEquivalent())
                return a.as_f64 * b.as_i64;
        }
        
        internalError("Bad types in evaluation of mult expression.");
        return { };
    }
   
    template <typename rT>
    rT _evalDiv(Val& a, Val& b) {
        if (a.t->enumerableEquivalent()) {
            if (b.t->enumerableEquivalent())
                return a.as_i64 / b.as_i64;
            if (b.t->isFP())
                return a.as_i64 / b.as_f64;
        } else if (a.t->isFP()) {
            if (b.t->isFP())
                return a.as_f64 / b.as_f64;
            if (b.t->enumerableEquivalent())
                return a.as_f64 / b.as_i64;
        }
        
        internalError("Bad types in evaluation of div expression.");
        return { };
    }
    
    template <typename rT>
    rT _evalMod(Val& a, Val& b) {
        if (a.t->enumerableEquivalent()) {
            if (b.t->enumerableEquivalent())
                return a.as_i64 % b.as_i64;
        }
        
        internalError("Bad types in evaluation of mod expression.");
        return { };
    }
}

#endif /* EvaluateImpl_h */
