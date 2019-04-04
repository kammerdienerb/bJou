//
//  LLVMBackEnd.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef LLVMBackEnd_hpp
#define LLVMBackEnd_hpp

#include <map>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "ABI.hpp"
#include "ASTNode.hpp"
#include "BackEnd.hpp"
#include "LLVMGenerator.hpp"
#include "Type.hpp"
#include "std_string_hasher.hpp"
#include "hybrid_map.hpp"

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace bjou {
struct StackFrame {
    StackFrame();

    struct FrameVal {
        llvm::Value * val;
        const Type * type;
    };

    std::vector<FrameVal> vals;
    hybrid_map<std::string, size_t, std_string_hasher> namedVals;
};

struct LoopFrameInfo {
    LoopFrameInfo(StackFrame _frame, llvm::BasicBlock * _bb);

    StackFrame frame;
    llvm::BasicBlock * bb;
};

struct LLVMBackEnd : BackEnd {
    LLVMBackEnd(FrontEnd & _frontEnd);
    ~LLVMBackEnd();

    enum GEN_MODE { RT, CT };
    GEN_MODE mode;

    void init();
    void init_jit();

    void jit_reset();

    LLVMGenerator generator;
    ABILowerer<LLVMBackEnd> * abi_lowerer;

    std::string nativeTriple;
    std::string genTriple;
    std::string genArch;
    const llvm::Target * target;
    llvm::TargetMachine * targetMachine;
    llvm::DataLayout * layout;

    llvm::LLVMContext llContext;
    llvm::Module *llModule, *outModule;

    std::unique_ptr<llvm::Module> jitModule;
    llvm::ExecutionEngine * ee;

    llvm::IRBuilder<> builder;

    std::vector<StackFrame> frames;

    std::map<ASTNode *, llvm::Value *> generated_nodes;
    std::map<const Type *, llvm::Type *> generated_types;
    std::set<ASTNode *>       types_need_completion;
    std::set<const SumType *> sum_types_need_completion;
    std::set<ASTNode *>       globs_need_completion;
    std::set<ASTNode *>       procs_need_completion;

    std::map<Procedure *, void *> proc_abi_info;

    std::map<std::string, std::function<llvm::Pass *()>> pass_create_by_name;
    std::map<Procedure *, llvm::legacy::FunctionPassManager *> proc_passes;
    void addProcedurePass(Procedure * proc, std::string pass_name);

    void genStructProcs(Struct * s);

    llvm::Value * getOrGenNode(ASTNode * node, bool getAddr = false);
    llvm::Type * getOrGenType(const Type * t);

    std::unordered_map<Constant *, llvm::Value *> generatedTypeMemberConstants;

    std::stack<LoopFrameInfo> loop_break_stack;
    std::stack<LoopFrameInfo> loop_continue_stack;
    std::stack<Procedure *> proc_stack;
    std::stack<llvm::BasicBlock *> local_alloc_stack;
    std::stack<llvm::Value *> expr_block_yield_stack;

    milliseconds go();
    void completeTypes();
    void completeSumType(const SumType * s_t);
    void completeSumTypes();
    void completeGlobs();
    void completeProcs();
    void * run(Procedure * proc, void * _val_args);

    void pushFrame();
    void popFrame();
    StackFrame & curFrame();

    llvm::Value * addNamedVal(std::string name, llvm::Value * val,
                              const Type * type);
    llvm::Value * addUnnamedVal(llvm::Value * val, const Type * type);
    llvm::Value * allocNamedVal(std::string name, const Type * type);
    llvm::Value * allocUnnamedVal(const Type * type, bool array2pointer = true);
    llvm::Value * getNamedVal(std::string name);

    llvm::Type * bJouTypeToLLVMType(const bjou::Type * t);
    llvm::Type * createOrLookupDefinedType(const bjou::Type * t);
    llvm::StructType * createSumStructType(const bjou::Type * t);
    llvm::StructType * createTupleStructType(const bjou::Type * t);

    llvm::Function * createMainEntryPoint();
    void completeMainEntryPoint(llvm::Function * func);

    void createPrintfProto();
    llvm::Value * getPointerToArrayElements(llvm::Value * array);
    llvm::Value * createPointerToArrayElementsOnStack(llvm::Value * array,
                                                      const Type * t);
    llvm::Constant * createConstantInitializer(InitializerList * ilist);
    llvm::Value * copyConstantInitializerToStack(llvm::Constant * constant_init,
                                                 const Type * t);
    llvm::Value * createGlobalStringVariable(std::string str);
    llvm::ConstantArray * create_v_table_constant_array(Struct * s);
    llvm::Value * createGlobaltypeinfo(Struct * s);
    llvm::Value * getGlobaltypeinfo(Struct * s);

    milliseconds IRGenStage();
    milliseconds CodeGenStage();
    milliseconds LinkingStage();
};
} // namespace bjou

#endif /* LLVMBackEnd_hpp */
