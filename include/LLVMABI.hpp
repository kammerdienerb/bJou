//
//  LLVMABI.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 2/20/18.
//  Copyright © 2018 me. All rights reserved.
//

#ifndef LLVMABI_hpp
#define LLVMABI_hpp

#include "ABI.hpp"
#include "LLVMBackEnd.hpp"

#include <vector>

namespace bjou {
struct ABILowerProcedureTypeData {
    ProcedureType * t = nullptr;
    bool sret = false;
    int byval = 0;
    int ref = 0;
    bool ref_ret = false;
    llvm::FunctionType * fn_t = nullptr;
};

namespace x86 {
enum ParamClass {
    POINTER, // This class consists of pointer types.
    INTEGER, // This class consists of integral types (except pointer types)
             // that fit into one of the general purpose registers.
    SSE,     // The class consists of types that fit into a vector register.
    SSEUP,   // The class consists of types that fit into a vector register and
             // can be passed and returned in the upper bytes of it.
    X87,
    X87UP, // These classes consists of types that will be returned via the x87
           // FPU.
    COMPLEX_X87, // This class consists of types that will be returned via the
                 // x87 FPU.
    NO_CLASS, // This class is used as initializer in the algorithms. It will be
              // used for padding and empty structures and unions.
    MEMORY // This class consists of types that will be passed and returned in
           // memory via the stack.
};

ParamClass ABIClassForType(LLVMBackEnd & backEnd, const Type * t);
} // namespace x86
template <> void x86Lowerer<LLVMBackEnd>::ABILowerProcedureType(void * data);
template <> void x86Lowerer<LLVMBackEnd>::ABILowerCall(void * data);
} // namespace bjou

#endif
