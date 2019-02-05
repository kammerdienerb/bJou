//
//  LLVMBackEnd.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "LLVMBackEnd.hpp"

#include <algorithm>
#include <future>
#include <sys/wait.h>
#include <unordered_map>
#include <vector>

// #include <llvm/CodeGen/CommandFlags.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/Interpreter.h>
#include <llvm/ExecutionEngine/MCJIT.h>

#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Scalar.h>

#include <llvm/Config/llvm-config.h>

#include "ASTNode.hpp"
#include "CLI.hpp"
#include "Compile.hpp"
#include "FrontEnd.hpp"
#include "Global.hpp"
#include "LLVMABI.hpp"
#include "LLVMGenerator.hpp"
#include "Misc.hpp"
#include "Type.hpp"
#include "CompilerAPI.hpp"

#include "nolibc_syscall.h"

#ifdef BJOU_DEBUG_BUILD
#define SAVE_BJOU_DEBUG_BUILD
#endif
#undef BJOU_DEBUG_BUILD
#include "tclap/CmdLine.h"
#ifdef SAVE_BJOU_DEBUG_BUILD
#define BJOU_DEBUG_BUILD
#endif

#ifdef BJOU_INSTALL_PREFIX
    #define NOLIBC_SYSCALL_SEARCH BJOU_INSTALL_PREFIX "/lib"
#else
    #define NOLIBC_SYSCALL_SEARCH "/usr/local/lib"
#endif

namespace bjou {

StackFrame::StackFrame() : vals({}), namedVals({}) {}

LoopFrameInfo::LoopFrameInfo(StackFrame _frame, llvm::BasicBlock * _bb)
    : frame(_frame), bb(_bb) {}

LLVMBackEnd::LLVMBackEnd(FrontEnd & _frontEnd)
    : BackEnd(_frontEnd), generator(*this),
      abi_lowerer(ABILowerer<LLVMBackEnd>::get(*this)), builder(llContext) {
    target = nullptr;
    targetMachine = nullptr;
    layout = nullptr;
    llModule = nullptr;
    outModule = nullptr;
    ee = nullptr;
}

void LLVMBackEnd::init() {
    mode = GEN_MODE::CT;

    // llvm initialization for object code gen
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();

    /* llvm::InitializeNativeTarget(); */
    /* llvm::InitializeNativeTargetAsmPrinter(); */
    /* llvm::InitializeNativeTargetAsmParser(); */

    // initialize passes
    llvm::PassRegistry & Registry = *llvm::PassRegistry::getPassRegistry();

    llvm::initializeCore(Registry);
    llvm::initializeCoroutines(Registry);
    llvm::initializeScalarOpts(Registry);
    llvm::initializeObjCARCOpts(Registry);
    llvm::initializeVectorization(Registry);
    llvm::initializeIPO(Registry);
    llvm::initializeAnalysis(Registry);
    llvm::initializeTransformUtils(Registry);
    llvm::initializeInstCombine(Registry);
    llvm::initializeInstrumentation(Registry);
    llvm::initializeTarget(Registry);
    llvm::initializeCodeGen(Registry);
    // For codegen passes, only passes that do IR to IR transformation are
    // supported.
    /* llvm::initializeCodeGenPreparePass(Registry); */
    /* llvm::initializeAtomicExpandPass(Registry); */
    /* llvm::initializeRewriteSymbolsLegacyPassPass(Registry); */
    /* llvm::initializeWinEHPreparePass(Registry); */
    /* llvm::initializeDwarfEHPreparePass(Registry); */
    /* llvm::initializeSafeStackLegacyPassPass(Registry); */
    /* llvm::initializeSjLjEHPreparePass(Registry); */
    /* llvm::initializePreISelIntrinsicLoweringLegacyPassPass(Registry); */
    /* llvm::initializeGlobalMergePass(Registry); */
    /* llvm::initializeInterleavedAccessPass(Registry); */
    /* // llvm::initializeCountingFunctionInserterPass(Registry); */
    /* llvm::initializeUnreachableBlockElimLegacyPassPass(Registry); */

    // set up target and triple
    nativeTriple = llvm::sys::getDefaultTargetTriple();
    genTriple = compilation->args.target_triple_arg;
    if (genTriple.empty()) {
        genTriple = nativeTriple; 
    }
    std::string targetErr;
    target =
        llvm::TargetRegistry::lookupTarget(genTriple, targetErr);
    if (!target) {
        error(Context(), "Could not create llvm Target.", true,
              targetErr, "Did you compile LLVM with the desired target enabled?");
    }

    llvm::TargetOptions Options;
    llvm::CodeGenOpt::Level OLvl =
        (compilation->args.opt_arg ? llvm::CodeGenOpt::Aggressive
                                   : llvm::CodeGenOpt::None);

    auto RM = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Model::PIC_);
    if (strstr(target->getShortDescription(), "RISC-V")) {
        RM = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Model::Static);
    }
    targetMachine = target->createTargetMachine(
        genTriple, "", "", Options, RM,
        llvm::None, OLvl);
    layout = new llvm::DataLayout(targetMachine->createDataLayout());

    outModule = new llvm::Module(compilation->outputbasefilename, llContext);
    outModule->setDataLayout(targetMachine->createDataLayout());
    outModule->setTargetTriple(genTriple);

    // setup pre defined passes
    pass_create_by_name["earlycse"] = []() {
        return llvm::createEarlyCSEPass();
    };

    pass_create_by_name["licm"] = []() { return llvm::createLICMPass(); };
    pass_create_by_name["constprop"] = []() {
        return llvm::createConstantPropagationPass();
    };
    pass_create_by_name["dce"] = []() {
        return llvm::createDeadCodeEliminationPass();
    };
    pass_create_by_name["adce"] = []() {
        return llvm::createAggressiveDCEPass();
    };
    pass_create_by_name["loops"] = []() {
        return new llvm::LoopInfoWrapperPass();
    };
    pass_create_by_name["simplifycfg"] = []() {
        return llvm::createCFGSimplificationPass();
    };
    pass_create_by_name["loop-simplify"] = []() {
#if LLVM_VERSION_MAJOR >= 7
        return llvm::createLoopSimplifyCFGPass();
#else
        return llvm::createLoopSimplifyPass();
#endif
    };
    pass_create_by_name["loop-unroll"] = []() {
        return llvm::createSimpleLoopUnrollPass();
    };
    pass_create_by_name["tailcallelim"] = []() {
        return llvm::createTailCallEliminationPass();
    };
    pass_create_by_name["print-function"] = []() {
        return llvm::createPrintFunctionPass(llvm::outs());
    };
}

void LLVMBackEnd::init_jit() { pushFrame(); }

void LLVMBackEnd::jit_reset() {
    generated_nodes.clear();
    generated_types.clear();

    jitModule = std::unique_ptr<llvm::Module>(
        new llvm::Module{compilation->outputbasefilename + "__jit", llContext});
    llModule = jitModule.get();

    std::string targetErr;
    target =
        llvm::TargetRegistry::lookupTarget(nativeTriple, targetErr);
    if (!target) {
        error(Context(), "Could not create llvm native Target.", true,
              targetErr);
    }

    llvm::TargetOptions Options;
    llvm::CodeGenOpt::Level OLvl =
        (compilation->args.opt_arg ? llvm::CodeGenOpt::Aggressive
                                   : llvm::CodeGenOpt::None);

    auto RM = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Model::PIC_);
    if (strstr(target->getShortDescription(), "RISC-V")) {
        RM = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Model::Static);
    }
    targetMachine = target->createTargetMachine(
        nativeTriple, "", "", Options, RM,
        llvm::None, OLvl, /* jit = */true);
    layout = new llvm::DataLayout(targetMachine->createDataLayout());

    jitModule->setDataLayout(targetMachine->createDataLayout());
    jitModule->setTargetTriple(nativeTriple);

    if (ee)
        delete ee;

    /* Create execution engine */
    // No optimization seems to be the best route here as far as
    // compile times go..
    std::string err_str;
    llvm::EngineBuilder engineBuilder(std::move(jitModule));
    engineBuilder.setErrorStr(&err_str).setOptLevel(llvm::CodeGenOpt::None);

    ee = engineBuilder.create();

    if (!ee) {
        error(COMPILER_SRC_CONTEXT(), "LLVM:", true, err_str);
        internalError("There was an llvm error.");
    }

    // add mappings to bJou compiler API functions
    ee->addGlobalMapping("bjou_StartDefaultCompilation", (uint64_t)&bjou_StartDefaultCompilation);
    ee->addGlobalMapping("bjou_dump", (uint64_t)&bjou_dump);
    ee->addGlobalMapping("bjou_makeUID", (uint64_t)&bjou_makeUID);
    ee->addGlobalMapping("bjou_isKeyword", (uint64_t)&bjou_isKeyword);
    ee->addGlobalMapping("bjou_createContext", (uint64_t)&bjou_createContext);
    ee->addGlobalMapping("bjou_getContext", (uint64_t)&bjou_getContext);
    ee->addGlobalMapping("bjou_contextGetFileName", (uint64_t)&bjou_contextGetFileName);
    ee->addGlobalMapping("bjou_setContext", (uint64_t)&bjou_setContext);
    ee->addGlobalMapping("bjou_setProcNameContext", (uint64_t)&bjou_setProcNameContext);
    ee->addGlobalMapping("bjou_setVarDeclNameContext", (uint64_t)&bjou_setVarDeclNameContext);
    ee->addGlobalMapping("bjou_setStructNameContext", (uint64_t)&bjou_setStructNameContext);
    ee->addGlobalMapping("bjou_setAliasNameContext", (uint64_t)&bjou_setAliasNameContext);
    ee->addGlobalMapping("bjou_freeContext", (uint64_t)&bjou_freeContext);
    ee->addGlobalMapping("bjou_error", (uint64_t)&bjou_error);
    ee->addGlobalMapping("bjou_getVersionString", (uint64_t)&bjou_getVersionString);
    ee->addGlobalMapping("bjou_parseToMultiNode", (uint64_t)&bjou_parseToMultiNode);
    ee->addGlobalMapping("bjou_parseAndAppend", (uint64_t)&bjou_parseAndAppend);
    ee->addGlobalMapping("bjou_appendNode", (uint64_t)&bjou_appendNode);
    ee->addGlobalMapping("bjou_runTypeCompletion", (uint64_t)&bjou_runTypeCompletion);
    ee->addGlobalMapping("bjou_setGlobalNodeRP", (uint64_t)&bjou_setGlobalNodeRP);
    ee->addGlobalMapping("bjou_getGlobalScope", (uint64_t)&bjou_getGlobalScope);
    ee->addGlobalMapping("bjou_clone", (uint64_t)&bjou_clone);
    ee->addGlobalMapping("bjou_preDeclare", (uint64_t)&bjou_preDeclare);
    ee->addGlobalMapping("bjou_addSymbols", (uint64_t)&bjou_addSymbols);
    ee->addGlobalMapping("bjou_analyze", (uint64_t)&bjou_analyze);
    ee->addGlobalMapping("bjou_forceAnalyze", (uint64_t)&bjou_forceAnalyze);
    ee->addGlobalMapping("bjou_getStructName", (uint64_t)&bjou_getStructName);
    ee->addGlobalMapping("bjou_getEnumName", (uint64_t)&bjou_getEnumName);
    ee->addGlobalMapping("bjou_setVarDeclName", (uint64_t)&bjou_setVarDeclName);
    ee->addGlobalMapping("bjou_makeZeroInitExpr", (uint64_t)&bjou_makeZeroInitExpr);
    ee->addGlobalMapping("bjou_createAddExpression", (uint64_t)&bjou_createAddExpression);
    ee->addGlobalMapping("bjou_createSubExpression", (uint64_t)&bjou_createSubExpression);
    ee->addGlobalMapping("bjou_createBSHLExpression", (uint64_t)&bjou_createBSHLExpression);
    ee->addGlobalMapping("bjou_createBSHRExpression", (uint64_t)&bjou_createBSHRExpression);
    ee->addGlobalMapping("bjou_createMultExpression", (uint64_t)&bjou_createMultExpression);
    ee->addGlobalMapping("bjou_createDivExpression", (uint64_t)&bjou_createDivExpression);
    ee->addGlobalMapping("bjou_createModExpression", (uint64_t)&bjou_createModExpression);
    ee->addGlobalMapping("bjou_createAssignmentExpression", (uint64_t)&bjou_createAssignmentExpression);
    ee->addGlobalMapping("bjou_createAddAssignExpression", (uint64_t)&bjou_createAddAssignExpression);
    ee->addGlobalMapping("bjou_createSubAssignExpression", (uint64_t)&bjou_createSubAssignExpression);
    ee->addGlobalMapping("bjou_createMultAssignExpression", (uint64_t)&bjou_createMultAssignExpression);
    ee->addGlobalMapping("bjou_createDivAssignExpression", (uint64_t)&bjou_createDivAssignExpression);
    ee->addGlobalMapping("bjou_createModAssignExpression", (uint64_t)&bjou_createModAssignExpression);
    ee->addGlobalMapping("bjou_createLssExpression", (uint64_t)&bjou_createLssExpression);
    ee->addGlobalMapping("bjou_createLeqExpression", (uint64_t)&bjou_createLeqExpression);
    ee->addGlobalMapping("bjou_createLeqExpression", (uint64_t)&bjou_createLeqExpression);
    ee->addGlobalMapping("bjou_createGtrExpression", (uint64_t)&bjou_createGtrExpression);
    ee->addGlobalMapping("bjou_createGeqExpression", (uint64_t)&bjou_createGeqExpression);
    ee->addGlobalMapping("bjou_createEquExpression", (uint64_t)&bjou_createEquExpression);
    ee->addGlobalMapping("bjou_createNeqExpression", (uint64_t)&bjou_createNeqExpression);
    ee->addGlobalMapping("bjou_createLogAndExpression", (uint64_t)&bjou_createLogAndExpression);
    ee->addGlobalMapping("bjou_createLogOrExpression", (uint64_t)&bjou_createLogOrExpression);
    ee->addGlobalMapping("bjou_createBANDExpression", (uint64_t)&bjou_createBANDExpression);
    ee->addGlobalMapping("bjou_createBORExpression", (uint64_t)&bjou_createBORExpression);
    ee->addGlobalMapping("bjou_createBXORExpression", (uint64_t)&bjou_createBXORExpression);
    ee->addGlobalMapping("bjou_createCallExpression", (uint64_t)&bjou_createCallExpression);
    ee->addGlobalMapping("bjou_createSubscriptExpression", (uint64_t)&bjou_createSubscriptExpression);
    ee->addGlobalMapping("bjou_createAccessExpression", (uint64_t)&bjou_createAccessExpression);
    ee->addGlobalMapping("bjou_createNewExpression", (uint64_t)&bjou_createNewExpression);
    ee->addGlobalMapping("bjou_createDeleteExpression", (uint64_t)&bjou_createDeleteExpression);
    ee->addGlobalMapping("bjou_createSizeofExpression", (uint64_t)&bjou_createSizeofExpression);
    ee->addGlobalMapping("bjou_createNotExpression", (uint64_t)&bjou_createNotExpression);
    ee->addGlobalMapping("bjou_createBNEGExpression", (uint64_t)&bjou_createBNEGExpression);
    ee->addGlobalMapping("bjou_createDerefExpression", (uint64_t)&bjou_createDerefExpression);
    ee->addGlobalMapping("bjou_createAddressExpression", (uint64_t)&bjou_createAddressExpression);
    ee->addGlobalMapping("bjou_createRefExpression", (uint64_t)&bjou_createRefExpression);
    ee->addGlobalMapping("bjou_createAsExpression", (uint64_t)&bjou_createAsExpression);
    ee->addGlobalMapping("bjou_createIdentifier", (uint64_t)&bjou_createIdentifier);
    ee->addGlobalMapping("bjou_createIdentifierWithInst", (uint64_t)&bjou_createIdentifierWithInst);
    ee->addGlobalMapping("bjou_createBooleanLiteral", (uint64_t)&bjou_createBooleanLiteral);
    ee->addGlobalMapping("bjou_createIntegerLiteral", (uint64_t)&bjou_createIntegerLiteral);
    ee->addGlobalMapping("bjou_createFloatLiteral", (uint64_t)&bjou_createFloatLiteral);
    ee->addGlobalMapping("bjou_createStringLiteral", (uint64_t)&bjou_createStringLiteral);
    ee->addGlobalMapping("bjou_createCharLiteral", (uint64_t)&bjou_createCharLiteral);
    ee->addGlobalMapping("bjou_createExprBlock", (uint64_t)&bjou_createExprBlock);
    ee->addGlobalMapping("bjou_createDeclarator", (uint64_t)&bjou_createDeclarator);
    ee->addGlobalMapping("bjou_createArrayDeclarator", (uint64_t)&bjou_createArrayDeclarator);
    ee->addGlobalMapping("bjou_createPointerDeclarator", (uint64_t)&bjou_createPointerDeclarator);
    ee->addGlobalMapping("bjou_createVariableDeclaration", (uint64_t)&bjou_createVariableDeclaration);
    ee->addGlobalMapping("bjou_createFieldDeclaration", (uint64_t)&bjou_createFieldDeclaration);
    ee->addGlobalMapping("bjou_createParamDeclaration", (uint64_t)&bjou_createParamDeclaration);
    ee->addGlobalMapping("bjou_createProcedureDeclarator", (uint64_t)&bjou_createProcedureDeclarator);
    ee->addGlobalMapping("bjou_createAlias", (uint64_t)&bjou_createAlias);
    ee->addGlobalMapping("bjou_createStruct", (uint64_t)&bjou_createStruct);
    ee->addGlobalMapping("bjou_createEnum", (uint64_t)&bjou_createEnum);
    ee->addGlobalMapping("bjou_createArgList", (uint64_t)&bjou_createArgList);
    ee->addGlobalMapping("bjou_createProcedure", (uint64_t)&bjou_createProcedure);
    ee->addGlobalMapping("bjou_createExternProcedure", (uint64_t)&bjou_createExternProcedure);
    ee->addGlobalMapping("bjou_createReturn", (uint64_t)&bjou_createReturn);
    ee->addGlobalMapping("bjou_createBreak", (uint64_t)&bjou_createBreak);
    ee->addGlobalMapping("bjou_createContinue", (uint64_t)&bjou_createContinue);
    ee->addGlobalMapping("bjou_createExprBlockYield", (uint64_t)&bjou_createExprBlockYield);
    ee->addGlobalMapping("bjou_createIf", (uint64_t)&bjou_createIf);
    ee->addGlobalMapping("bjou_createElse", (uint64_t)&bjou_createElse);
    ee->addGlobalMapping("bjou_createFor", (uint64_t)&bjou_createFor);
    ee->addGlobalMapping("bjou_createWhile", (uint64_t)&bjou_createWhile);
    ee->addGlobalMapping("bjou_createDoWhile", (uint64_t)&bjou_createDoWhile);
    ee->addGlobalMapping("bjou_createTemplateInst", (uint64_t)&bjou_createTemplateInst);
    ee->addGlobalMapping("bjou_createMacroUse", (uint64_t)&bjou_createMacroUse);

    ee->addGlobalMapping("nolibc_syscall", (uint64_t)&nolibc_syscall);
}

