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

template <> void x86Lowerer<LLVMBackEnd>::ABILowerProcedureType(void * data);
template <> void x86Lowerer<LLVMBackEnd>::ABILowerCall(void * data);
} // namespace bjou

#endif
