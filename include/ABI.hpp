//
//  ABI.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 2/7/18.
//  Copyright © 2018 me. All rights reserved.
//

#ifndef ABI_hpp
#define ABI_hpp

#include "ASTNode.hpp"
#include "BackEnd.hpp"

#include <vector>

namespace bjou {
template <typename BackEnd_T> struct x86Lowerer;

template <typename BackEnd_T> struct ABILowerer {
    static ABILowerer * get(BackEnd_T & backEnd) {
#if defined(__i386) || defined(__x86_64)
        return new x86Lowerer<BackEnd_T>(backEnd);
#endif
        internalError("Did not resolve an ABI.");
        return nullptr;
    }

    BackEnd_T & backEnd;

    ABILowerer(BackEnd_T & _backEnd) : backEnd(_backEnd) {}

    virtual void ABILowerProcedureType(void * data) = 0;
    virtual void ABILowerCall(void * data) = 0;
};

template <typename BackEnd_T> struct x86Lowerer : ABILowerer<BackEnd_T> {
    x86Lowerer(BackEnd_T & backEnd) : ABILowerer<BackEnd_T>(backEnd) {}

    void ABILowerProcedureType(void * data);
    void ABILowerCall(void * data);
};
} // namespace bjou

#endif