LLVMBackEnd::~LLVMBackEnd() {
    // if (jitModule)
    // delete jitModule;
    if (outModule)
        delete outModule;
    if (layout)
        delete layout;
}

milliseconds LLVMBackEnd::go() {
    auto start = Clock::now();

    auto cg_time = CodeGenStage();
    if (compilation->args.time_arg)
        prettyPrintTimeMin(
            cg_time,
            std::string("Code Generation") +
                (compilation->args.opt_arg ? " and Optimization" : ""));

    if (!compilation->args.c_arg) {
        auto l_time = LinkingStage();
        if (compilation->args.time_arg)
            prettyPrintTimeMin(l_time, "Linking");
    }

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

void * LLVMBackEnd::run(Procedure * proc, void * _val_args) {
    void * ret;

    std::vector<Val> & val_args = *(std::vector<Val> *)_val_args;

    mode = GEN_MODE::CT;

    /* save generation target info */
    const llvm::Target * save_target         = target;
    llvm::TargetMachine * save_targetMachine = targetMachine;
    llvm::DataLayout * save_layout           = layout;
    /*******************************/

    if (!ee)
        init_jit();

    jit_reset();

    // just in case
    if (!compilation->args.nopreload_arg) {
        getOrGenNode(compilation->frontEnd.printf_decl);
        getOrGenNode(compilation->frontEnd.malloc_decl);
        getOrGenNode(compilation->frontEnd.free_decl);
        getOrGenNode(compilation->frontEnd.memset_decl);
        getOrGenNode(compilation->frontEnd.memcpy_decl);
        getOrGenNode(compilation->frontEnd.__bjou_rt_init_def);
    }

    llvm::Function * fn = (llvm::Function *)getOrGenNode(proc);
    std::string name = fn->getName().str();

    completeTypes();
    completeGlobs();
    completeProcs();

    // verify module
    std::string errstr;
    llvm::raw_string_ostream errstream(errstr);
    if (llvm::verifyModule(*llModule, &errstream)) {
        llModule->print(llvm::errs(), nullptr);
        error(COMPILER_SRC_CONTEXT(), "LLVM:", true, errstream.str());
        internalError("There was an llvm error.");
    }

    uint64_t ptr = ee->getFunctionAddress(name);

    const Type * _t = proc->getType();
    ProcedureType * t = (ProcedureType *)_t;
    const std::vector<const Type *> & param_types = t->getParamTypes();
    const Type * ret_t = t->getRetType();

    if (param_types.size() == 0) {
        if (ret_t->isVoid()) {
            void (*fn_ptr)() = (void (*)())ptr;
            fn_ptr();
            return nullptr;
        } else if (ret_t->isInt()) {
            if (((IntType *)ret_t)->width == 32) {
                int (*fn_ptr)() = (int (*)())ptr;
                ret = (void *)fn_ptr();
            } else if (((IntType *)ret_t)->width == 64) {
                long long (*fn_ptr)() = (long long (*)())ptr;
                ret = (void *)fn_ptr();
            }
        } else if (ret_t->isChar()) {
            char (*fn_ptr)() = (char (*)())ptr;
            ret = (void *)fn_ptr();
        } else if (ret_t->isFloat()) {
            if (((FloatType *)ret_t)->width == 32) {
                float (*fn_ptr)() = (float (*)())ptr;
                float r = fn_ptr();
                ret = (void *)*((uint64_t *)&r);
            } else if (((FloatType *)ret_t)->width == 64) {
                double (*fn_ptr)() = (double (*)())ptr;
                double r = fn_ptr();
                ret = (void *)*((uint64_t *)&r);
            }
        } else if (ret_t->isPointer() && ret_t->under()->isChar()) {
            char * (*fn_ptr)() = (char * (*)())ptr;
            ret = (void *)fn_ptr();
        }
    } else if (param_types.size() == 1) {
        const Type * param_t = param_types[0];
        Val & arg = val_args[0];
        if (ret_t->isVoid()) {
            if (param_t->isInt()) {
                if (((IntType *)param_t)->width == 32) {
                    void (*fn_ptr)(int) = (void (*)(int))ptr;
                    fn_ptr(arg.as_i64);
                    return nullptr;
                } else if (((IntType *)param_t)->width == 64) {
                    void (*fn_ptr)(long long) = (void (*)(long long))ptr;
                    fn_ptr(arg.as_i64);
                    return nullptr;
                }
            } else if (param_t->isChar()) {
                void (*fn_ptr)(char) = (void (*)(char))ptr;
                fn_ptr(arg.as_i64);
                return nullptr;
            } else if (param_t->isFloat()) {
                if (((FloatType *)param_t)->width == 32) {
                    void (*fn_ptr)(float) = (void (*)(float))ptr;
                    fn_ptr(arg.as_f64);
                    return nullptr;
                } else if (((FloatType *)param_t)->width == 64) {
                    void (*fn_ptr)(double) = (void (*)(double))ptr;
                    fn_ptr(arg.as_f64);
                    return nullptr;
                }
            } else if (param_t->isPointer() && param_t->under()->isChar()) {
                void (*fn_ptr)(char *) = (void (*)(char *))ptr;
                fn_ptr((char *)arg.as_string.c_str());
                return nullptr;
            }
        } else if (ret_t->isInt()) {
            if (((IntType *)ret_t)->width == 32) {
                if (param_t->isInt()) {
                    if (((IntType *)param_t)->width == 32) {
                        int (*fn_ptr)(int) = (int (*)(int))ptr;
                        ret = (void *)fn_ptr(arg.as_i64);
                    } else if (((IntType *)param_t)->width == 64) {
                        int (*fn_ptr)(long long) = (int (*)(long long))ptr;
                        ret = (void *)fn_ptr(arg.as_i64);
                    }
                } else if (param_t->isChar()) {
                    int (*fn_ptr)(char) = (int (*)(char))ptr;
                    ret = (void *)fn_ptr(arg.as_i64);
                } else if (param_t->isFloat()) {
                    if (((FloatType *)param_t)->width == 32) {
                        int (*fn_ptr)(float) = (int (*)(float))ptr;
                        ret = (void *)fn_ptr(arg.as_f64);
                    } else if (((FloatType *)param_t)->width == 64) {
                        int (*fn_ptr)(double) = (int (*)(double))ptr;
                        ret = (void *)fn_ptr(arg.as_f64);
                    }
                } else if (param_t->isPointer() && param_t->under()->isChar()) {
                    int (*fn_ptr)(char *) = (int (*)(char *))ptr;
                    ret = (void *)fn_ptr((char *)arg.as_string.c_str());
                }
            } else if (((IntType *)ret_t)->width == 64) {
                if (param_t->isInt()) {
                    if (((IntType *)param_t)->width == 32) {
                        long long (*fn_ptr)(int) = (long long (*)(int))ptr;
                        ret = (void *)fn_ptr(arg.as_i64);
                    } else if (((IntType *)param_t)->width == 64) {
                        long long (*fn_ptr)(long long) =
                            (long long (*)(long long))ptr;
                        ret = (void *)fn_ptr(arg.as_i64);
                    }
                } else if (param_t->isChar()) {
                    long long (*fn_ptr)(char) = (long long (*)(char))ptr;
                    ret = (void *)fn_ptr(arg.as_i64);
                } else if (param_t->isFloat()) {
                    if (((FloatType *)param_t)->width == 32) {
                        long long (*fn_ptr)(float) = (long long (*)(float))ptr;
                        ret = (void *)fn_ptr(arg.as_f64);
                    } else if (((FloatType *)param_t)->width == 64) {
                        long long (*fn_ptr)(double) =
                            (long long (*)(double))ptr;
                        ret = (void *)fn_ptr(arg.as_f64);
                    }
                } else if (param_t->isPointer() && param_t->under()->isChar()) {
                    long long (*fn_ptr)(char *) = (long long (*)(char *))ptr;
                    ret = (void *)fn_ptr((char *)arg.as_string.c_str());
                }
            }
        } else if (ret_t->isChar()) {
            if (param_t->isInt()) {
                if (((IntType *)param_t)->width == 32) {
                    char (*fn_ptr)(int) = (char (*)(int))ptr;
                    ret = (void *)fn_ptr(arg.as_i64);
                } else if (((IntType *)param_t)->width == 64) {
                    char (*fn_ptr)(long long) = (char (*)(long long))ptr;
                    ret = (void *)fn_ptr(arg.as_i64);
                }
            } else if (param_t->isChar()) {
                char (*fn_ptr)(char) = (char (*)(char))ptr;
                ret = (void *)fn_ptr(arg.as_i64);
            } else if (param_t->isFloat()) {
                if (((FloatType *)param_t)->width == 32) {
                    char (*fn_ptr)(float) = (char (*)(float))ptr;
                    ret = (void *)fn_ptr(arg.as_f64);
                } else if (((FloatType *)param_t)->width == 64) {
                    char (*fn_ptr)(double) = (char (*)(double))ptr;
                    ret = (void *)fn_ptr(arg.as_f64);
                }
            } else if (param_t->isPointer() && param_t->under()->isChar()) {
                char (*fn_ptr)(char *) = (char (*)(char *))ptr;
                ret = (void *)fn_ptr((char *)arg.as_string.c_str());
            }
        } else if (ret_t->isFloat()) {
            if (((FloatType *)ret_t)->width == 32) {
                if (param_t->isInt()) {
                    if (((IntType *)param_t)->width == 32) {
                        float (*fn_ptr)(int) = (float (*)(int))ptr;
                        float r = fn_ptr(arg.as_i64);
                        ret = (void *)*((uint64_t *)&r);
                    } else if (((IntType *)param_t)->width == 64) {
                        float (*fn_ptr)(long long) = (float (*)(long long))ptr;
                        float r = fn_ptr(arg.as_i64);
                        ret = (void *)*((uint64_t *)&r);
                    }
                } else if (param_t->isChar()) {
                    float (*fn_ptr)(char) = (float (*)(char))ptr;
                    float r = fn_ptr(arg.as_i64);
                    ret = (void *)*((uint64_t *)&r);
                } else if (param_t->isFloat()) {
                    if (((FloatType *)param_t)->width == 32) {
                        float (*fn_ptr)(float) = (float (*)(float))ptr;
                        float r = fn_ptr(arg.as_f64);
                        ret = (void *)*((uint64_t *)&r);
                    } else if (((FloatType *)param_t)->width == 64) {
                        float (*fn_ptr)(double) = (float (*)(double))ptr;
                        float r = fn_ptr(arg.as_f64);
                        ret = (void *)*((uint64_t *)&r);
                    }
                } else if (param_t->isPointer() && param_t->under()->isChar()) {
                    float (*fn_ptr)(char *) = (float (*)(char *))ptr;
                    float r = fn_ptr((char *)arg.as_string.c_str());
                    ret = (void *)*((uint64_t *)&r);
                }
            } else if (((FloatType *)ret_t)->width == 64) {
                if (param_t->isInt()) {
                    if (((IntType *)param_t)->width == 32) {
                        double (*fn_ptr)(int) = (double (*)(int))ptr;
                        double r = fn_ptr(arg.as_i64);
                        ret = (void *)*((uint64_t *)&r);
                    } else if (((IntType *)param_t)->width == 64) {
                        double (*fn_ptr)(long long) =
                            (double (*)(long long))ptr;
                        double r = fn_ptr(arg.as_i64);
                        ret = (void *)*((uint64_t *)&r);
                    }
                } else if (param_t->isChar()) {
                    double (*fn_ptr)(char) = (double (*)(char))ptr;
                    double r = fn_ptr(arg.as_i64);
                    ret = (void *)*((uint64_t *)&r);
                } else if (param_t->isFloat()) {
                    if (((FloatType *)param_t)->width == 32) {
                        double (*fn_ptr)(float) = (double (*)(float))ptr;
                        double r = fn_ptr(arg.as_f64);
                        ret = (void *)*((uint64_t *)&r);
                    } else if (((FloatType *)param_t)->width == 64) {
                        double (*fn_ptr)(double) = (double (*)(double))ptr;
                        double r = fn_ptr(arg.as_f64);
                        ret = (void *)*((uint64_t *)&r);
                    }
                } else if (param_t->isPointer() && param_t->under()->isChar()) {
                    double (*fn_ptr)(char *) = (double (*)(char *))ptr;
                    double r = fn_ptr((char *)arg.as_string.c_str());
                    ret = (void *)*((uint64_t *)&r);
                }
            }
        } else if (ret_t->isPointer() && ret_t->under()->isChar()) {
            if (param_t->isInt()) {
                if (((IntType *)param_t)->width == 32) {
                    char * (*fn_ptr)(int) = (char * (*)(int))ptr;
                    ret = (void *)fn_ptr(arg.as_i64);
                } else if (((IntType *)param_t)->width == 64) {
                    char * (*fn_ptr)(long long) = (char * (*)(long long))ptr;
                    ret = (void *)fn_ptr(arg.as_i64);
                }
            } else if (param_t->isChar()) {
                char * (*fn_ptr)(char) = (char * (*)(char))ptr;
                ret = (void *)fn_ptr(arg.as_i64);
            } else if (param_t->isFloat()) {
                if (((FloatType *)param_t)->width == 32) {
                    char * (*fn_ptr)(float) = (char * (*)(float))ptr;
                    ret = (void *)fn_ptr(arg.as_f64);
                } else if (((FloatType *)param_t)->width == 64) {
                    char * (*fn_ptr)(double) = (char * (*)(double))ptr;
                    ret = (void *)fn_ptr(arg.as_f64);
                }
            } else if (param_t->isPointer() && param_t->under()->isChar()) {
                char * (*fn_ptr)(char *) = (char * (*)(char *))ptr;
                ret = (void *)fn_ptr((char *)arg.as_string.c_str());
            }
        }
    }
    
    /* restore generation target info */
    target        = save_target;
    targetMachine = save_targetMachine;
    layout        = save_layout;
    /*******************************/

    /* internalError("LLVMBackEnd::run(): For now, there is a limit to the type " */
    /*               "signatures of procedures that we can call."); */

    return ret;
}

void LLVMBackEnd::addProcedurePass(Procedure * proc, std::string pass_name) {
    auto search = proc_passes.find(proc);
    if (search == proc_passes.end()) {
        // create manager and add default passes needed for others
        llvm::PassManagerBuilder Builder;
        Builder.OptLevel = 0;
        Builder.SizeLevel = 0;
        Builder.DisableUnrollLoops = false;

        llvm::legacy::FunctionPassManager * fpass =
            new llvm::legacy::FunctionPassManager(outModule);

        fpass->add(new llvm::TargetLibraryInfoWrapperPass(
            llvm::Triple(outModule->getTargetTriple())));
        fpass->add(createTargetTransformInfoWrapperPass(
            targetMachine->getTargetIRAnalysis()));

#if LLVM_VERSION_MAJOR > 4
        targetMachine->adjustPassManager(Builder);
#endif

        Builder.populateFunctionPassManager(*fpass);

#if !LLVM_VERSION_MAJOR > 4
        targetMachine->addEarlyAsPossiblePasses(*fpass);
#endif

        fpass->add(pass_create_by_name["earlycse"]());

        proc_passes[proc] = fpass;
    }

    llvm::Pass * pass = pass_create_by_name[pass_name]();

    BJOU_DEBUG_ASSERT(pass);

    proc_passes[proc]->add(pass);
}

static void findSliceAndDynamicArrayDecls(Declarator * base,
                                          std::vector<Declarator *> & out) {
    Declarator * decl = (Declarator *)base->parent;
    while (decl && IS_DECLARATOR(decl)) {
        if (decl->nodeKind == ASTNode::SLICE_DECLARATOR ||
            decl->nodeKind == ASTNode::DYNAMIC_ARRAY_DECLARATOR) {

            out.push_back(decl);
        }
        decl = (Declarator *)decl->parent;
    }
}

static void genDeps(ASTNode * node, LLVMBackEnd & llbe) {
    // @bad -- sort of an on-demand generation model
    // we'll see how stable this is

    if (llbe.mode == LLVMBackEnd::GEN_MODE::RT && isCT(node))
        return;

    std::vector<ASTNode *> terms;
    node->unwrap(terms);
    for (ASTNode * term : terms) {
        if (IS_DECLARATOR(term)) {
            const Type * t = term->getType();
            llbe.getOrGenType(t);
            std::vector<Declarator *> slices_and_das;
            findSliceAndDynamicArrayDecls((Declarator *)term, slices_and_das);

            for (Declarator * decl : slices_and_das)
                llbe.getOrGenType(decl->getType());
        } else if (term->nodeKind == ASTNode::IDENTIFIER) {
            Identifier * ident = (Identifier *)term;

            if (ident->resolved) {
                ASTNode * r = ident->resolved;

                llbe.getOrGenType(r->getType());

                if (r->nodeKind == ASTNode::PROCEDURE ||
                    r->nodeKind == ASTNode::CONSTANT ||
                    (r->nodeKind == ASTNode::VARIABLE_DECLARATION &&
                     (!r->getScope()->parent))) {

                    llbe.getOrGenNode(r);
                }
            }
        } else if (term->nodeKind == ASTNode::PROC_LITERAL ||
                   term->nodeKind == ASTNode::EXTERN_LITERAL) {
            llbe.getOrGenNode(term);
        }
    }
}

void LLVMBackEnd::genStructProcs(Struct * s) {
    const StructType * s_t = (const StructType *)s->getType();
    for (auto & set : s_t->memberProcs) {
        for (auto & _proc : set.second->procs) {
            Procedure * proc = (Procedure *)_proc.second->node();
            if (!_proc.second->isTemplateProc()) { //&& proc->getParentStruct() == s) {
                getOrGenNode(proc);
            }
        }
    }
}

llvm::Value * LLVMBackEnd::getOrGenNode(ASTNode * node, bool getAddr) {
    if (node->getFlag(ASTNode::IGNORE_GEN))
        return nullptr;

    // ignore compile time nodes
    // we should be safe returning null here since we catch
    // compile-time-only symbol references in analysis
    if (mode == GEN_MODE::RT && isCT(node))
        return nullptr;

    auto search = generated_nodes.find(node);
    if (search != generated_nodes.end())
        return search->second;
    llvm::Value * val = (llvm::Value *)node->generate(*this, getAddr);
    // @note: Procedure::generate already fills this value to preemptively
    // protect from recursion, but this is not a problem -- there should
    // be no conflicts
    generated_nodes[node] = val;
    return val;
}

llvm::Type * LLVMBackEnd::getOrGenType(const Type * t) {
    auto search = generated_types.find(t);
    if (search != generated_types.end())
        return search->second;
    llvm::Type * ll_t = bJouTypeToLLVMType(t);
    generated_types[t] = ll_t;
    return ll_t;
}

void LLVMBackEnd::completeTypes() {
    while (!types_need_completion.empty()) {
        std::vector<ASTNode *> copy;
        copy.insert(copy.begin(), types_need_completion.begin(),
                    types_need_completion.end());
        types_need_completion.clear();

        for (ASTNode * node : copy)
            node->generate(*this);
    }
}

void LLVMBackEnd::completeGlobs() {
    while (!globs_need_completion.empty()) {
        std::vector<ASTNode *> copy;
        copy.insert(copy.begin(), globs_need_completion.begin(),
                    globs_need_completion.end());
        globs_need_completion.clear();

        for (ASTNode * node : copy)
            generated_nodes[node] = (llvm::Value *)node->generate(*this);
    }
}

void LLVMBackEnd::completeProcs() {
    while (!procs_need_completion.empty()) {
        std::vector<ASTNode *> copy;
        copy.insert(copy.begin(), procs_need_completion.begin(),
                    procs_need_completion.end());
        procs_need_completion.clear();

#if 0
        if (!compilation->args.noparallel_arg) {
            size_t n_procs = copy.size();

            auto completeProc = [this](ASTNode * proc) { return proc->generate(*this); };
          
            unsigned int nthreaded = std::thread::hardware_concurrency() - 1;

            std::vector<std::future<void*> > futures;

            unsigned int i;
            for (i = 0; i < n_procs / nthreaded; i += 1) {
                for (unsigned int j = 0; j < nthreaded; j += 1) {
                    ASTNode * the_proc = copy[(i * nthreaded) + j];
                    futures.push_back(std::async(std::launch::async, completeProc, the_proc));
                }

                for (auto& f : futures)
                    f.get();

                futures.clear();
            }

            for (unsigned long i = n_procs - (n_procs % nthreaded); i < n_procs; i += 1) {
                ASTNode * the_proc = copy[i];
                futures.push_back(std::async(std::launch::async, completeProc, the_proc));
            }

            for (auto& f : futures)
                f.get();
        } else {
            for (ASTNode * node : copy)
                node->generate(*this);
        }
#endif
        for (ASTNode * node : copy)
            node->generate(*this);
    }
}

static void * GenerateGlobalVariable(VariableDeclaration * var,
                                     llvm::GlobalVariable * gvar,
                                     BackEnd & backEnd, bool flag = false) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * t = var->getType();
    llvm::Type * ll_t = llbe->getOrGenType(t);

    /* if (!gvar) { */
        if (t->isArray()) {
            PointerType * ptr_t = (PointerType *)t->under()->getPointer();
            llvm::Type * ll_ptr_t = llbe->getOrGenType(ptr_t);
            gvar =
                new llvm::GlobalVariable(*llbe->llModule, ll_ptr_t, false,
                                         llvm::GlobalVariable::ExternalLinkage,
                                         0, var->getMangledName());
        } else {
            gvar = new llvm::GlobalVariable(
                /*Module=*/*llbe->llModule,
                /*Type=*/ll_t,
                /*isConstant=*/false,
                /*Linkage=*/llvm::GlobalValue::ExternalLinkage,
                /*Initializer=*/0, // has initializer, specified below
                /*Name=*/var->getMangledName());
        }
        BJOU_DEBUG_ASSERT(gvar);

        llbe->addNamedVal(var->getMangledName(), gvar, t);

        /* return gvar; */
    /* } else { */
        auto align = llbe->layout->getABITypeAlignment(ll_t);

        if (t->isArray()) {
            PointerType * ptr_t = (PointerType *)t->under()->getPointer();
            llvm::Type * ll_ptr_t = llbe->getOrGenType(ptr_t);

            gvar->setAlignment(
                (unsigned int)llbe->layout->getTypeAllocSize(ll_ptr_t));

            llvm::GlobalVariable * under = new llvm::GlobalVariable(
                *llbe->llModule, ll_t, false,
                llvm::GlobalVariable::ExternalLinkage, 0,
                "__bjou_array_under_" + var->getMangledName());
            under->setAlignment(llbe->layout->getPreferredAlignment(under));

            if (var->getInitialization()) {
                if (var->getInitialization()->nodeKind ==
                    ASTNode::INITIALZER_LIST)
                    under->setInitializer(llbe->createConstantInitializer(
                        (InitializerList *)var->getInitialization()));
            } else {
                under->setInitializer(llvm::Constant::getNullValue(ll_t));
            }

            gvar->setInitializer(
                (llvm::Constant *)llbe->builder.CreateInBoundsGEP(
                    under,
                    {llvm::Constant::getNullValue(
                         llvm::IntegerType::getInt32Ty(llbe->llContext)),
                     llvm::Constant::getNullValue(
                         llvm::IntegerType::getInt32Ty(llbe->llContext))}));
        } else {
            gvar->setAlignment((unsigned int)align);
            if (var->getInitialization())
                gvar->setInitializer((llvm::Constant *)llbe->getOrGenNode(
                    var->getInitialization()));
            else
                gvar->setInitializer(llvm::Constant::getNullValue(ll_t));
        }
    /* } */

    return gvar;
}

