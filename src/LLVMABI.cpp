//
//  LLVMABI.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 2/20/18.
//  Copyright © 2018 me. All rights reserved.
//

#include "LLVMABI.hpp"
#include "LLVMBackEnd.hpp"
#include "Type.hpp"

namespace bjou {
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

static ParamClass ABIClassForField(ParamClass c, ParamClass _8b) {
    if (_8b == NO_CLASS)
        return c;

    if (_8b == MEMORY || c == MEMORY)
        return MEMORY;

    if (_8b == INTEGER || c == INTEGER)
        return INTEGER;

    if (_8b == X87 || c == X87 || _8b == X87UP || c == X87UP)
        return MEMORY;

    return SSE;
}

#define MEM_THRESH 16

static ParamClass ABIClassForType(LLVMBackEnd & backEnd, const Type * t) {
    unsigned int size = simpleSizer(t);

    if (t->isVoid())
        return NO_CLASS;

    if (t->isPointer() || t->isRef() || t->isProcedure()) {
        return POINTER;
    } else if (t->isInt() || t->isBool() || t->isChar()) {
        return INTEGER;
    } else if (t->isFloat()) {
        return SSE;
    } else if (t->isArray()) {
        if (size > MEM_THRESH)
            return MEMORY;
        return ABIClassForType(backEnd, ((ArrayType *)t)->elem_t);
    } else if (t->isStruct() || t->isTuple()) {
        if (size > MEM_THRESH)
            return MEMORY;

        const std::vector<const Type *> & types =
            t->isStruct() ? ((StructType *)t)->memberTypes
                          : ((TupleType *)t)->getTypes();

        if (types.empty())
            return NO_CLASS;
        if (types.size() == 1)
            return ABIClassForType(backEnd, types[0]);

        ParamClass c = NO_CLASS;
        for (const Type * m_t : types) {
            ParamClass _8b = ABIClassForType(backEnd, m_t);
            c = ABIClassForField(_8b, c);
        }
        // @incomplete post merger cleanup
        return c;
    }
    printf("%s\n", t->getDemangledName().c_str());
    internalError("No ABI parameter class type resolved.");
}
} // namespace x86

template <> void x86Lowerer<LLVMBackEnd>::ABILowerProcedureType(void * data) {
    using namespace x86;

    ABILowerProcedureTypeData * payload = (ABILowerProcedureTypeData *)data;
    LLVMBackEnd& llbe = ((LLVMBackEnd&)backEnd);
    ProcedureType * t = payload->t;

    const Type * new_ret_t = nullptr;
    std::vector<const Type *> new_params;

    // handle struct return
    const Type * ret_t = t->getRetType();
    if (ret_t->isRef())
        payload->ref_ret = true;
    ParamClass r_c = ABIClassForType(backEnd, ret_t);
    if (r_c == MEMORY) {
        new_params.push_back(ret_t->getPointer());
        new_ret_t = VoidType::get();
        payload->sret = true;
    } else
        new_ret_t = ret_t;

    // params
    int i = payload->sret;
    for (const Type * a_t : t->getParamTypes()) {
        if (a_t->isRef())
            payload->ref |= (1 << i);

        if (a_t->isStruct() || a_t->isTuple()) {
            if (ABIClassForType(backEnd, a_t) == MEMORY) {
                new_params.push_back(a_t->getPointer());
                payload->byval |= (1 << i);
            } else {
                // @incomplete
                // need to split members into eightbytes or something

                /*
                const std::vector<const Type*>& types = t->isStruct()
                    ? ((StructType*)t)->memberTypes
                    : ((TupleType*)t)->getTypes();
                */

                new_params.push_back(a_t);
            }
        } else
            new_params.push_back(a_t);

        i += 1;
    }

    payload->t = (ProcedureType*)ProcedureType::get(new_params, new_ret_t, t->isVararg);

    std::vector<llvm::Type*> ll_param_types;
    for (const Type * t : new_params)
        ll_param_types.push_back(llbe.getOrGenType(t));

    payload->fn_t = llvm::FunctionType::get(llbe.getOrGenType(new_ret_t), ll_param_types, t->isVararg);
}

template <> void x86Lowerer<LLVMBackEnd>::ABILowerCall(void * data) {}
} // namespace bjou
