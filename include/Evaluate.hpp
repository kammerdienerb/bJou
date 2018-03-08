//
//  Evaluate.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 4/29/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Evaluate_hpp
#define Evaluate_hpp

#include "CLI.hpp"
#include "Misc.hpp"

#include <vector>

namespace bjou {
struct Type;
struct ASTNode;

struct StructVal {
    std::vector<struct Val> vals;
};

struct Val {
    const Type * t;

    union {
        int64_t as_i64;
        double as_f64;
    };

    std::string as_string;
    StructVal as_struct;

    ASTNode * toExpr();
};

Val evalAdd(Val & a, Val & b, const Type * t);
Val evalSub(Val & a, Val & b, const Type * t);
Val evalMult(Val & a, Val & b, const Type * t);
Val evalDiv(Val & a, Val & b, const Type * t);
Val evalMod(Val & a, Val & b, const Type * t);
Val evalNot(Val & a, const Type * t);
Val evalEqu(Val & a, Val & b, const Type * t);
Val evalNeq(Val & a, Val & b, const Type * t);
Val evalLogAnd(Val & a, Val & b, const Type * t);
Val evalLogOr(Val & a, Val & b, const Type * t);

} // namespace bjou

#endif /* Evaluate_hpp */