milliseconds LLVMBackEnd::CodeGenStage() {
    auto start = Clock::now();

    mode = GEN_MODE::RT;

    llModule = outModule;
    generated_nodes.clear();
    generated_types.clear();
    types_need_completion.clear();
    procs_need_completion.clear();
    generatedTypeMemberConstants.clear();

    frames.clear(); // clear jit frames if any

    pushFrame();

    // just in case
    if (!compilation->args.nopreload_arg) {
        getOrGenNode(compilation->frontEnd.printf_decl);
        getOrGenNode(compilation->frontEnd.malloc_decl);
        getOrGenNode(compilation->frontEnd.free_decl);
        getOrGenNode(compilation->frontEnd.memset_decl);
        getOrGenNode(compilation->frontEnd.memcpy_decl);
        getOrGenNode(compilation->frontEnd.__bjou_rt_init_def);
    }

    // global variables and constants may reference procs
    // or types that have not been declared yet
    std::vector<ASTNode *> global_vars_and_consts;

    /*
    for (ASTNode * node : compilation->frontEnd.AST) {
        if (node->isStatement()) {
            if (node->nodeKind == ASTNode::CONSTANT ||
                node->nodeKind == ASTNode::VARIABLE_DECLARATION) {
                // other statements delegated to createMainEntryPoint()
                global_vars_and_consts.push_back(node);
            }
        } else
            getOrGenNode(node);
    }
    */

    // for (ASTNode * node : global_vars_and_consts)
    // getOrGenNode(node);

    if (compilation->args.c_arg) {
        for (ASTNode * node : compilation->frontEnd.AST) {
            getOrGenNode(node);
        }

        completeTypes();
        completeGlobs();
        completeProcs();
    } else {
        llvm::Function * main = createMainEntryPoint();

        completeTypes();
        completeGlobs();
        completeProcs();

        completeMainEntryPoint(main);
    }

    popFrame();

    std::string errstr;
    llvm::raw_string_ostream errstream(errstr);
    if (llvm::verifyModule(*llModule, &errstream)) {
        llModule->print(llvm::errs(), nullptr);
        error(COMPILER_SRC_CONTEXT(), "LLVM:", true, errstream.str());
        internalError("There was an llvm error.");
    }

    generator.generate();

    if (compilation->args.verbose_arg)
        llModule->print(llvm::errs(), nullptr);

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

milliseconds LLVMBackEnd::LinkingStage() {
    auto start = Clock::now();

    std::string dest, dest_o;
    if (!compilation->args.output_arg.empty()) {
        dest = compilation->args.output_arg;
        if (compilation->args.c_arg)
            dest_o = dest;
        else
            dest_o = dest + ".o";
    } else {
        dest = compilation->outputpath + compilation->outputbasefilename;
        dest_o = dest + ".o";
    }

    std::vector<const char *> link_args = {dest_o.c_str()};

    for (auto & f : compilation->obj_files)
        link_args.push_back(f.c_str());

    link_args.push_back("-o");
    link_args.push_back(dest.c_str());

    for (auto & l : compilation->args.link_arg) {
        link_args.push_back("-l");
        link_args.push_back(l.c_str());
    }

    link_args.push_back("-ldl");
    link_args.push_back("-lm");
    link_args.push_back("-L" NOLIBC_SYSCALL_SEARCH);
    link_args.push_back("-lnolibc_syscall");

    bool use_system_linker = true;

    if (compilation->args.lld_arg) {
        internalError("No lld support at this time.");

        /*
        std::string errstr;
        llvm::raw_string_ostream errstream(errstr);

        std::vector<const char *> more_args = {
            "-demangle",
            "-dynamic",
            "-arch",
            "x86_64",
            "-macosx_version_min",
            "10.12.0",
            "-lSystem",
            "/Applications/Xcode.app/Contents/Developer/Toolchains/"
            "XcodeDefault.xctoolchain/usr/bin/../lib/clang/8.1.0/lib/darwin/"
            "libclang_rt.osx.a"};

        for (auto arg : more_args)
            link_args.push_back(arg);

        bool success = lld::mach_o::link(link_args, errstream);

        if (!success) {
            if (errstream.str().find("warning") == 0) {
                warning(COMPILER_SRC_CONTEXT(), "LLD:", errstream.str());
            } else {
                error(COMPILER_SRC_CONTEXT(), "LLD:", false, errstream.str());
                // internalError("There was an lld error.");
            }
            warning(COMPILER_SRC_CONTEXT(),
                    "lld failed. Falling back to system linker.");
            use_system_linker = true;
        } else
            use_system_linker = false;

        */
    }

    int fds[2];
    int status;
    pid_t childpid;

    if (use_system_linker) {
        const char * cc = getenv("CC");
        if (!cc)
            cc = "cc";

        link_args.insert(link_args.begin(), cc);
        link_args.push_back(NULL);

        if (pipe(fds) == -1)
            internalError("Error creating pipe to system linker.");

        childpid = fork();
        if (childpid == 0) {
            dup2(fds[1], STDERR_FILENO);
            close(fds[0]);
            execvp(link_args[0], (char **)link_args.data());
            internalError("Error executing system linker.");
        } else if (childpid == -1) {
            internalError("Error forking system linker.");
        }

        close(fds[1]);
        waitpid(childpid, &status, 0);

        if (WIFEXITED(status)) {
            status = WEXITSTATUS(status);
            if (status) {
                char errout[4096];
                int n = read(fds[0], errout, sizeof(errout));
                errout[n] = '\0';
                std::string s_errout(errout);
                error(COMPILER_SRC_CONTEXT(), "Linker error.", false, s_errout);
            }
        }
    }

    if (!compilation->args.c_arg) {
        const char * rm_args[] = {"rm", dest_o.c_str(), nullptr};

        childpid = fork();
        if (childpid == 0) {
            execvp(rm_args[0], (char **)rm_args);
            internalError("Error executing 'rm'.");
        } else if (childpid == -1) {
            internalError("Error forking 'rm'.");
        }

        waitpid(childpid, &status, 0);
    }

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

void LLVMBackEnd::pushFrame() { frames.emplace_back(); }

StackFrame & LLVMBackEnd::curFrame() { return frames.back(); }

void LLVMBackEnd::popFrame() {
    BJOU_DEBUG_ASSERT(!frames.empty());
    frames.pop_back();
}

llvm::Value * LLVMBackEnd::addNamedVal(std::string name, llvm::Value * val,
                                       const Type * type) {
    BJOU_DEBUG_ASSERT(!frames.empty());
    auto & f = curFrame();

    if (type->isSlice())
        type = ((SliceType *)type)->getRealType();
    if (type->isDynamicArray())
        type = ((DynamicArrayType *)type)->getRealType();

    f.vals.push_back({val, type});
    f.namedVals[name] = f.vals.size() - 1;

    return val;
}

llvm::Value * LLVMBackEnd::addUnnamedVal(llvm::Value * val, const Type * type) {
    BJOU_DEBUG_ASSERT(!frames.empty());
    auto & f = curFrame();

    if (type->isSlice())
        type = ((SliceType *)type)->getRealType();
    if (type->isDynamicArray())
        type = ((DynamicArrayType *)type)->getRealType();

    f.vals.push_back({val, type});

    return val;
}

llvm::Value * LLVMBackEnd::allocNamedVal(std::string name, const Type * type) {
    llvm::Type * t = getOrGenType(type);

    BJOU_DEBUG_ASSERT(t);

    auto save = builder.saveIP();
    builder.SetInsertPoint(local_alloc_stack.top());

    llvm::Value * alloca = builder.CreateAlloca(t, 0, name);
    ((llvm::AllocaInst *)alloca)->setAlignment(layout->getABITypeAlignment(t));

    if (t->isArrayTy()) {
        llvm::Value * ptr =
            builder.CreateAlloca(t->getArrayElementType()->getPointerTo());
        builder.CreateStore(
            builder.CreateInBoundsGEP(
                alloca, {llvm::Constant::getNullValue(
                             llvm::IntegerType::getInt32Ty(llContext)),
                         llvm::Constant::getNullValue(
                             llvm::IntegerType::getInt32Ty(llContext))}),
            ptr);

        addNamedVal(name, ptr, type);

        builder.restoreIP(save);

        return ptr;
    }

    addNamedVal(name, alloca, type);

    builder.restoreIP(save);

    return alloca;
}

llvm::Value * LLVMBackEnd::allocUnnamedVal(const Type * type,
                                           bool array2pointer) {
    llvm::Type * t = getOrGenType(type);

    BJOU_DEBUG_ASSERT(t);

    auto save = builder.saveIP();
    builder.SetInsertPoint(local_alloc_stack.top());

    llvm::Value * alloca = builder.CreateAlloca(t, 0, "");
    ((llvm::AllocaInst *)alloca)->setAlignment(layout->getABITypeAlignment(t));

    if (t->isArrayTy() && array2pointer) {
        llvm::Value * ptr =
            builder.CreateAlloca(t->getArrayElementType()->getPointerTo());
        builder.CreateStore(
            builder.CreateInBoundsGEP(
                alloca, {llvm::Constant::getNullValue(
                             llvm::IntegerType::getInt32Ty(llContext)),
                         llvm::Constant::getNullValue(
                             llvm::IntegerType::getInt32Ty(llContext))}),
            ptr);

        addUnnamedVal(ptr, type);
        return ptr;
    }

    addUnnamedVal(alloca, type);

    builder.restoreIP(save);

    return alloca;
}

llvm::Value * LLVMBackEnd::getNamedVal(std::string name) {
    BJOU_DEBUG_ASSERT(!frames.empty());
    auto & f = curFrame();
    for (auto search = frames.rbegin(); search != frames.rend(); search++)
        if (search->namedVals.count(name))
            return search->vals[search->namedVals[name]].val;

    return nullptr;
}

llvm::Type * LLVMBackEnd::bJouTypeToLLVMType(const bjou::Type * t) {
    switch (t->kind) {
    case Type::PLACEHOLDER:
        internalError("Invalid type in llvm generation.");
        break;
    case Type::VOID:
        return llvm::Type::getVoidTy(llContext);
    case Type::BOOL:
        return llvm::IntegerType::get(llContext, 1);
    case Type::INT:
        return llvm::IntegerType::get(llContext, ((IntType *)t)->width);
    case Type::ENUM:
        return llvm::IntegerType::get(llContext, 64);
    case Type::FLOAT: {
        unsigned int width = ((FloatType*)t)->width;
        if (width == 32)
            return llvm::Type::getFloatTy(llContext);
        else if (width == 64)
            return llvm::Type::getDoubleTy(llContext);
        else if (width == 128)
            return llvm::Type::getFP128Ty(llContext);
        // return llvm::Type::getDoubleTy(llContext);
        /*if (t->size == 32)
            return llvm::Type::getFloatTy(llContext);
        else if (t->size == 64)
            return llvm::Type::getDoubleTy(llContext);*/ }
        case Type::CHAR:
            return llvm::IntegerType::get(llContext, 8);
        case Type::STRUCT: {
            Struct * s = ((StructType *)t)->_struct;
            llvm::Type * ll_t = llvm::StructType::create(llContext, s->getMangledName());
            generated_types[t] = ll_t;
            s->analyze();
            genStructProcs(s);
            getOrGenNode(s);
            return ll_t;
        }
        /*
        case Type::ENUM:
            // @incomplete
            break;
        case Type::ALIAS:
            return getOrGenType(((AliasType *)t)->getOriginal());
            break;
        */
        case Type::ARRAY: {
            ArrayType * array_t = (ArrayType *)t;
            uint64_t width = 1;
            while (array_t->isArray()) {
                width *= array_t->width;
                array_t = (ArrayType *)array_t->under();
            }
            return llvm::ArrayType::get(getOrGenType(array_t), width);
        }
        case Type::SLICE: {
            SliceType * s_t = (SliceType *)t;
            return getOrGenType(s_t->getRealType());
        }
        case Type::DYNAMIC_ARRAY: {
            DynamicArrayType * dyn_t = (DynamicArrayType *)t;
            return getOrGenType(dyn_t->getRealType());
        }
        /*
        case Type::DYNAMIC_ARRAY:
            BJOU_DEBUG_ASSERT(false && "Dynamic arrays not implemented yet.");
        */
        case Type::POINTER:
            if (t->under() == VoidType::get())
                return getOrGenType(
                    IntType::get(Type::UNSIGNED, 8)->getPointer());
            return getOrGenType(t->under())->getPointerTo();
        case Type::REF:
            if (t->under()->isArray())
                return getOrGenType(t->under()->under())->getPointerTo()->getPointerTo();
            return getOrGenType(t->under())->getPointerTo();
        /*
        case Type::MAYBE:
            // @incomplete
            break;
        */
        case Type::TUPLE:
            return createTupleStructType(t);
        case Type::PROCEDURE: {
            ProcedureType * proc_t = (ProcedureType *)t;

            // @abi
            ABILowerProcedureTypeData * payload = new ABILowerProcedureTypeData;
            payload->t = proc_t;
            abi_lowerer->ABILowerProcedureType(payload);
            llvm::FunctionType * fn_t = payload->fn_t;
            delete payload;
            return fn_t->getPointerTo();
            //
        }
            /*
            case Type::TEMPLATE_STRUCT:
            case Type::TEMPLATE_ALIAS:
                internalError("Template type in llvm generation.");
                break;
            */
        }

        BJOU_DEBUG_ASSERT(false && "Did not convert bJou type to LLVM type.");
        return nullptr;
}

llvm::Type * LLVMBackEnd::createOrLookupDefinedType(const bjou::Type * t) {
    // @incomplete
    return nullptr;
}

llvm::StructType * LLVMBackEnd::createTupleStructType(const bjou::Type * t) {
    BJOU_DEBUG_ASSERT(t->isTuple());
    TupleType * t_t = (TupleType *)t;
    std::string name = compilation->frontEnd.makeUID("tuple");
    llvm::StructType * ll_t = llvm::StructType::create(llContext, name);
    std::vector<llvm::Type *> sub_types;
    for (const Type * sub_t : t_t->types)
        sub_types.push_back(getOrGenType(sub_t));
    ll_t->setBody(sub_types);
    return ll_t;
}

llvm::Function * LLVMBackEnd::createMainEntryPoint() {
    std::vector<llvm::Type *> main_arg_types = {
        llvm::Type::getInt32Ty(llContext),
        llvm::PointerType::get(llvm::Type::getInt8PtrTy(llContext), 0)};

    llvm::FunctionType * main_t = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(llContext), main_arg_types, false);
    llvm::Function * func = llvm::Function::Create(
        main_t, llvm::Function::ExternalLinkage, "main", llModule);
    
    llvm::BasicBlock * bblock =
        llvm::BasicBlock::Create(llContext, "entry", func);
    builder.SetInsertPoint(bblock);

    auto arg_it = func->args().begin();
    addNamedVal("__bjou_main_arg0", &(*arg_it++),
                IntType::get(Type::UNSIGNED, 32));
    addNamedVal("__bjou_main_arg1", &(*arg_it),
                CharType::get()->getPointer()->getPointer());

    for (ASTNode * node : compilation->frontEnd.AST) {
        if (node->isStatement()) {
            genDeps(node, *this);
        } else if (node->nodeKind == ASTNode::MULTINODE) {
            std::vector<ASTNode *> subNodes;

            ((MultiNode *)node)->flatten(subNodes);

            for (ASTNode * sub : subNodes)
                if (sub->isStatement())
                    genDeps(sub, *this);
        }
    }

    return func;
}

void LLVMBackEnd::completeMainEntryPoint(llvm::Function * func) {
    BJOU_DEBUG_ASSERT(func);

    builder.SetInsertPoint(&func->back());

    if (!compilation->args.nopreload_arg) {
        llvm::Function * rt_init_fn = llModule->getFunction(
            compilation->frontEnd.__bjou_rt_init_def->getMangledName());
        BJOU_DEBUG_ASSERT(rt_init_fn);
        std::vector<llvm::Value *> args;
        args.push_back(getNamedVal("__bjou_main_arg0"));
        args.push_back(getNamedVal("__bjou_main_arg1"));
        builder.CreateCall(rt_init_fn, args);
    }

    for (ASTNode * node : compilation->frontEnd.AST) {
        if (node->isStatement()) {
            getOrGenNode(node);
        } else if (node->nodeKind == ASTNode::MULTINODE) {
            std::vector<ASTNode *> subNodes;

            ((MultiNode *)node)->flatten(subNodes);

            for (ASTNode * sub : subNodes)
                if (sub->isStatement())
                    getOrGenNode(sub);
        }
    }

    builder.CreateRet(
        llvm::ConstantInt::get(llContext, llvm::APInt(32, 0, true)));

    std::string errstr;
    llvm::raw_string_ostream errstream(errstr);
    if (llvm::verifyFunction(*func, &errstream)) {
        func->print(llvm::errs(), nullptr);
        error(COMPILER_SRC_CONTEXT(), "LLVM:", true, errstream.str());
        internalError("There was an llvm error.");
    }
}

void LLVMBackEnd::createPrintfProto() {
    std::vector<llvm::Type *> printf_arg_types = {
        llvm::Type::getInt8PtrTy(llContext)};

    llvm::FunctionType * printf_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(llContext), printf_arg_types, true);

    llvm::Function * func = llvm::Function::Create(
        printf_type, llvm::Function::ExternalLinkage, "printf", llModule);
    func->setCallingConv(llvm::CallingConv::C);

    std::string errstr;
    llvm::raw_string_ostream errstream(errstr);
    if (llvm::verifyFunction(*func, &errstream)) {
        func->print(llvm::errs(), nullptr);
        error(COMPILER_SRC_CONTEXT(), "LLVM:", true, errstream.str());
        internalError("There was an llvm error.");
    }
}

llvm::Value * LLVMBackEnd::createGlobalStringVariable(std::string str) {
    llvm::ArrayType * var_t =
        llvm::ArrayType::get(llvm::Type::getInt8Ty(llContext), str.size() + 1);
    llvm::GlobalVariable * global_var = new llvm::GlobalVariable(
        /*Module=*/*llModule,
        /*Type=*/var_t,
        /*isConstant=*/true,
        /*Linkage=*/llvm::GlobalValue::PrivateLinkage,
        /*Initializer=*/0, // has initializer, specified below
        /*Name=*/".str");
    global_var->setAlignment(1);
    global_var->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);

    llvm::Constant * str_constant =
        llvm::ConstantDataArray::getString(llContext, str.c_str(), true);
    global_var->setInitializer(str_constant);

    llvm::Value * indices[2] = {
        llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llContext)),
        llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llContext))};

    return llvm::ConstantExpr::getGetElementPtr(var_t, global_var, indices,
                                                true);
}

void * ASTNode::generate(BackEnd & backEnd, bool flag) {
    return nullptr;
}

void * MultiNode::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * val = nullptr;

    for (ASTNode * node : nodes)
        val = llbe->getOrGenNode(node);

    return val;
}

void * BinaryExpression::generate(BackEnd & backEnd, bool flag) {
    // should never be called
    BJOU_DEBUG_ASSERT(false);
    return nullptr;
    /*
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;
    // typedef llvm::Value* (llvm::IRBuilder<llvm::ConstantFolder,
    // llvm::IRBuilderDefaultInserter>::*instrCreateFn)(llvm::Value*,
    // llvm::Value*, const llvm::Twine&, bool, bool);

    const Type * lt = getLeft()->getType();
    const Type * rt = getRight()->getType();

    llvm::Value * lval = (llvm::Value *)getLeft().generate(backEnd);
    llvm::Value * rval = (llvm::Value *)getRight().generate(backEnd);

    // VERY @incomplete

    if (lt->isPrimative() && lt->equivalent(rt)) {
        if (lt->isInt()) {
            switch (getContents()[0]) {
            case '+':
                return llbe->builder.CreateAdd(lval, rval);
                break;
            case '-':
                return llbe->builder.CreateSub(lval, rval);
                break;
            case '*':
                return llbe->builder.CreateMul(lval, rval);
                break;
            case '/':
                return llbe->builder.CreateSDiv(lval, rval);
                break;
            case '%':
                return llbe->builder.CreateSRem(lval, rval);
                break;
            case '<':
                return llbe->builder.CreateICmpSLT(lval, rval);
                break;
            case '>':
                return llbe->builder.CreateICmpSGT(lval, rval);
                break;
            }
        } else {
            switch (getContents()[0]) {
            case '+':
                return llbe->builder.CreateFAdd(lval, rval);
                break;
            case '-':
                return llbe->builder.CreateFSub(lval, rval);
                break;
            case '*':
                return llbe->builder.CreateFMul(lval, rval);
                break;
            case '/':
                return llbe->builder.CreateFDiv(lval, rval);
                break;
            case '%':
                return llbe->builder.CreateFRem(lval, rval);
                break;
            case '<':
                return llbe->builder.CreateFCmpOLT(lval, rval);
                break;
            case '>':
                return llbe->builder.CreateFCmpOGT(lval, rval);
                break;
            }
        }
    }
    return nullptr;
    */
}

void * AddExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * myt = getType();
    const Type * lt = getLeft()->getType()->unRef();

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd);
    llvm::Value * rv = (llvm::Value *)getRight()->generate(backEnd);

    if (myt->isPointer()) {
        std::vector<llvm::Value *> indices;
        if (lt->isPointer()) {
            indices.push_back(rv);
            return llbe->builder.CreateInBoundsGEP(lv, indices);
        } else {
            indices.push_back(lv);
            return llbe->builder.CreateInBoundsGEP(rv, indices);
        }
    } else if (myt->isInt()) {
        return llbe->builder.CreateAdd(lv, rv);
    } else if (myt->isFloat()) {
        return llbe->builder.CreateFAdd(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * SubExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * myt = getType();
    const Type * lt = getLeft()->getType();
    const Type * rt = getRight()->getType();

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd);
    llvm::Value * rv = (llvm::Value *)getRight()->generate(backEnd);

    if (lt->isPointer() && rt->isPointer()) {
        return llbe->builder.CreatePtrDiff(lv, rv);
    } else if (myt->isPointer()) {
        if (lt->isPointer()) {
            std::vector<llvm::Value *> indices;
            indices.push_back(llbe->builder.CreateNeg(rv));
            return llbe->builder.CreateInBoundsGEP(lv, indices);
        } else
            BJOU_DEBUG_ASSERT(false);
    } else if (myt->isInt()) {
        return llbe->builder.CreateSub(lv, rv);
    } else if (myt->isFloat()) {
        return llbe->builder.CreateFSub(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * BSHLExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd);
    llvm::Value * rv = (llvm::Value *)getRight()->generate(backEnd);

    return llbe->builder.CreateShl(lv, rv);
}

void * BSHRExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd);
    llvm::Value * rv = (llvm::Value *)getRight()->generate(backEnd);

    return llbe->builder.CreateLShr(lv, rv);
}

void * MultExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * myt = getType();

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd);
    llvm::Value * rv = (llvm::Value *)getRight()->generate(backEnd);

    if (myt->isInt()) {
        return llbe->builder.CreateMul(lv, rv);
    } else if (myt->isFloat()) {
        return llbe->builder.CreateFMul(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * DivExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * myt = getType();

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd);
    llvm::Value * rv = (llvm::Value *)getRight()->generate(backEnd);

    if (myt->isInt()) {
        const IntType * myit = (const IntType *)myt;
        if (myit->sign == Type::SIGNED)
            return llbe->builder.CreateSDiv(lv, rv);
        else
            return llbe->builder.CreateUDiv(lv, rv);
    } else if (myt->isFloat()) {
        return llbe->builder.CreateFDiv(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * ModExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * myt = getType();

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd);
    llvm::Value * rv = (llvm::Value *)getRight()->generate(backEnd);

    if (myt->isInt()) {
        const IntType * myit = (const IntType *)myt;
        if (myit->sign == Type::SIGNED)
            return llbe->builder.CreateSRem(lv, rv);
        else
            return llbe->builder.CreateURem(lv, rv);
    } else if (myt->isFloat()) {
        return llbe->builder.CreateFRem(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * AssignmentExpression::generate(BackEnd & backEnd, bool getAddr) {
    bool v_r = getFlag(Expression::VOLATILE_R);
    bool v_w = getFlag(Expression::VOLATILE_W);

    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value *lv = (llvm::Value *)getLeft()->generate(backEnd, true),
                *rv = nullptr;

    const Type * lt = getLeft()->getType()->unRef();
    if (lt->isDynamicArray())
        lt = ((DynamicArrayType *)lt)->getRealType();
    else if (lt->isSlice())
        lt = ((SliceType *)lt)->getRealType();

    if (lt->isStruct()) { // do memcpy
        rv = (llvm::Value *)getRight()->generate(backEnd, true);

        llvm::PointerType * ll_byte_ptr_t =
            llvm::Type::getInt8Ty(llbe->llContext)->getPointerTo();

        auto align = llbe->layout->getABITypeAlignment(llbe->getOrGenType(lt));

        llvm::Value * dest = llbe->builder.CreateBitCast(lv, ll_byte_ptr_t);
        llvm::Value * src = llbe->builder.CreateBitCast(rv, ll_byte_ptr_t);
        llvm::Value * size = llvm::ConstantInt::get(
            llvm::Type::getInt64Ty(llbe->llContext),
            llbe->layout->getTypeAllocSize(llbe->getOrGenType(lt)));

#if LLVM_VERSION_MAJOR >= 7
        llbe->builder.CreateMemCpy(dest, align, src, align, size);
#else
        llbe->builder.CreateMemCpy(dest, src, size, align);
#endif
    } else {
        rv = (llvm::Value *)getRight()->generate(backEnd);

        llvm::StoreInst * store = llbe->builder.CreateStore(rv, lv, v_w);

        if (getLeft()->getType()->isPointer())
            store->setAlignment(sizeof(void *));
    }

    if (getAddr || lt->isStruct())
        return lv;

    return llbe->builder.CreateLoad(lv, v_r, "assign_load");
}

void * AddAssignExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd, true);
    llvm::Value * addv = (llvm::Value *)((AddExpression *)this)
                             ->AddExpression::generate(backEnd);

    return llbe->builder.CreateStore(addv, lv);
}

void * SubAssignExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd, true);

    return llbe->builder.CreateStore((llvm::Value *)((SubExpression *)this)
                                         ->SubExpression::generate(backEnd),
                                     lv);
}

void * MultAssignExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd, true);

    return llbe->builder.CreateStore((llvm::Value *)((MultExpression *)this)
                                         ->MultExpression::generate(backEnd),
                                     lv);
}

void * DivAssignExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd, true);

    return llbe->builder.CreateStore((llvm::Value *)((DivExpression *)this)
                                         ->DivExpression::generate(backEnd),
                                     lv);
}

void * ModAssignExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd, true);

    return llbe->builder.CreateStore((llvm::Value *)((ModExpression *)this)
                                         ->ModExpression::generate(backEnd),
                                     lv);
}

void * MaybeAssignExpression::generate(BackEnd & backEnd, bool flag) {
    // @incomplete
    return nullptr;
}

void * LssExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * myt =
        conv(getLeft()->getType(), getRight()->getType())->unRef();

    llvm::Value * lv = (llvm::Value *)llbe->getOrGenNode(getLeft());
    llvm::Value * rv = (llvm::Value *)llbe->getOrGenNode(getRight());

    if (myt->isInt()) {
        const IntType * myit = (const IntType *)myt;
        if (myit->sign == Type::SIGNED)
            return llbe->builder.CreateICmpSLT(lv, rv);
        else
            return llbe->builder.CreateICmpULT(lv, rv);
    } else if (myt->isChar()) {
        return llbe->builder.CreateICmpULT(lv, rv);
    } else if (myt->isFloat()) {
        return llbe->builder.CreateFCmpULT(lv, rv);
    } else if (myt->isPointer()) {
        lv = llbe->builder.CreatePtrToInt(
            lv, llvm::Type::getInt64Ty(llbe->llContext));
        rv = llbe->builder.CreatePtrToInt(
            rv, llvm::Type::getInt64Ty(llbe->llContext));

        return llbe->builder.CreateICmpULT(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * LeqExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * myt =
        conv(getLeft()->getType(), getRight()->getType())->unRef();

    llvm::Value * lv = (llvm::Value *)llbe->getOrGenNode(getLeft());
    llvm::Value * rv = (llvm::Value *)llbe->getOrGenNode(getRight());

    if (myt->isInt()) {
        const IntType * myit = (const IntType *)myt;
        if (myit->sign == Type::SIGNED)
            return llbe->builder.CreateICmpSLE(lv, rv);
        else
            return llbe->builder.CreateICmpULE(lv, rv);
    } else if (myt->isChar()) {
        return llbe->builder.CreateICmpULE(lv, rv);
    } else if (myt->isFloat()) {
        return llbe->builder.CreateFCmpULE(lv, rv);
    } else if (myt->isPointer()) {
        lv = llbe->builder.CreatePtrToInt(
            lv, llvm::Type::getInt64Ty(llbe->llContext));
        rv = llbe->builder.CreatePtrToInt(
            rv, llvm::Type::getInt64Ty(llbe->llContext));

        return llbe->builder.CreateICmpULE(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * GtrExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * myt =
        conv(getLeft()->getType(), getRight()->getType())->unRef();

    llvm::Value * lv = (llvm::Value *)llbe->getOrGenNode(getLeft());
    llvm::Value * rv = (llvm::Value *)llbe->getOrGenNode(getRight());

    if (myt->isInt()) {
        const IntType * myit = (const IntType *)myt;
        if (myit->sign == Type::SIGNED)
            return llbe->builder.CreateICmpSGT(lv, rv);
        else
            return llbe->builder.CreateICmpUGT(lv, rv);
    } else if (myt->isChar()) {
        return llbe->builder.CreateICmpUGT(lv, rv);
    } else if (myt->isFloat()) {
        return llbe->builder.CreateFCmpUGT(lv, rv);
    } else if (myt->isPointer()) {
        lv = llbe->builder.CreatePtrToInt(
            lv, llvm::Type::getInt64Ty(llbe->llContext));
        rv = llbe->builder.CreatePtrToInt(
            rv, llvm::Type::getInt64Ty(llbe->llContext));

        return llbe->builder.CreateICmpUGT(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * GeqExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * myt =
        conv(getLeft()->getType(), getRight()->getType())->unRef();

    llvm::Value * lv = (llvm::Value *)llbe->getOrGenNode(getLeft());
    llvm::Value * rv = (llvm::Value *)llbe->getOrGenNode(getRight());

    if (myt->isInt() || myt->isChar()) {
        const IntType * myit = (const IntType *)myt;
        if (myit->sign == Type::SIGNED)
            return llbe->builder.CreateICmpSGE(lv, rv);
        else
            return llbe->builder.CreateICmpUGE(lv, rv);
    } else if (myt->isFloat()) {
        return llbe->builder.CreateFCmpUGE(lv, rv);
    } else if (myt->isPointer()) {
        lv = llbe->builder.CreatePtrToInt(
            lv, llvm::Type::getInt64Ty(llbe->llContext));
        rv = llbe->builder.CreatePtrToInt(
            rv, llvm::Type::getInt64Ty(llbe->llContext));

        return llbe->builder.CreateICmpUGE(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * EquExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * lt = getLeft()->getType()->unRef();
    // const Type * rt = getRight()->getType();

    llvm::Value * lv = (llvm::Value *)llbe->getOrGenNode(getLeft());
    llvm::Value * rv = (llvm::Value *)llbe->getOrGenNode(getRight());

    if (lt->isBool() || lt->isInt() || lt->isChar() || lt->isEnum()) {
        return llbe->builder.CreateICmpEQ(lv, rv);
    } else if (lt->isFloat()) {
        return llbe->builder.CreateFCmpUEQ(lv, rv);
    } else if (lt->isPointer()) {
        return llbe->builder.CreateICmpEQ(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * NeqExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * lt = getLeft()->getType()->unRef();
    // const Type * rt = getRight()->getType();

    llvm::Value * lv = (llvm::Value *)llbe->getOrGenNode(getLeft());
    llvm::Value * rv = (llvm::Value *)llbe->getOrGenNode(getRight());

    if (lt->isInt() || lt->isChar() || lt->isEnum()) {
        return llbe->builder.CreateICmpNE(lv, rv);
    } else if (lt->isFloat()) {
        return llbe->builder.CreateFCmpUNE(lv, rv);
    } else if (lt->isPointer()) {
        return llbe->builder.CreateICmpNE(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * LogAndExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Function * func = llbe->builder.GetInsertBlock()->getParent();

    llvm::Value * cond1 = (llvm::Value *)llbe->getOrGenNode(getLeft());
    cond1 = llbe->builder.CreateICmpEQ(
        cond1, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)),
        "lhs_cond");

    // Create blocks for the then and else cases.  Insert the 'then' block at
    // the end of the function.
    llvm::BasicBlock * then =
        llvm::BasicBlock::Create(llbe->llContext, "then", func);
    llvm::BasicBlock * condfalse =
        llvm::BasicBlock::Create(llbe->llContext, "condfalse", func);
    llvm::BasicBlock * mergephi =
        llvm::BasicBlock::Create(llbe->llContext, "mergephi", func);

    llbe->builder.CreateCondBr(cond1, then, mergephi);

    llvm::BasicBlock * origin_exit = llbe->builder.GetInsertBlock();

    // Emit then block.
    llbe->builder.SetInsertPoint(then);

    llvm::Value * cond2 = (llvm::Value *)llbe->getOrGenNode(getRight());
    cond2 = llbe->builder.CreateICmpEQ(
        cond2, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)),
        "rhs_cond");

    llbe->builder.CreateCondBr(cond2, mergephi, condfalse);

    llvm::BasicBlock * then_exit = llbe->builder.GetInsertBlock();

    // Emit condfalse block.
    llbe->builder.SetInsertPoint(condfalse);

    llbe->builder.CreateBr(mergephi);

    // Emit mergephi block.
    llbe->builder.SetInsertPoint(mergephi);

    llvm::PHINode * phi = llbe->builder.CreatePHI(
        llvm::Type::getInt1Ty(llbe->llContext), 3, "lazyphi");
    phi->addIncoming(llvm::ConstantInt::getFalse(llbe->llContext), origin_exit);
    phi->addIncoming(llvm::ConstantInt::getTrue(llbe->llContext), then_exit);
    phi->addIncoming(llvm::ConstantInt::getFalse(llbe->llContext), condfalse);

    return phi;
}

void * LogOrExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Function * func = llbe->builder.GetInsertBlock()->getParent();

    llvm::Value * cond1 = (llvm::Value *)llbe->getOrGenNode(getLeft());
    cond1 = llbe->builder.CreateICmpEQ(
        cond1, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)),
        "lhs_cond");

    // Create blocks for the then and else cases.  Insert the 'then' block at
    // the end of the function.
    llvm::BasicBlock * _else =
        llvm::BasicBlock::Create(llbe->llContext, "_else", func);
    llvm::BasicBlock * condfalse =
        llvm::BasicBlock::Create(llbe->llContext, "condfalse", func);
    llvm::BasicBlock * mergephi =
        llvm::BasicBlock::Create(llbe->llContext, "mergephi", func);

    llbe->builder.CreateCondBr(cond1, mergephi, _else);

    llvm::BasicBlock * origin_exit = llbe->builder.GetInsertBlock();

    // Emit _else block.
    llbe->builder.SetInsertPoint(_else);

    llvm::Value * cond2 = (llvm::Value *)llbe->getOrGenNode(getRight());
    cond2 = llbe->builder.CreateICmpEQ(
        cond2, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)),
        "rhs_cond");

    llbe->builder.CreateCondBr(cond2, mergephi, condfalse);

    llvm::BasicBlock * _else_exit = llbe->builder.GetInsertBlock();

    // Emit condfalse block.
    llbe->builder.SetInsertPoint(condfalse);

    llbe->builder.CreateBr(mergephi);

    // Emit mergephi block.
    llbe->builder.SetInsertPoint(mergephi);

    llvm::PHINode * phi = llbe->builder.CreatePHI(
        llvm::Type::getInt1Ty(llbe->llContext), 3, "lazyphi");
    phi->addIncoming(llvm::ConstantInt::getTrue(llbe->llContext), origin_exit);
    phi->addIncoming(llvm::ConstantInt::getTrue(llbe->llContext), _else_exit);
    phi->addIncoming(llvm::ConstantInt::getFalse(llbe->llContext), condfalse);

    return phi;
}

void * BANDExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd);
    llvm::Value * rv = (llvm::Value *)getRight()->generate(backEnd);

    return llbe->builder.CreateAnd(lv, rv);
}

void * BORExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd);
    llvm::Value * rv = (llvm::Value *)getRight()->generate(backEnd);

    return llbe->builder.CreateOr(lv, rv);
}

void * BXORExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd);
    llvm::Value * rv = (llvm::Value *)getRight()->generate(backEnd);

    return llbe->builder.CreateXor(lv, rv);
}

llvm::Value * fromVec(std::vector<llvm::Value *> & vec, int idx) {
    return vec[idx];
}

void * CallExpression::generate(BackEnd & backEnd, bool getAddr) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;
    ArgList * arglist = (ArgList *)getRight();
    std::vector<llvm::Value *> args;

    const Type * lt = getLeft()->getType();
    BJOU_DEBUG_ASSERT(lt->isProcedure());
    ProcedureType * plt = (ProcedureType *)lt;

    // @abi
    ABILowerProcedureTypeData * payload = new ABILowerProcedureTypeData;
    llvm::Value * sret = nullptr;
    payload->t = plt;
    llbe->abi_lowerer->ABILowerProcedureType(payload);
    if (payload->sret) {
        sret = llbe->allocUnnamedVal(plt->retType);
        args.push_back(sret);
    }
    //

    llvm::Value * callee = nullptr;

    if (resolved_proc)
        callee = llbe->getOrGenNode(resolved_proc);

    std::vector<int> byvals;
    std::vector<int> byrefs;
    std::vector<int> ptralign;
    int i = payload->sret;
    for (ASTNode * arg : arglist->getExpressions()) {
        llvm::Value * val = nullptr;
        if (payload->ref & (1 << i))
            byrefs.push_back(i);
        // could be vararg
        if ((i - payload->sret) < plt->getParamTypes().size() &&
            plt->getParamTypes()[i - payload->sret]->isRef()) {

            const Type * t = plt->getParamTypes()[i - payload->sret];

            val = llbe->getOrGenNode(arg, true);

            if (t->unRef()->isStruct() &&
                !equal(t->unRef(), arg->getType()->unRef()))
                val = llbe->builder.CreateBitCast(val, llbe->getOrGenType(t));
        } else {
            bool getArgAddr = false;

            if (arg->getType()->isRef() && arg->getType()->unRef()->isArray())
                getArgAddr = true;

            if (payload->byval & (1 << i)) {
                getArgAddr = true;
                byvals.push_back(i);
            }

            val = llbe->getOrGenNode(arg, getArgAddr);

            if (arg->getType()->isRef() && arg->getType()->unRef()->isArray()) {
                const Type * elem_t = arg->getType()->unRef()->under();
                val = llbe->builder.CreateBitCast(
                    val, llbe->getOrGenType(elem_t)->getPointerTo());
            }

            if (!getArgAddr) {
                if (val->getType()->isPointerTy())
                    ptralign.push_back(i);
            }
        }

        BJOU_DEBUG_ASSERT(val);

        args.push_back(val);
        i += 1;
    }

    if (!callee)
        callee = (llvm::Value *)llbe->getOrGenNode(getLeft());

    BJOU_DEBUG_ASSERT(callee);

    /*
     * variadic functions have promotion rules:
     * int types smaller than int -> int
     * float -> double
     */
    llvm::FunctionType * f_t = (llvm::FunctionType*)callee->getType()->getPointerElementType();
    if (f_t->isVarArg()) {
        for (int i = f_t->getNumParams(); i < args.size(); i += 1) {
            llvm::Value * arg = args[i];
            llvm::Type * arg_t = arg->getType();
            if (arg_t->isFloatTy()) {
                args[i] = llbe->builder.CreateFPExt(arg, llvm::Type::getDoubleTy(llbe->llContext));
            } else if (arg_t->isIntegerTy()) {
                llvm::IntegerType * int_t  = (llvm::IntegerType*)llvm::Type::getInt32Ty(llbe->llContext);
                llvm::IntegerType * this_t = (llvm::IntegerType*)arg_t;
                if (this_t->getIntegerBitWidth() < int_t->getIntegerBitWidth()) {
                    if (((IntType*)arglist->getExpressions()[i]->getType())->sign == Type::Sign::SIGNED) {
                        args[i] = llbe->builder.CreateSExt(arg, int_t);
                    } else {
                        args[i] = llbe->builder.CreateZExt(arg, int_t);
                    }
                }
            }
        }
    }

    llvm::CallInst * callinst = llbe->builder.CreateCall(callee, args);

    if (!payload->sret) {
        if (payload->t->getRetType()->isRef()) {
            const Type * ret_t = payload->t->getRetType()->unRef();
            unsigned int size = simpleSizer(ret_t);
            if (!size)
                size = 1;
            callinst->addAttribute(
                0, llvm::Attribute::getWithDereferenceableBytes(llbe->llContext,
                                                                size));
        } else if (payload->t->getRetType()->isPointer() ||
                   payload->t->getRetType()->isArray()) {
            // callinst->addAttribute(0, llvm::Attribute::getWithAlignment(
            //                               llbe->llContext, sizeof(void *)));
        }
    }

    llvm::Value * ret = callinst;

    if (payload->sret) {
        if (!getAddr)
            ret = llbe->builder.CreateLoad(sret);
        else
            ret = sret;
    } else {
        if (payload->t->getRetType()->isStruct() && getAddr) {
            llvm::Value * tmp_ret =
                llbe->allocUnnamedVal(payload->t->getRetType());
            llbe->builder.CreateStore(ret, tmp_ret);

            ret = tmp_ret;
        }
    }

    for (int ibyval : byvals) {
#if LLVM_VERSION_MAJOR > 4
        callinst->addParamAttr(ibyval, llvm::Attribute::ByVal);
#else
        auto attrs = callinst->getAttributes();
        attrs = attrs.addAttribute(llbe->llContext, ibyval + 1,
                                   llvm::Attribute::ByVal);
        callinst->setAttributes(attrs);
#endif
    }
    for (int ibyref : byrefs) {
        const Type * param_t = payload->t->getParamTypes()[ibyref]->unRef();
        unsigned int size = simpleSizer(param_t);
        if (!size)
            size = 1;

        llvm::Attribute attr =
            llvm::Attribute::getWithDereferenceableBytes(llbe->llContext, size);

#if LLVM_VERSION_MAJOR > 4
        callinst->addParamAttr(ibyref, attr);
#else
        auto attrs = callinst->getAttributes();
        attrs = attrs.addAttribute(llbe->llContext, ibyref + 1, attr);
        callinst->setAttributes(attrs);
#endif
    }
    for (int al : ptralign) {
        // callinst->addParamAttr(al, llvm::Attribute::getWithAlignment(
        //                               llbe->llContext, sizeof(void *)));
    }

    if (!getAddr && payload->t->getRetType()->isRef())
        ret = llbe->builder.CreateLoad(ret, "ref");

    delete payload;

    return ret;
}

static llvm::Value * gep_by_index(BackEnd & backEnd, llvm::Type * t,
                                  llvm::Value * val, int index) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;
    std::vector<llvm::Value *> indices;

    indices.push_back(
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), 0));
    indices.push_back(
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), index));

    return llbe->builder.CreateInBoundsGEP(t, val, indices);
}

void * SubscriptExpression::generate(BackEnd & backEnd, bool getAddr) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * lt = getLeft()->getType()->unRef();
    const Type * elem_t = getType();
    llvm::Value *lv = nullptr, *rv = nullptr;
    std::vector<llvm::Value *> indices;

    if (!lt->isPointer() && !lt->isArray()) {
        BJOU_DEBUG_ASSERT(false && "Invalid subscript type");
        return nullptr;
    }

    if (lt->isArray()) {
        std::vector<unsigned int> widths,
            steps; // number of steps per dimension
                   // int[2][2][3] -> { 1, 3, 6 }
        std::vector<llvm::Value *> indices;

        auto multiplicative_sum = [&](std::vector<unsigned int> & vec,
                                      int take) {
            unsigned int sum = 1;
            for (int i = 0; i < take; i += 1)
                sum *= vec[i];
            return sum;
        };

        ASTNode *s = this->getLeft(), *l = nullptr;

        // find bottom of subscript chain
        while (s && s->nodeKind == ASTNode::SUBSCRIPT_EXPRESSION)
            s = ((Expression *)s)->getLeft();
        // s is the array.. save it
        ASTNode * the_array = s;
        // go back up one to find the last subscript
        s = s->parent;
        // go back up, gathering widths
        while (s != this->parent) {
            Expression * e = (Expression *)s;
            const Type * t = e->getLeft()->getType();
            if (!t->isArray())
                break;
            ArrayType * a_t = (ArrayType *)t;

            widths.push_back(a_t->width);

            s = s->parent;
        }

        // this level
        // first step is always 1
        steps.push_back(1);
        indices.push_back(llbe->getOrGenNode(getRight()));

        s = this->getLeft();
        l = this->getLeft();
        // traverse down
        int take = 1;
        while (s && s->nodeKind == ASTNode::SUBSCRIPT_EXPRESSION) {
            Expression * e = (Expression *)s;
            const Type * t = e->getLeft()->getType();
            if (!t->isArray())
                break;
            ArrayType * a_t = (ArrayType *)t;

            steps.push_back(multiplicative_sum(widths, take++));

            widths.push_back(a_t->width);

            indices.push_back(llbe->getOrGenNode(e->getRight()));

            l = e->getLeft();

            s = l;
        }

        BJOU_DEBUG_ASSERT(the_array);

        lv = llbe->getOrGenNode(the_array);

        llvm::Value * sum_value = indices[0];
        for (int i = 1; i < (int)indices.size(); i += 1) {
            llvm::Value * tmp = indices[1];
            llvm::Value * step = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(llbe->llContext), steps[i]);
            tmp = llbe->builder.CreateMul(tmp, step);
            sum_value = llbe->builder.CreateAdd(sum_value, tmp);
        }

        rv = sum_value;
    } else {
        lv = (llvm::Value *)llbe->getOrGenNode(getLeft());
        rv = (llvm::Value *)llbe->getOrGenNode(getRight());
    }

    BJOU_DEBUG_ASSERT(lv);
    BJOU_DEBUG_ASSERT(rv);

    indices.push_back(rv);

    llvm::Value * gep = llbe->builder.CreateInBoundsGEP(lv, indices);

    if (elem_t->isArray())
        getAddr = true;

    if (getAddr)
        return gep;
    return llbe->builder.CreateLoad(gep, "subscript");
}

void * AccessExpression::generate(BackEnd & backEnd, bool getAddr) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;
    std::vector<llvm::Value *> indices;
    llvm::Value * left_val = nullptr;
    int elem = -1;
    std::string name;

    const Type * t = getLeft()->getType()->unRef();

    // to accommodate the way LenExpression desugars
    if (t->isSlice())
        t = ((SliceType *)t)->getRealType();
    else if (t->isDynamicArray())
        t = ((DynamicArrayType *)t)->getRealType();

    StructType * s_t = nullptr;
    TupleType * t_t = nullptr;
    Identifier * r_id = (Identifier *)getRight();
    IntegerLiteral * r_elem = (IntegerLiteral *)getRight();

    if (t->isSlice())
        t = ((SliceType *)t)->getRealType();
    else if (t->isDynamicArray())
        t = ((DynamicArrayType *)t)->getRealType();

    if (t->isPointer()) {
        PointerType * pt = (PointerType *)t;
        left_val = (llvm::Value *)llbe->getOrGenNode(getLeft());
        if (pt->under()->isStruct()) {
            s_t = (StructType *)t->under();
            // @refactor type member constants
            // should this be here or somewhere in the frontend?

            if (s_t->constantMap.count(r_id->getSymName()))
                return llbe->getOrGenNode(
                    s_t->constantMap[r_id->getSymName()]);

            // regular structure access

            name = "structure_access";
            elem = s_t->memberIndices[r_id->getSymName()];
        } else if (pt->under()->isTuple()) {
            name = "tuple_access";
            t_t = (TupleType *)t->under();
            elem = (int)r_elem->eval().as_i64;
        }
    } else if (t->isStruct()) {
        s_t = (StructType *)t;
        // @refactor type member constants
        // should this be here or somewhere in the frontend?

        if (s_t->constantMap.count(r_id->getSymName()))
            return llbe->getOrGenNode(s_t->constantMap[r_id->getSymName()]);

        // regular structure access

        name = "structure_access";
        left_val = (llvm::Value *)llbe->getOrGenNode(getLeft(), true);
        elem = s_t->memberIndices[r_id->getSymName()];
    } else if (t->isTuple()) {
        name = "tuple_access";
        left_val = (llvm::Value *)llbe->getOrGenNode(getLeft(), true);
        t_t = (TupleType *)t;
        elem = (int)r_elem->eval().as_i64;
    } else
        BJOU_DEBUG_ASSERT(false);

    indices.push_back(
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), 0));
    indices.push_back(
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), elem));

    llvm::Value * access = llbe->builder.CreateInBoundsGEP(left_val, indices);

    BJOU_DEBUG_ASSERT(access->getType()->isPointerTy());

    if (access->getType()->getPointerElementType()->isArrayTy())
        access = llbe->createPointerToArrayElementsOnStack(access, getType());

    if (getType()->isRef())
        access = llbe->builder.CreateLoad(access, "ref");
    if (getAddr)
        return access;

    llvm::LoadInst * load = llbe->builder.CreateLoad(access, name);

    if (getType()->isPointer())
        load->setAlignment(sizeof(void *));

    return load;
}

void * NewExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    if (compilation->frontEnd.malloc_decl)
        llbe->getOrGenNode(compilation->frontEnd.malloc_decl);
    else
        errorl(getContext(), "bJou is missing a malloc declaration.", true,
               "if using --nopreload, an extern declaration must be made "
               "available");

    llvm::Function * func = llbe->llModule->getFunction("malloc");
    BJOU_DEBUG_ASSERT(func);

    const Type * r_t = getRight()->getType();
    uint64_t size;
    std::vector<llvm::Value *> args;
    llvm::Value * size_val = nullptr;

    if (r_t->isArray()) {
        const ArrayType * array_t = (const ArrayType *)r_t;
        const Type * sub_t = array_t->under();

        ArrayDeclarator * array_decl = (ArrayDeclarator *)getRight();

        size = llbe->layout->getTypeAllocSize(llbe->getOrGenType(sub_t));

        size_val = llvm::ConstantInt::get(
            llvm::Type::getInt64Ty(llbe->llContext), size);

        llvm::Value * width_val =
            (llvm::Value *)llbe->getOrGenNode(array_decl->getExpression());
        width_val = llbe->builder.CreateIntCast(
            width_val, llvm::IntegerType::getInt64Ty(llbe->llContext), true,
            "width");

        size_val = llbe->builder.CreateMul(size_val, width_val);

    } else {
        size = llbe->layout->getTypeAllocSize(llbe->getOrGenType(r_t));
        size_val = llvm::ConstantInt::get(
            llvm::Type::getInt64Ty(llbe->llContext), size);
    }

    args.push_back(size_val);

    llvm::Value * val = llbe->builder.CreateBitCast(
        llbe->builder.CreateCall(func, args), llbe->getOrGenType(getType()));

    if (r_t->isArray()) {
        // @incomplete
    }

    return val;
}

void * DeleteExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Function * func = llbe->llModule->getFunction("free");
    BJOU_DEBUG_ASSERT(func);

    std::vector<llvm::Value *> args = {llbe->builder.CreateBitCast(
        (llvm::Value *)llbe->getOrGenNode(getRight()),
        llvm::Type::getInt8PtrTy(llbe->llContext))};

    return llbe->builder.CreateCall(func, args);
}

void * SizeofExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * rt = getRight()->getType();

    BJOU_DEBUG_ASSERT(rt != VoidType::get());

    uint64_t size = llbe->layout->getTypeAllocSize(
        llbe->getOrGenType(getRight()->getType()));

    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(llbe->llContext),
                                  size);
}

void * NotExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    return llbe->builder.CreateNot(
        (llvm::Value *)llbe->getOrGenNode(getRight()));
}

void * BNEGExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * rv = (llvm::Value *)getRight()->generate(backEnd);

    return llbe->builder.CreateNot(rv);
}

void * DerefExpression::generate(BackEnd & backEnd, bool getAddr) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * rv = (llvm::Value *)llbe->getOrGenNode(getRight());

    if (getAddr)
        return rv;
    return llbe->builder.CreateLoad(rv, "deref");
}

void * AddressExpression::generate(BackEnd & backEnd, bool flag) {
    return ((LLVMBackEnd *)&backEnd)->getOrGenNode(getRight(), true);
}

void * RefExpression::generate(BackEnd & backEnd, bool flag) {
    BJOU_DEBUG_ASSERT(false);
    return nullptr;
    // return getRight().generate(backEnd, !getRight()->getType()->isRef());
}

#if 0
    const Type * conv(const Type * l, const Type * r) {
    // one of l and r is fp, then we choose the fp type
    if (l->isFloat() && !r->isFloat())
        return l;
    if (!l->isFloat() && r->isFloat())
        return r;
    // if both, choose the larger
    if (l->isFloat() && r->isFloat()) {
        if (l->size >= r->size)
            return l;
        else return r;
    }
    
    // both integer types
    Type::Sign sign = Type::Sign::UNSIGNED;
    if (l->sign == Type::Sign::SIGNED || r->sign == Type::Sign::SIGNED)
        sign = Type::Sign::SIGNED;
        int size = l->size >= r->size ? l->size : r->size;
        
        if (size == -1)
            return compilation->frontEnd.typeTable["void"];
    
    char buf[32];
    sprintf(buf, "%c%d", (sign == Type::Sign::SIGNED ? 'i' : 'u'), size);
    
    BJOU_DEBUG_ASSERT(compilation->frontEnd.typeTable.count(buf) > 0);
    
    return compilation->frontEnd.typeTable[buf];
}
#endif

void * AsExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * lt = getLeft()->getType()->unRef();
    const Type * rt = getRight()->getType();

    if (lt->isEnum())
        lt = IntType::get(Type::Sign::UNSIGNED, 64);
    if (rt->isEnum())
        rt = IntType::get(Type::Sign::UNSIGNED, 64);

    llvm::Value * val = (llvm::Value *)llbe->getOrGenNode(getLeft());
    llvm::Type * ll_rt = llbe->getOrGenType(rt);

    if (lt->isInt() && rt->isInt()) {
        return llbe->builder.CreateIntCast(
            val, ll_rt, ((const IntType *)rt)->sign == Type::SIGNED);
    } else if (lt->isBool() && (rt->isInt() || rt->isChar())) {
        return llbe->builder.CreateIntCast(val, ll_rt, 0, "booltoint");
    } else if ((lt->isInt() || lt->isChar()) && rt->isBool()) {
        return llbe->builder.CreateICmpNE(val, llvm::ConstantInt::get(llbe->getOrGenType(lt), 0, ((IntType*)lt)->sign == Type::SIGNED));
    } else if (lt->isInt() && rt->isChar()) {
        return llbe->builder.CreateIntCast(val, ll_rt, false);
    } else if (lt->isChar() && rt->isInt()) {
        return llbe->builder.CreateIntCast(val, ll_rt, false);
    } else if (lt->isChar() && rt->isFloat()) {
        return llbe->builder.CreateUIToFP(val, ll_rt);
    } else if (lt->isInt() && rt->isFloat()) {
        if (((const IntType *)lt)->sign == Type::SIGNED)
            return llbe->builder.CreateSIToFP(val, ll_rt);
        else
            return llbe->builder.CreateUIToFP(val, ll_rt);
    } else if (lt->isFloat() && rt->isInt()) {
        if (((const IntType *)rt)->sign == Type::SIGNED)
            return llbe->builder.CreateFPToSI(val, ll_rt);
        else
            return llbe->builder.CreateFPToUI(val, ll_rt);
    } else if (lt->isFloat() && rt->isFloat()) {
        if (((const FloatType *)lt)->width < ((const FloatType *)rt)->width)
            return llbe->builder.CreateFPExt(val, ll_rt);
        else
            return llbe->builder.CreateFPTrunc(val, ll_rt);
    } else if (lt->isInt() && rt->isPointer() && rt->under()->isVoid()) {
        return llbe->builder.CreateIntToPtr(val, ll_rt);
    } else if (lt->isPointer() && lt->under()->isVoid() && rt->isInt()) {
        return llbe->builder.CreatePtrToInt(val, ll_rt);
    }

    // @incomplete
    if (lt->isPointer() && rt->isPointer())
        return llbe->builder.CreateBitCast(val, ll_rt);
    else if (lt->isPointer() && lt->under() == VoidType::get() &&
             rt->isProcedure()) {
        return llbe->builder.CreateBitCast(val, ll_rt);
    } else if (lt->isProcedure() && rt->isPointer() &&
               rt->under() == VoidType::get()) {
        return llbe->builder.CreateBitCast(val, ll_rt);
    } else if (lt->isArray() && rt->isPointer()) {
        ArrayType * a_t = (ArrayType *)lt;
        PointerType * p_t = (PointerType *)rt;

        if (equal(a_t->under(), p_t->under()) ||
            p_t->under() == VoidType::get())
            return llbe->builder.CreateBitCast(val, ll_rt);
    }

    BJOU_DEBUG_ASSERT(false && "Did not create cast for AsExpression");

    return nullptr;
}

void * BooleanLiteral::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    if (getContents() == "true")
        return llvm::ConstantInt::getTrue(llbe->llContext);
    else if (getContents() == "false")
        return llvm::ConstantInt::getFalse(llbe->llContext);
    return nullptr;
}

void * IntegerLiteral::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * t = getType();
    llvm::Type * ll_t = llbe->getOrGenType(t);

    std::stringstream ss(getContents());
    uint64_t V;
    int64_t Vi;
    ss >> V;
    if (!ss) {
        ss.clear();
        ss.str(getContents());
        ss >> Vi;
        V = Vi;
    }

    return llvm::ConstantInt::get(ll_t, V,
                                  ((IntType *)t)->sign == Type::Sign::SIGNED);
}

void * FloatLiteral::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;
    return llvm::ConstantFP::get(llbe->getOrGenType(getType()), getContents());
}

void * CharLiteral::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;
    std::string & contents = getContents();
    char ch = get_ch_value(contents);
    return llvm::ConstantInt::get(llvm::Type::getInt8Ty(llbe->llContext), ch);
}

void * StringLiteral::generate(BackEnd & backEnd, bool flag) {
    std::string str = getContents();
    return ((LLVMBackEnd *)&backEnd)->createGlobalStringVariable(str);
}

void * TupleLiteral::generate(BackEnd & backEnd, bool getAddr) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * my_t = getType();
    llvm::StructType * ll_t = (llvm::StructType *)llbe->getOrGenType(my_t);

    llvm::Value * alloca = llbe->allocUnnamedVal(my_t);
    std::vector<ASTNode *> & subExpressions = getSubExpressions();
    for (size_t i = 0; i < subExpressions.size(); i += 1) {
        llvm::Value * elem = llbe->builder.CreateInBoundsGEP(
            ll_t, alloca,
            {llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), 0),
             llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext),
                                    i)});

        llbe->builder.CreateStore(
            (llvm::Value *)llbe->getOrGenNode(subExpressions[i], ((TupleType*)my_t)->getTypes()[i]->isRef()), elem);
    }

    if (getAddr) {
        return alloca;
    }
    return llbe->builder.CreateLoad(alloca, "tupleliteral");
}

void * ProcLiteral::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;
    Procedure * proc = (Procedure *)getRight();
    llvm::Function * func = (llvm::Function *)llbe->getOrGenNode(proc);
    BJOU_DEBUG_ASSERT(func);
    return func;
}

void * ExternLiteral::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;
    Procedure * proc = (Procedure *)getRight();
    llvm::Function * func = (llvm::Function *)llbe->getOrGenNode(proc);
    BJOU_DEBUG_ASSERT(func);
    return func;
}

void * Identifier::generate(BackEnd & backEnd, bool getAddr) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    TmpMangler mangled(sym_name, sym_mod, sym_type);
    llvm::Value * ptr = llbe->getNamedVal(mangled.real_mangled);

    if (!ptr && resolved) {
        ptr = llbe->getOrGenNode(resolved);
    }

    // There is a chance that when running code at compile time
    // things like global vars, constants, and procs will not have been
    // generated. In this case, we will do a symbol table lookup and gen the
    // node
    if (!ptr) {
        Maybe<Symbol *> m_sym = getScope()->getSymbol(
            getScope(), this, &getContext(), true, true, false);
        Symbol * sym = nullptr;
        m_sym.assignTo(sym);
        BJOU_DEBUG_ASSERT(sym);
        ptr = llbe->getOrGenNode(sym->node());
    }

    BJOU_DEBUG_ASSERT(ptr);

    // if the named value is not a stack allocation or global variable (i.e.
    // constant, function, etc.) we don't want the load instruction ever
    // @update unless it is a reference
    if (!llvm::isa<llvm::AllocaInst>(ptr) &&
        !llvm::isa<llvm::GlobalVariable>(ptr) &&
        !llvm::isa<llvm::Argument>(ptr) && !getType()->isRef())
        getAddr = true;
    // we shouldn't load direct function references
    else if (llvm::isa<llvm::Function>(ptr))
        getAddr = false;

    if (getAddr)
        return ptr;

    llvm::LoadInst * load = llbe->builder.CreateLoad(ptr, symAll());
    if (getType()->isPointer() || getType()->isRef())
        load->setAlignment(sizeof(void *));

    return load;
}

llvm::Value * LLVMBackEnd::getPointerToArrayElements(llvm::Value * array) {
    std::vector<llvm::Value *> indices{
        llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llContext)),
        llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llContext))};
    return builder.CreateInBoundsGEP(array, indices);
}

llvm::Value *
LLVMBackEnd::createPointerToArrayElementsOnStack(llvm::Value * array,
                                                 const Type * t) {
    BJOU_DEBUG_ASSERT(array->getType()->isPointerTy() &&
                      array->getType()->getPointerElementType()->isArrayTy());
    llvm::Value * ptr = allocUnnamedVal(t->under()->getPointer());
    builder.CreateStore(getPointerToArrayElements(array), ptr);
    return ptr;
}

llvm::Value *
LLVMBackEnd::copyConstantInitializerToStack(llvm::Constant * constant_init,
                                            const Type * t) {
    llvm::Value * alloca = allocUnnamedVal(t, false);
    builder.CreateStore(constant_init, alloca);
    return alloca;
}

llvm::Constant *
LLVMBackEnd::createConstantInitializer(InitializerList * ilist) {
    const Type * t = ilist->getType();
    llvm::Type * ll_t = getOrGenType(t);
    std::vector<ASTNode *> & expressions = ilist->getExpressions();

    if (t->isStruct()) {
        StructType * s_t = (StructType *)t;

        std::vector<std::string> & names = ilist->getMemberNames();
        std::vector<llvm::Constant *> vals(
            s_t->memberTypes.size(), nullptr);

        for (int i = 0; i < (int)names.size(); i += 1) {
            // we will assume everything works out since it passed analysis
            int index =  s_t->memberIndices[names[i]];
            vals[index] = (llvm::Constant *)getOrGenNode(expressions[i]);
        }
        for (int i = 0; i < (int)vals.size(); i += 1)
            if (!vals[i])
                vals[i] = llvm::Constant::getNullValue(
                    getOrGenType(s_t->memberTypes[i]));
        return llvm::ConstantStruct::get((llvm::StructType *)ll_t, vals);
    } else if (t->isArray()) {
        std::vector<llvm::Constant *> vals;
        for (ASTNode * expr : expressions)
            vals.push_back((llvm::Constant *)getOrGenNode(expr));
        llvm::Constant * c =
            llvm::ConstantArray::get((llvm::ArrayType *)ll_t, vals);
        return c;
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * InitializerList::generate(BackEnd & backEnd, bool getAddr) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * myt = getType();
    if (myt->isDynamicArray())
        myt = ((DynamicArrayType *)myt)->getRealType();
    if (myt->isSlice())
        myt = ((SliceType *)myt)->getRealType();

    std::vector<ASTNode *> & expressions = getExpressions();
    llvm::Type * ll_t = (llvm::Type *)llbe->getOrGenType(myt);
    llvm::AllocaInst * alloca = nullptr;
    llvm::Value * ptr = nullptr;
    llvm::Value * ldptr = nullptr;

    bool do_memset = false;

    if (myt->isStruct()) {
        StructType * s_t = (StructType *)myt;
        for (const Type * t : s_t->memberTypes) {
            if (t->isArray()) {
                ArrayType * a_t = (ArrayType *)t;
                // @bad, sort of arbitrary choice of threshold here
                // really we would like to know when the cost of
                // generating lots of moves becomes greater than generating
                // a memset
                if (a_t->width > 4)
                    do_memset = true;
            }
        }
    }

    if (!do_memset && isConstant()) {
        llvm::Constant * constant_init = llbe->createConstantInitializer(this);
        if (myt->isStruct()) {
            if (getAddr)
                return llbe->copyConstantInitializerToStack(constant_init, myt);
            return constant_init;
        } else if (myt->isArray()) {
            llvm::Value * onStack =
                llbe->copyConstantInitializerToStack(constant_init, myt);
            ptr = llbe->createPointerToArrayElementsOnStack(onStack, myt);
            if (getAddr)
                return ptr;
            return llbe->builder.CreateLoad(ptr, "array_ptr");
        }
        BJOU_DEBUG_ASSERT(false);
    }

    if (myt->isStruct()) {
        StructType * s_t = (StructType *)myt;

        std::vector<std::string> & names = getMemberNames();

        alloca = (llvm::AllocaInst *)llbe->allocUnnamedVal(myt);
        ptr = llbe->builder.CreateInBoundsGEP(
            alloca, {llvm::Constant::getNullValue(
                        llvm::IntegerType::getInt32Ty(llbe->llContext))});
        alloca->setName("structinitializer");

        BJOU_DEBUG_ASSERT(names.size() == expressions.size());

        std::vector<llvm::Value *> vals;
        vals.resize(s_t->memberTypes.size() , nullptr);

        for (int i = 0; i < (int)names.size(); i += 1) {
            int index = s_t->memberIndices[names[i]] ;
            const Type * dest_t =
                s_t->memberTypes[s_t->memberIndices[names[i]]];
            if (dest_t->isRef())
                vals[index] =
                    (llvm::Value *)llbe->getOrGenNode(expressions[i], true);
            else
                vals[index] = (llvm::Value *)llbe->getOrGenNode(expressions[i]);
        }

        // record uninitialized array members so that we can do a
        // memset to zero later
        std::map<size_t, ArrayType *> uninit_array_elems;
        for (size_t i = 0; i < s_t->memberTypes.size(); i += 1) {
            if (s_t->memberTypes[i]->isArray()) {
                size_t index = i ;
                if (!vals[index])
                    uninit_array_elems[index] =
                        (ArrayType *)s_t->memberTypes[i];
            }
        }

        for (size_t i = 0; i < vals.size(); i += 1) {
            if (uninit_array_elems.find(i) != uninit_array_elems.end())
                continue;

            llvm::Value * elem = llbe->builder.CreateInBoundsGEP(
                ptr, {llvm::Constant::getNullValue(
                          llvm::IntegerType::getInt32Ty(llbe->llContext)),
                      llvm::ConstantInt::get(
                          llvm::Type::getInt32Ty(llbe->llContext), i)});

            const Type * elem_t = s_t->memberTypes[i];

            if (!vals[i]) {
                llvm::Value * null_val =
                    llvm::Constant::getNullValue(llbe->getOrGenType(elem_t));
                llbe->builder.CreateStore(null_val, elem);
            } else {
                llvm::StoreInst * store =
                    llbe->builder.CreateStore(vals[i], elem);

                if (elem_t->isPointer() || elem_t->isRef())
                    store->setAlignment(sizeof(void *));
            }
        }

        // memset uninitialized array members to zero
        for (auto uninit : uninit_array_elems) {
            const Type * elem_t = uninit.second->under();

            llvm::PointerType * ll_byte_ptr_t =
                llvm::Type::getInt8Ty(llbe->llContext)->getPointerTo();

            llvm::Value *Ptr = nullptr, *Val = nullptr, *Size = nullptr;

            Ptr = llbe->builder.CreateInBoundsGEP(
                ptr,
                {llvm::Constant::getNullValue(
                     llvm::IntegerType::getInt32Ty(llbe->llContext)),
                 llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext),
                                        uninit.first)});

            Ptr = llbe->builder.CreateBitCast(Ptr, ll_byte_ptr_t);

            Val = llvm::Constant::getNullValue(
                llvm::IntegerType::getInt8Ty(llbe->llContext));

            Size = llvm::ConstantInt::get(
                llvm::Type::getInt64Ty(llbe->llContext),
                uninit.second->width *
                    llbe->layout->getTypeAllocSize(llbe->getOrGenType(elem_t)));

            auto Align = llbe->layout->getABITypeAlignment(llbe->getOrGenType(elem_t));

            llbe->builder.CreateMemSet(Ptr, Val, Size, Align);
        }

        if (getAddr || x86::ABIClassForType(*llbe, myt) == x86::MEMORY)
            return ptr;

        ldptr = llbe->builder.CreateLoad(alloca, "structinitializer_ld");
    } else if (myt->isArray()) {
        alloca = (llvm::AllocaInst *)llbe->allocUnnamedVal(myt);
        alloca->setName("arrayinitializer");
        ptr = llbe->createPointerToArrayElementsOnStack(alloca, myt);
        ldptr = llbe->builder.CreateLoad(ptr, "arrayptr");

        for (size_t i = 0; i < expressions.size(); i += 1) {
            llvm::Value * idx = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(llbe->llContext), i);
            llvm::Value * elem = llbe->builder.CreateInBoundsGEP(ldptr, idx);
            llbe->builder.CreateStore(
                (llvm::Value *)llbe->getOrGenNode(expressions[i]), elem);
        }

        if (getAddr)
            return ptr;
    }

    BJOU_DEBUG_ASSERT(ldptr);
    return ldptr;
}

void * Constant::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;
    if (getFlag(IS_TYPE_MEMBER)) {
        auto c = llbe->generatedTypeMemberConstants.find(this);
        if (c != llbe->generatedTypeMemberConstants.end())
            return c->second;
        llvm::Value * v =
            (llvm::Value *)llbe->getOrGenNode(getInitialization());
        BJOU_DEBUG_ASSERT(v);
        llbe->generatedTypeMemberConstants[this] = v;
        return v;
    }
    return llbe->addNamedVal(
        getMangledName(),
        (llvm::Value *)llbe->getOrGenNode(getInitialization()), getType());
}

void * VariableDeclaration::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    if (!getScope()->parent || getScope()->is_module_scope) {
            return GenerateGlobalVariable(this, nullptr, backEnd);
        /* if (llbe->generated_nodes.find(this) == llbe->generated_nodes.end()) { */
        /*     llbe->globs_need_completion.insert(this); */
        /*     return GenerateGlobalVariable(this, nullptr, backEnd); */
        /* } else { */
        /*     llvm::Value * me = llbe->getOrGenNode(this); */
        /*     return GenerateGlobalVariable(this, (llvm::GlobalVariable *)me, */
        /*                                   backEnd); */
        /* } */
    }

    llvm::Type * t = llbe->getOrGenType(getType());
    llvm::Value * val = nullptr;

    std::string val_name = getMangledName();

    if (getType()->isRef()) {
        BJOU_DEBUG_ASSERT(getInitialization());

        if (getType()->unRef()->isArray()) {
            val = llbe->allocNamedVal(
                val_name, getType()->unRef()->under()->getPointer());
            llbe->builder.CreateStore(
                llbe->getOrGenNode(getInitialization(), true), val);
        } else {
            val = llbe->addNamedVal(
                val_name,
                (llvm::Value *)llbe->getOrGenNode(getInitialization(), true),
                getType());
        }
    } else {
        val = llbe->allocNamedVal(val_name, getType());
        BJOU_DEBUG_ASSERT(val);

        if (getType()->isArray()) {
            // @incomplete
        } else {
            if (getType()->isStruct() || getType()->isSlice() ||
                getType()->isDynamicArray()) {

                StructType * s_t = (StructType *)getType();

                if (getType()->isSlice())
                    s_t = (StructType *)((SliceType *)getType())->getRealType();
                if (getType()->isDynamicArray())
                    s_t = (StructType *)((DynamicArrayType *)getType())
                              ->getRealType();
            }
            //
        }

        if (getInitialization() && !getType()->isRef()) {
            if (getType()->isStruct()) {
                llvm::Type * ll_t = llbe->getOrGenType(getType());

                llvm::Value * init_v = (llvm::Value *)llbe->getOrGenNode(
                    getInitialization(), true);

#if LLVM_VERSION_MAJOR >= 7
                llbe->builder.CreateMemCpy(
                    val, llbe->layout->getABITypeAlignment(ll_t),
                    init_v, llbe->layout->getABITypeAlignment(ll_t),
                    llbe->layout->getTypeAllocSize(ll_t));
#else
                llbe->builder.CreateMemCpy(
                    val, init_v, llbe->layout->getTypeAllocSize(ll_t),
                    llbe->layout->getABITypeAlignment(ll_t));
#endif

            } else {
                llvm::Value * init_v =
                    (llvm::Value *)llbe->getOrGenNode(getInitialization());
                llbe->builder.CreateStore(init_v, val);
            }
        }
    }

    BJOU_DEBUG_ASSERT(val);

    return val;
}

static void gen_printf_fmt(BackEnd & backEnd, llvm::Value * val, const Type * t,
                           std::string & fmt,
                           std::vector<llvm::Value *> & vals) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    if (t->isStruct()) {
        // val should be a ptr!!

        StructType * s_t = (StructType *)t;

        fmt += "{ ";
        for (int i = 0; i < (int)s_t->memberTypes.size(); i += 1) {
            fmt += ".";
            for (auto & mem : s_t->memberIndices) {
                if (mem.second == i) {
                    fmt += mem.first;
                    break;
                }
            }
            fmt += " = ";
            const Type * mem_t = s_t->memberTypes[i];
            llvm::Value * mem_val = gep_by_index(
                backEnd, llbe->getOrGenType(s_t), val, i );
            if (mem_t->isArray()) {
                mem_val = gep_by_index(
                    backEnd, mem_val->getType()->getPointerElementType(),
                    mem_val, 0);
            } else if (!mem_t->isStruct()) {
                mem_val = llbe->builder.CreateLoad(mem_val);
            }
            gen_printf_fmt(backEnd, mem_val, s_t->memberTypes[i], fmt, vals);
            if (i < (int)s_t->memberTypes.size() - 1)
                fmt += ", ";
        }
        fmt += " }";
    } else {
        if (t->isBool()) {
            fmt += "%d";
        } else if (t->isInt()) {
            IntType * it = (IntType *)t;
            if (it->width <= 32) {
                if (it->sign == Type::UNSIGNED)
                    fmt += "%u";
                else
                    fmt += "%d";
            } else {
                if (it->sign == Type::UNSIGNED)
                    fmt += "%llu";
                else
                    fmt += "%lld";
            }
        } else if (t->isEnum()) {
            fmt += "%llu";
        } else if (t->isFloat()) {
            if (((FloatType *)t)->width == 32)
                fmt += "%f";
            else
                fmt += "%g";
        } else if (t->isChar()) {
            fmt += "%c";
        } else if (t->isPointer()) {
            if (t->under() == CharType::get())
                fmt += "%s";
            else
                fmt += "%p";
        } else if (t->isProcedure()) {
            fmt += "%p";
        } else if (t->isArray() && t->under() == CharType::get()) {
            fmt += "%s";
        } else {
            internalError("Can't print type '" + t->key + "'."); // @temporary
        }
        
        /* printf is vararg -- do promotion */
        if (val->getType()->isFloatTy()) {
            val = llbe->builder.CreateFPExt(val, llvm::Type::getDoubleTy(llbe->llContext));
        } else if (val->getType()->isIntegerTy()) {
            llvm::IntegerType * this_t = (llvm::IntegerType*)val->getType();
            llvm::IntegerType * int_t  = (llvm::IntegerType*)llvm::Type::getInt32Ty(llbe->llContext);
            if (this_t->getIntegerBitWidth() < int_t->getIntegerBitWidth()) {
                if (((IntType*)t)->sign == Type::Sign::SIGNED) {
                    val = llbe->builder.CreateSExt(val, int_t);
                } else {
                    val = llbe->builder.CreateZExt(val, int_t);
                }
            }
        }

        vals.push_back(val);
    }
}

void * Print::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    if (compilation->frontEnd.printf_decl)
        llbe->getOrGenNode(compilation->frontEnd.printf_decl);
    else
        errorl(getContext(), "bJou is missing a printf declaration.", true,
               "if using --nopreload, an extern declaration must be made "
               "available");

    std::string fmt;

    ArgList * args = (ArgList *)getArgs();
    std::string pre_fmt =
        ((Expression *)args->getExpressions()[0])->getContents();

    std::vector<llvm::Value *> vals = {nullptr};

    int arg = 1;
    for (int c = 0; c < (int)pre_fmt.size(); c++) {
        if (pre_fmt[c] == '%') {
            Expression * expr = (Expression *)args->expressions[arg];
            const Type * t = expr->getType();
            llvm::Value * val = nullptr;
            if (t->isRef()) {
                val = (llvm::Value *)llbe->getOrGenNode(expr, true);
                if (!t->unRef()->isStruct()) {
                    val = llbe->builder.CreateLoad(val, "loadRef");
                }
                t = t->unRef();
            } else if (t->isStruct()) {
                val = (llvm::Value *)llbe->getOrGenNode(expr, true);
            } else {
                val = (llvm::Value *)llbe->getOrGenNode(expr);
            }

            gen_printf_fmt(backEnd, val, t, fmt, vals);

            arg++;
        } else
            fmt += pre_fmt[c];
    }

    fmt += "\n";

    vals[0] = llbe->createGlobalStringVariable(fmt);

    llvm::Function * pf = llbe->llModule->getFunction("printf");
    BJOU_DEBUG_ASSERT(pf && "Did not find printf()!");
    llvm::Value * val = llbe->builder.CreateCall(pf, vals);

    return val;
}

void * If::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llbe->pushFrame();

    bool hasElse = (bool)getElse();

    llvm::Value * cond = (llvm::Value *)llbe->getOrGenNode(getConditional());
    // Convert condition to a bool by comparing equal to 1.
    cond = llbe->builder.CreateICmpEQ(
        cond, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)),
        "ifcond");

    llvm::Function * func = llbe->builder.GetInsertBlock()->getParent();

    // Create blocks for the then and else cases.  Insert the 'then' block at
    // the end of the function.
    llvm::BasicBlock * then =
        llvm::BasicBlock::Create(llbe->llContext, "then", func);
    llvm::BasicBlock * _else = nullptr;
    if (hasElse)
        _else = llvm::BasicBlock::Create(llbe->llContext, "else");

    shouldEmitMerge = true;
    if (alwaysReturns())
        shouldEmitMerge = false;

    llvm::BasicBlock * merge = nullptr;
    if (shouldEmitMerge)
        merge = llvm::BasicBlock::Create(llbe->llContext, "merge");

    if (hasElse) {
        llbe->builder.CreateCondBr(cond, then, _else);
    } else if (shouldEmitMerge) {
        llbe->builder.CreateCondBr(cond, then, merge);
    }

    // Emit then value.
    llbe->builder.SetInsertPoint(then);

    for (ASTNode * statement : getStatements())
        llbe->getOrGenNode(statement);

    if (!getFlag(HAS_TOP_LEVEL_RETURN) && shouldEmitMerge) {
        llbe->builder.CreateBr(merge);
    }

    llbe->popFrame();

    if (hasElse) {
        // Emit else block.
        func->getBasicBlockList().push_back(_else);
        llbe->builder.SetInsertPoint(_else);

        llbe->pushFrame();

        Else * _Else = (Else *)getElse();
        for (ASTNode * statement : _Else->getStatements())
            llbe->getOrGenNode(statement);

        if (!_Else->getFlag(HAS_TOP_LEVEL_RETURN) && shouldEmitMerge) {
            llbe->builder.CreateBr(merge);
        }

        llbe->popFrame();

        _else = llbe->builder.GetInsertBlock();
    }

    // Codegen of 'Then' can change the current block, update ThenBB for the
    // PHI.
    then = llbe->builder.GetInsertBlock();

    if (shouldEmitMerge) {
        // Emit merge block.
        func->getBasicBlockList().push_back(merge);
        llbe->builder.SetInsertPoint(merge);
    }

    return nullptr; // what does this accomplish?
}

void * For::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Function * func = llbe->builder.GetInsertBlock()->getParent();

    llvm::BasicBlock * check =
        llvm::BasicBlock::Create(llbe->llContext, "forcheckcond", func);

    llvm::BasicBlock * then = llvm::BasicBlock::Create(llbe->llContext, "then");
    llvm::BasicBlock * after =
        llvm::BasicBlock::Create(llbe->llContext, "after");
    llvm::BasicBlock * merge =
        llvm::BasicBlock::Create(llbe->llContext, "merge");

    llbe->loop_continue_stack.push({llbe->curFrame(), after});
    llbe->loop_break_stack.push({llbe->curFrame(), merge});

    llbe->pushFrame();

    for (ASTNode * init : getInitializations())
        llbe->getOrGenNode(init);

    llbe->builder.CreateBr(check);

    llbe->builder.SetInsertPoint(check);

    llvm::Value * cond = (llvm::Value *)llbe->getOrGenNode(getConditional());
    // Convert condition to a bool by comparing equal to 1.
    cond = llbe->builder.CreateICmpEQ(
        cond, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)),
        "forcond");

    llbe->builder.CreateCondBr(cond, then, merge);

    // Emit then value.
    func->getBasicBlockList().push_back(then);
    llbe->builder.SetInsertPoint(then);

    for (ASTNode * statement : getStatements())
        llbe->getOrGenNode(statement);

    // afterthoughts
    if (!getFlag(HAS_TOP_LEVEL_RETURN)) {
        llbe->builder.CreateBr(after);
    }

    // afterthoughts and jump back to check
    func->getBasicBlockList().push_back(after);
    llbe->builder.SetInsertPoint(after);

    for (ASTNode * at : getAfterthoughts())
        llbe->getOrGenNode(at);

    /* if (!getFlag(HAS_TOP_LEVEL_RETURN)) { */
        llbe->builder.CreateBr(check);
    /* } */

    // Emit merge block.
    func->getBasicBlockList().push_back(merge);
    llbe->builder.SetInsertPoint(merge);

    llbe->loop_continue_stack.pop();
    llbe->loop_break_stack.pop();

    llbe->popFrame();

    return merge; // what does this accomplish?
}

void * While::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Function * func = llbe->builder.GetInsertBlock()->getParent();

    llvm::BasicBlock * check =
        llvm::BasicBlock::Create(llbe->llContext, "whilecheckcond", func);

    llbe->loop_continue_stack.push({llbe->curFrame(), check});

    llbe->builder.CreateBr(check);

    llbe->builder.SetInsertPoint(check);

    llvm::Value * cond = (llvm::Value *)llbe->getOrGenNode(getConditional());
    // Convert condition to a bool by comparing equal to 1.
    cond = llbe->builder.CreateICmpEQ(
        cond, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)),
        "whilecond");

    llbe->pushFrame();

    llvm::BasicBlock * then =
        llvm::BasicBlock::Create(llbe->llContext, "then", func);
    llvm::BasicBlock * merge =
        llvm::BasicBlock::Create(llbe->llContext, "merge");

    llbe->loop_break_stack.push({llbe->curFrame(), merge});

    llbe->builder.CreateCondBr(cond, then, merge);

    // Emit then value.
    llbe->builder.SetInsertPoint(then);

    for (ASTNode * statement : getStatements())
        llbe->getOrGenNode(statement);

    if (!getFlag(HAS_TOP_LEVEL_RETURN)) {
        llbe->builder.CreateBr(check);
    }

    // Codegen of 'Then' can change the current block, update ThenBB for the
    // PHI.
    then = llbe->builder.GetInsertBlock();

    // Emit merge block.
    func->getBasicBlockList().push_back(merge);
    llbe->builder.SetInsertPoint(merge);

    llbe->loop_continue_stack.pop();
    llbe->loop_break_stack.pop();

    llbe->popFrame();

    return merge; // what does this accomplish?
}

void * DoWhile::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Function * func = llbe->builder.GetInsertBlock()->getParent();

    llvm::BasicBlock * top =
        llvm::BasicBlock::Create(llbe->llContext, "dowhiletop", func);
    llvm::BasicBlock * check =
        llvm::BasicBlock::Create(llbe->llContext, "dowhilecheckcond", func);

    llbe->loop_continue_stack.push({llbe->curFrame(), check});

    llbe->builder.CreateBr(top);

    llbe->builder.SetInsertPoint(check);

    llvm::Value * cond = (llvm::Value *)llbe->getOrGenNode(getConditional());
    // Convert condition to a bool by comparing equal to 1.
    cond = llbe->builder.CreateICmpEQ(
        cond, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)),
        "dowhilecond");

    llbe->pushFrame();

    llvm::BasicBlock * merge =
        llvm::BasicBlock::Create(llbe->llContext, "merge");

    llbe->loop_break_stack.push({llbe->curFrame(), merge});

    llbe->builder.CreateCondBr(cond, top, merge);

    // Emit top block.
    llbe->builder.SetInsertPoint(top);

    for (ASTNode * statement : getStatements())
        llbe->getOrGenNode(statement);

    if (!getFlag(HAS_TOP_LEVEL_RETURN)) {
        llbe->builder.CreateBr(check);
    }

    // Codegen of 'Then' can change the current block, update ThenBB for the
    // PHI.
    top = llbe->builder.GetInsertBlock();

    // Emit merge block.
    func->getBasicBlockList().push_back(merge);
    llbe->builder.SetInsertPoint(merge);

    llbe->loop_continue_stack.pop();
    llbe->loop_break_stack.pop();

    llbe->popFrame();

    return merge; // what does this accomplish?
}


void * Procedure::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    analyze();

    const ProcedureType * my_real_t = (const ProcedureType *)getType();

    llvm::Function * func = llbe->llModule->getFunction(getMangledName());

    if (!func) {
        // generate proto and queue up the proc to be defined later

        // quick swap of array args to equivalent ptr args
        auto copy_params = my_real_t->getParamTypes();
        for (const Type *& argt : copy_params) {
            if (argt->isArray()) {
                argt = argt->under()->getPointer();
            }
        }

        ProcedureType * my_t = (ProcedureType *)ProcedureType::get(
            copy_params, my_real_t->getRetType(), my_real_t->isVararg);

        llvm::FunctionType * func_type =
            (llvm::FunctionType *)llbe->getOrGenType(my_t)
                ->getPointerElementType();

        // @abi
        ABILowerProcedureTypeData * payload = new ABILowerProcedureTypeData;
        payload->t = my_t;
        llbe->abi_lowerer->ABILowerProcedureType(payload);
        BJOU_DEBUG_ASSERT(payload && "could not create proc abi lower procedure type data");
        llbe->proc_abi_info[this] = payload;
        func_type = (llvm::FunctionType *)payload->fn_t;
        //

        llvm::Function::LinkageTypes linkage = llvm::Function::ExternalLinkage;
            /* ((getFlag(Procedure::IS_EXTERN) || (compilation->args.c_arg && llbe->mode != LLVMBackEnd::GEN_MODE::CT))) */
            /*     ? llvm::Function::ExternalLinkage */
            /*     : llvm::Function::InternalLinkage; */

        func = llvm::Function::Create(func_type, linkage, getMangledName(),
                                      llbe->llModule);

        BJOU_DEBUG_ASSERT(func);

        // we go ahead and put func into the generated_nodes table so that
        // any recursive references do not cause problems
        llbe->generated_nodes[this] = func;

        if (!getFlag(Procedure::IS_EXTERN)) {
            llbe->procs_need_completion.insert(this);
        }
        /* if (linkage == llvm::Function::LinkageTypes::InternalLinkage) */
        /*     llbe->procs_need_completion.insert(this); */

        int i = 0;
        for (auto & arg : func->args()) {
            if (payload->byval & (1 << i))
                arg.addAttr(llvm::Attribute::ByVal);
            if (payload->ref & (1 << i)) {
                const Type * param_t = payload->t->getParamTypes()[i]->unRef();
                unsigned int size = simpleSizer(param_t);
                if (!size)
                    size = 1;

                llvm::Attribute attr =
                    llvm::Attribute::getWithDereferenceableBytes(
                        llbe->llContext, size);
#if LLVM_VERSION_MAJOR > 4
                arg.addAttr(attr);
#else
                llvm::AttrBuilder ab;
                ab.addAttribute(attr);
                llvm::AttributeSet as =
                    llvm::AttributeSet::get(llbe->llContext, i + 1, ab);
                arg.addAttr(as);
#endif
            } else if (arg.getType()->isPointerTy()) {
                // arg.addAttr(llvm::Attribute::getWithAlignment(llbe->llContext,
                //                                               sizeof(void
                //                                               *)));
            }
            i += 1;
        }

        if (!payload->sret) {
            if (payload->t->getRetType()->isRef()) {
                const Type * ret_t = payload->t->getRetType()->unRef();
                unsigned int size = simpleSizer(ret_t);
                if (!size)
                    size = 1;
                func->addAttribute(0,
                                   llvm::Attribute::getWithDereferenceableBytes(
                                       llbe->llContext, size));
            } else if (payload->t->getRetType()->isPointer() ||
                       payload->t->getRetType()->isArray()) {
                // func->addAttribute(0, llvm::Attribute::getWithAlignment(
                //                           llbe->llContext, sizeof(void *)));
            }
        }

        llbe->addNamedVal(getMangledName(), func, my_real_t);

        // get dependencies and generate them before we try to complete any
        // procedures
        genDeps(this, *llbe);
    } else {
        if (getFlag(Procedure::IS_EXTERN))
            return func;

        // generate the definition

        llbe->proc_stack.push(this);

        // keep around the frame pointers so that we can backtrace
        func->addFnAttr("no-frame-pointer-elim", "true");
        func->addFnAttr("no-frame-pointer-elim-non-leaf", "true");

        // @abi
        ABILowerProcedureTypeData * payload =
            (ABILowerProcedureTypeData *)llbe->proc_abi_info[this];
        BJOU_DEBUG_ASSERT(payload && "could not get proc abi lower procedure type data");
        //

        int i = 0;
        for (auto & arg : func->args()) {
            if (i == 0 && payload->sret) {
                arg.setName("__bjou_sret");
            } else {
                VariableDeclaration * param = (VariableDeclaration *)(getParamVarDeclarations()
                                                         [i - payload->sret]);
                arg.setName(param->getMangledName());
            }
            i += 1;
        }

        BJOU_DEBUG_ASSERT(func->getBasicBlockList().empty());

        llvm::BasicBlock * entry = llvm::BasicBlock::Create(
            llbe->llContext, getMangledName() + "_entry", func);

        llvm::BasicBlock * begin = llvm::BasicBlock::Create(
            llbe->llContext, getMangledName() + "_begin", func);

        llbe->builder.SetInsertPoint(begin);

        llbe->pushFrame();

        llbe->local_alloc_stack.push(entry);

        int j = 0;
        for (auto & Arg : func->args()) {
            const Type * t = nullptr;
            if (j - payload->sret >= 0) {
                t = getParamVarDeclarations()[j - payload->sret]->getType();
            }

            if (t && t->isRef()) {
                llbe->addNamedVal(Arg.getName(), &Arg, t);
            } else {
                // Create an alloca for this variable.
                llvm::Value * alloca = nullptr;
                if (j == 0 && payload->sret) {
                    // @abi
                    alloca = llbe->allocNamedVal(
                        Arg.getName(), my_real_t->retType->getPointer());
                } else {
                    VariableDeclaration * v = (VariableDeclaration *)
                        getParamVarDeclarations()[j - payload->sret];
                    alloca = llbe->allocNamedVal(Arg.getName(), v->getType());
                }

                llvm::Value * val = &Arg;

                // Store the initial value into the alloca.

                // If this is byval, we need to do memcpy because the value is
                // big.
                if (payload->byval & (1 << j)) {
                    auto size = llbe->layout->getTypeAllocSize(
                        alloca->getType()->getPointerElementType());
                    auto align = llbe->layout->getABITypeAlignment(
                        alloca->getType()->getPointerElementType());
#if LLVM_VERSION_MAJOR >= 7
                    llbe->builder.CreateMemCpy(alloca, align, val, align, size);
#else
                    llbe->builder.CreateMemCpy(alloca, val, size, align);
#endif
                } else {
                    llbe->builder.CreateStore(val, alloca);
                }
            }
            j += 1;
        }

        for (ASTNode * statement : getStatements()) {
            llbe->getOrGenNode(statement);
        }

        if (!getFlag(HAS_TOP_LEVEL_RETURN)) {
            // dangerous to assume void ret type..  @bad
            // but we hope that semantic analysis will have caught
            // a missing explicitly typed return

            llbe->builder.CreateRetVoid();
        }

        llbe->local_alloc_stack.pop();

        llbe->popFrame();

        auto save = llbe->builder.saveIP();
        llbe->builder.SetInsertPoint(entry);
        llbe->builder.CreateBr(begin);
        llbe->builder.restoreIP(save);

        std::string errstr;
        llvm::raw_string_ostream errstream(errstr);
        if (llvm::verifyFunction(*func, &errstream)) {
            func->print(llvm::errs(), nullptr);
            error(COMPILER_SRC_CONTEXT(), "LLVM:", false, errstream.str());
            internalError("There was an llvm error.");
        }

        // run passes
        auto FPM = llbe->proc_passes.find(this);
        if (FPM != llbe->proc_passes.end()) {
            FPM->second->doInitialization();
            FPM->second->run(*func);
            FPM->second->doFinalization();
        }
        //

        llbe->proc_stack.pop();
    }

    return func;
}

void * Return::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    Procedure * proc = llbe->proc_stack.top();
    ABILowerProcedureTypeData * payload =
        (ABILowerProcedureTypeData *)llbe->proc_abi_info[proc];

    ProcedureType * pt = (ProcedureType *)proc->getType();

    if (payload->sret) {
        // @abi
        BJOU_DEBUG_ASSERT(getExpression());
        llvm::Value * _sret = llbe->getNamedVal("__bjou_sret");
        BJOU_DEBUG_ASSERT(_sret);
        llvm::Value * sret = llbe->builder.CreateLoad(_sret, "sret");
        llvm::Value * val =
            (llvm::Value *)llbe->getOrGenNode(getExpression(), true);

        const Type * t = getExpression()->getType();
        llvm::Type * ll_t = llbe->getOrGenType(t);

        llvm::PointerType * ll_byte_ptr_t =
            llvm::Type::getInt8Ty(llbe->llContext)->getPointerTo();

        sret = llbe->builder.CreateBitCast(sret, ll_byte_ptr_t);
        val = llbe->builder.CreateBitCast(val, ll_byte_ptr_t);

        llvm::Value * size =
            llvm::ConstantInt::get(llvm::Type::getInt64Ty(llbe->llContext),
                                   llbe->layout->getTypeAllocSize(ll_t));

#if LLVM_VERSION_MAJOR >= 7
        llbe->builder.CreateMemCpy(sret, llbe->layout->getABITypeAlignment(ll_t),
                                   val, llbe->layout->getABITypeAlignment(ll_t),
                                   size);
#else
        llbe->builder.CreateMemCpy(sret, val, size,
                                   llbe->layout->getABITypeAlignment(ll_t));
#endif

        return llbe->builder.CreateRetVoid();
    }
    if (getExpression()) {
        llvm::Value * v    = nullptr;
        const Type * rt    = pt->getRetType();
        llvm::Type * ll_rt = llbe->getOrGenType(rt);
        if (rt->isRef()) {
            v = (llvm::Value *)llbe->getOrGenNode(getExpression(), true);
            if (v->getType() != ll_rt)
                v = llbe->builder.CreateBitCast(v, ll_rt);
        } else {
            v = (llvm::Value *)llbe->getOrGenNode(getExpression());
        }

        BJOU_DEBUG_ASSERT(v);

        return llbe->builder.CreateRet(v);
    }
    return llbe->builder.CreateRetVoid();
}

void * Struct::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    std::string & mname = getMangledName();
    llvm::StructType * ll_s_ty = nullptr;
    StructType * bjou_s_ty = nullptr;

    if (llbe->generated_nodes.find(this) == llbe->generated_nodes.end()) {
        genDeps(this, *llbe);
        llbe->types_need_completion.insert(this);
    } else {
        std::vector<llvm::Type *> member_types;

        ll_s_ty = (llvm::StructType *)llbe->getOrGenType(getType());

        bjou_s_ty = (StructType *)getType();

        for (const Type * bjou_mem_t : bjou_s_ty->memberTypes) {
            llvm::Type * ll_mem_t = llbe->getOrGenType(bjou_mem_t);
            BJOU_DEBUG_ASSERT(ll_mem_t);
            member_types.push_back(ll_mem_t);
        }
        ll_s_ty->setBody(member_types);
    }

    return ll_s_ty;
}

void * Break::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    auto & lfi = llbe->loop_break_stack.top();

    return llbe->builder.CreateBr(lfi.bb);
}

void * Continue::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    auto & lfi = llbe->loop_continue_stack.top();

    return llbe->builder.CreateBr(lfi.bb);
}

void * ExprBlock::generate(BackEnd & backEnd, bool getAddr) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * t = getType();

    llvm::Value * alloca = llbe->allocUnnamedVal(t);

    llbe->expr_block_yield_stack.push(alloca);

    for (ASTNode * statement : getStatements())
        llbe->getOrGenNode(statement);

    llbe->expr_block_yield_stack.pop();

    if (getAddr && !t->isRef())
        return alloca;

    return llbe->builder.CreateLoad(alloca);
}

void * ExprBlockYield::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    ASTNode * p = getParent();
    while (p) {
        if (p->nodeKind == EXPR_BLOCK)
            break;
        p = p->getParent();
    }

    ExprBlock * block = (ExprBlock *)p;

    const Type * block_t = block->getType();

    llvm::Value * val =
        (llvm::Value *)llbe->getOrGenNode(getExpression(), block_t->isRef());

    BJOU_DEBUG_ASSERT(val);

    llvm::Value * dest = llbe->expr_block_yield_stack.top();

    llbe->builder.CreateStore(val, dest);

    return val;
}

} // namespace bjou
