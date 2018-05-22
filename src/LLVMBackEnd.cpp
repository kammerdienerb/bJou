//
//  LLVMBackEnd.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "LLVMBackEnd.hpp"

#include <algorithm>
#include <unordered_map>

// #include <llvm/CodeGen/CommandFlags.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/Interpreter.h>
#include <llvm/ExecutionEngine/MCJIT.h>

#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Transforms/Scalar.h>

#include "ASTNode.hpp"
#include "CLI.hpp"
#include "Compile.hpp"
#include "FrontEnd.hpp"
#include "Global.hpp"
#include "LLVMABI.hpp"
#include "LLVMGenerator.hpp"
#include "Misc.hpp"
#include "Type.hpp"

#ifdef BJOU_DEBUG_BUILD
#define SAVE_BJOU_DEBUG_BUILD
#endif
#undef BJOU_DEBUG_BUILD
#include "tclap/CmdLine.h"
#ifdef SAVE_BJOU_DEBUG_BUILD
#define BJOU_DEBUG_BUILD
#endif

namespace bjou {

StackFrame::StackFrame() : vals({}), namedVals({}) {}

LoopFrameInfo::LoopFrameInfo(StackFrame _frame, llvm::BasicBlock * _bb)
    : frame(_frame), bb(_bb) {}

LLVMBackEnd::LLVMBackEnd(FrontEnd & _frontEnd)
    : BackEnd(_frontEnd), generator(*this),
      abi_lowerer(ABILowerer<LLVMBackEnd>::get(*this)), builder(llContext) {
    defaultTarget = nullptr;
    defaultTargetMachine = nullptr;
    layout = nullptr;
    llModule = nullptr;
    outModule = nullptr;
    ee = nullptr;
}

void LLVMBackEnd::init() {
    mode = GEN_MODE::CT;

    // llvm initialization for object code gen
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargetMCs();

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

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
    // For codegen passes, only passes that do IR to IR transformation are
    // supported.
    llvm::initializeCodeGenPreparePass(Registry);
    llvm::initializeAtomicExpandPass(Registry);
    llvm::initializeRewriteSymbolsLegacyPassPass(Registry);
    llvm::initializeWinEHPreparePass(Registry);
    llvm::initializeDwarfEHPreparePass(Registry);
    llvm::initializeSafeStackLegacyPassPass(Registry);
    llvm::initializeSjLjEHPreparePass(Registry);
    llvm::initializePreISelIntrinsicLoweringLegacyPassPass(Registry);
    llvm::initializeGlobalMergePass(Registry);
    llvm::initializeInterleavedAccessPass(Registry);
    // llvm::initializeCountingFunctionInserterPass(Registry);
    llvm::initializeUnreachableBlockElimLegacyPassPass(Registry);

    // set up target and triple
    defaultTriple = llvm::sys::getDefaultTargetTriple();
    std::string targetErr;
    defaultTarget =
        llvm::TargetRegistry::lookupTarget(defaultTriple, targetErr);
    if (!defaultTarget) {
        error(Context(), "Could not create llvm default Target.", false,
              targetErr);
        internalError("There was an llvm error.");
    }

    llvm::TargetOptions Options;
    llvm::CodeGenOpt::Level OLvl =
        (compilation->args.opt_arg.getValue() ? llvm::CodeGenOpt::Aggressive
                                              : llvm::CodeGenOpt::None);

    defaultTargetMachine = defaultTarget->createTargetMachine(
        defaultTriple, "generic", "", Options, llvm::None,
        llvm::CodeModel::Small, OLvl);
    layout = new llvm::DataLayout(defaultTargetMachine->createDataLayout());

    // set up default target machine
    std::string CPU = "generic";
    std::string Features = "";
    llvm::TargetOptions opt;
    auto RM = llvm::Optional<llvm::Reloc::Model>();
    defaultTargetMachine = defaultTarget->createTargetMachine(
        defaultTriple, CPU, Features, opt, RM);

    outModule = new llvm::Module(compilation->outputbasefilename, llContext);
    outModule->setDataLayout(defaultTargetMachine->createDataLayout());
    outModule->setTargetTriple(defaultTriple);

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
        return llvm::createLoopSimplifyPass();
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
    jitModule->setDataLayout(defaultTargetMachine->createDataLayout());
    jitModule->setTargetTriple(defaultTriple);

    if (ee)
        delete ee;

    /* Create execution engine */
    // No optimization seems to be the best route here as far as 
    // compile times go..
    std::string err_str;
    llvm::EngineBuilder engineBuilder(std::move(jitModule));
    engineBuilder
        .setErrorStr(&err_str)
        .setOptLevel(llvm::CodeGenOpt::None);

    ee = engineBuilder.create();

    if (!ee) {
        error(COMPILER_SRC_CONTEXT(), "LLVM:", true, err_str);
        internalError("There was an llvm error.");
    }
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
    if (compilation->args.time_arg.getValue())
        prettyPrintTimeMin(cg_time, std::string("Code Generation") +
                                        (compilation->args.opt_arg.getValue()
                                             ? " and Optimization"
                                             : ""));

    if (!compilation->args.c_arg.getValue()) {
        auto l_time = LinkingStage();
        if (compilation->args.time_arg.getValue())
            prettyPrintTimeMin(l_time, "Linking");
    }

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

void * LLVMBackEnd::run(Procedure * proc, void * _val_args) {
    std::vector<Val>& val_args = *(std::vector<Val>*)_val_args;

    mode = GEN_MODE::CT;

    if (!ee)
        init_jit();

    jit_reset();

    // just in case
    getOrGenNode(compilation->frontEnd.printf_decl);
    getOrGenNode(compilation->frontEnd.malloc_decl);
    getOrGenNode(compilation->frontEnd.free_decl);
    getOrGenNode(compilation->frontEnd.memset_decl);
    getOrGenNode(compilation->frontEnd.memcpy_decl);

    llvm::Function * fn = (llvm::Function *)getOrGenNode(proc);
    std::string name = fn->getName().str();

    // @bad
    // have to make a quick fix since, in normal compilation,
    // this happens after analysis rather than in the middle of it
    if (!compilation->args.nopreload_arg.isSet())
        compilation->frontEnd.fix_typeinfo_v_table_size();

    completeTypes();
    completeGlobs();
    completeProcs();

    // verify module
    std::string errstr;
    llvm::raw_string_ostream errstream(errstr);
    if (llvm::verifyModule(*llModule, &errstream)) {
        llModule->dump();
        error(COMPILER_SRC_CONTEXT(), "LLVM:", true, errstream.str());
        internalError("There was an llvm error.");
    }

    uint64_t ptr = ee->getFunctionAddress(name);

    const Type * _t = proc->getType();
    ProcedureType * t = (ProcedureType*)_t;
    const std::vector<const Type*>& param_types = t->getParamTypes();
    const Type * ret_t = t->getRetType();

    if (param_types.size() == 0) {
        if (ret_t->isVoid()) {
            void (*fn_ptr)() = (void (*)())ptr;
            fn_ptr();
            return nullptr;
        } else if (ret_t->isInt()) {
            if (((IntType*)ret_t)->width == 32) {
                int (*fn_ptr)() = (int(*)())ptr;
                return (void*)fn_ptr(); 
            } else if (((IntType*)ret_t)->width == 64) {
                long long(*fn_ptr)() = (long long(*)())ptr;
                return (void*)fn_ptr(); 
            }
        } else if (ret_t->isChar()) {
            char (*fn_ptr)() = (char(*)())ptr;
            return (void*)fn_ptr(); 
        } else if (ret_t->isFloat()) {
            if (((FloatType*)ret_t)->width == 32) {
                float (*fn_ptr)() = (float(*)())ptr;
                float r = fn_ptr();
                return (void*)*((uint64_t*)&r);
            } else if (((FloatType*)ret_t)->width == 64) {
                double (*fn_ptr)() = (double(*)())ptr;
                double r = fn_ptr();
                return (void*)*((uint64_t*)&r);
            }
        } else if (ret_t->isPointer() && ret_t->under()->isChar()) {
            char * (*fn_ptr)() = (char*(*)())ptr;
            return (void*)fn_ptr();
        }
    } else if (param_types.size() == 1) {
        const Type * param_t = param_types[0];
        Val& arg = val_args[0];
        if (ret_t->isVoid()) {
            if (param_t->isInt()) {
                if (((IntType*)param_t)->width == 32) {
                    void (*fn_ptr)(int) = (void(*)(int))ptr;
                    fn_ptr(arg.as_i64);
                    return nullptr;
                } else if (((IntType*)param_t)->width == 64) {
                    void (*fn_ptr)(long long) = (void(*)(long long))ptr;
                    fn_ptr(arg.as_i64); 
                    return nullptr;
                }
            } else if (param_t->isChar()) {
                void (*fn_ptr)(char) = (void(*)(char))ptr;
                fn_ptr(arg.as_i64); 
                return nullptr;
            } else if (param_t->isFloat()) {
                if (((FloatType*)param_t)->width == 32) {
                    void (*fn_ptr)(float) = (void(*)(float))ptr;
                    fn_ptr(arg.as_f64); 
                    return nullptr;
                } else if (((FloatType*)param_t)->width == 64) {
                    void (*fn_ptr)(double) = (void(*)(double))ptr;
                    fn_ptr(arg.as_f64); 
                    return nullptr;
                }
           } else if (param_t->isPointer() && param_t->under()->isChar()) {
                void (*fn_ptr)(char*) = (void(*)(char*))ptr;
                fn_ptr((char*)arg.as_string.c_str()); 
                return nullptr;
           }
        } else if (ret_t->isInt()) {
            if (((IntType*)ret_t)->width == 32) {
                if (param_t->isInt()) {
                    if (((IntType*)param_t)->width == 32) {
                        int (*fn_ptr)(int) = (int(*)(int))ptr;
                        return (void*)fn_ptr(arg.as_i64); 
                    } else if (((IntType*)param_t)->width == 64) {
                        int (*fn_ptr)(long long) = (int(*)(long long))ptr;
                        return (void*)fn_ptr(arg.as_i64); 
                    }
                } else if (param_t->isChar()) {
                    int (*fn_ptr)(char) = (int(*)(char))ptr;
                    return (void*)fn_ptr(arg.as_i64); 
                } else if (param_t->isFloat()) {
                    if (((FloatType*)param_t)->width == 32) {
                        int (*fn_ptr)(float) = (int(*)(float))ptr;
                        return (void*)fn_ptr(arg.as_f64); 
                    } else if (((FloatType*)param_t)->width == 64) {
                        int (*fn_ptr)(double) = (int(*)(double))ptr;
                        return (void*)fn_ptr(arg.as_f64); 
                    }
               } else if (param_t->isPointer() && param_t->under()->isChar()) {
                    int (*fn_ptr)(char*) = (int(*)(char*))ptr;
                    return (void*)fn_ptr((char*)arg.as_string.c_str()); 
               }
            } else if (((IntType*)ret_t)->width == 64) {
                if (param_t->isInt()) {
                    if (((IntType*)param_t)->width == 32) {
                        long long (*fn_ptr)(int) = (long long(*)(int))ptr;
                        return (void*)fn_ptr(arg.as_i64); 
                    } else if (((IntType*)param_t)->width == 64) {
                        long long (*fn_ptr)(long long) = (long long(*)(long long))ptr;
                        return (void*)fn_ptr(arg.as_i64); 
                    }
                } else if (param_t->isChar()) {
                    long long (*fn_ptr)(char) = (long long(*)(char))ptr;
                    return (void*)fn_ptr(arg.as_i64); 
                } else if (param_t->isFloat()) {
                    if (((FloatType*)param_t)->width == 32) {
                        long long (*fn_ptr)(float) = (long long(*)(float))ptr;
                        return (void*)fn_ptr(arg.as_f64); 
                    } else if (((FloatType*)param_t)->width == 64) {
                        long long (*fn_ptr)(double) = (long long(*)(double))ptr;
                        return (void*)fn_ptr(arg.as_f64); 
                    }
                } else if (param_t->isPointer() && param_t->under()->isChar()) {
                    long long (*fn_ptr)(char*) = (long long(*)(char*))ptr;
                    return (void*)fn_ptr((char*)arg.as_string.c_str()); 
                }
            }
        } else if (ret_t->isChar()) {
            if (param_t->isInt()) {
                if (((IntType*)param_t)->width == 32) {
                    char (*fn_ptr)(int) = (char(*)(int))ptr;
                    return (void*)fn_ptr(arg.as_i64); 
                } else if (((IntType*)param_t)->width == 64) {
                    char (*fn_ptr)(long long) = (char(*)(long long))ptr;
                    return (void*)fn_ptr(arg.as_i64); 
                }
            } else if (param_t->isChar()) {
                char (*fn_ptr)(char) = (char(*)(char))ptr;
                return (void*)fn_ptr(arg.as_i64); 
            } else if (param_t->isFloat()) {
                if (((FloatType*)param_t)->width == 32) {
                    char (*fn_ptr)(float) = (char(*)(float))ptr;
                    return (void*)fn_ptr(arg.as_f64); 
                } else if (((FloatType*)param_t)->width == 64) {
                    char (*fn_ptr)(double) = (char(*)(double))ptr;
                    return (void*)fn_ptr(arg.as_f64); 
                }
            } else if (param_t->isPointer() && param_t->under()->isChar()) {
                char (*fn_ptr)(char*) = (char(*)(char*))ptr;
                return (void*)fn_ptr((char*)arg.as_string.c_str()); 
            }
        } else if (ret_t->isFloat()) {
            if (((FloatType*)ret_t)->width == 32) {
                if (param_t->isInt()) {
                    if (((IntType*)param_t)->width == 32) {
                        float (*fn_ptr)(int) = (float(*)(int))ptr;
                        float r = fn_ptr(arg.as_i64);
                        return (void*)*((uint64_t*)&r);
                    } else if (((IntType*)param_t)->width == 64) {
                        float (*fn_ptr)(long long) = (float(*)(long long))ptr;
                        float r = fn_ptr(arg.as_i64);
                        return (void*)*((uint64_t*)&r);
                    }
                } else if (param_t->isChar()) {
                    float (*fn_ptr)(char) = (float(*)(char))ptr;
                    float r = fn_ptr(arg.as_i64);
                    return (void*)*((uint64_t*)&r);
                } else if (param_t->isFloat()) {
                    if (((FloatType*)param_t)->width == 32) {
                        float (*fn_ptr)(float) = (float(*)(float))ptr;
                        float r = fn_ptr(arg.as_f64);
                        return (void*)*((uint64_t*)&r);
                    } else if (((FloatType*)param_t)->width == 64) {
                        float (*fn_ptr)(double) = (float(*)(double))ptr;
                        float r = fn_ptr(arg.as_f64);
                        return (void*)*((uint64_t*)&r);
                    }
                } else if (param_t->isPointer() && param_t->under()->isChar()) {
                    float (*fn_ptr)(char*) = (float(*)(char*))ptr;
                    float r = fn_ptr((char*)arg.as_string.c_str());
                    return (void*)*((uint64_t*)&r);
                }
            } else if (((FloatType*)ret_t)->width == 64) {
                if (param_t->isInt()) {
                    if (((IntType*)param_t)->width == 32) {
                        double (*fn_ptr)(int) = (double(*)(int))ptr;
                        double r = fn_ptr(arg.as_i64);
                        return (void*)*((uint64_t*)&r);
                    } else if (((IntType*)param_t)->width == 64) {
                        double (*fn_ptr)(long long) = (double(*)(long long))ptr;
                        double r = fn_ptr(arg.as_i64);
                        return (void*)*((uint64_t*)&r);
                    }
                } else if (param_t->isChar()) {
                    double (*fn_ptr)(char) = (double(*)(char))ptr;
                    double r = fn_ptr(arg.as_i64);
                    return (void*)*((uint64_t*)&r);
                } else if (param_t->isFloat()) {
                    if (((FloatType*)param_t)->width == 32) {
                        double (*fn_ptr)(float) = (double(*)(float))ptr;
                        double r = fn_ptr(arg.as_f64);
                        return (void*)*((uint64_t*)&r);
                    } else if (((FloatType*)param_t)->width == 64) {
                        double (*fn_ptr)(double) = (double(*)(double))ptr;
                        double r = fn_ptr(arg.as_f64);
                        return (void*)*((uint64_t*)&r);
                    }
                } else if (param_t->isPointer() && param_t->under()->isChar()) {
                    double (*fn_ptr)(char*) = (double(*)(char*))ptr;
                    double r = fn_ptr((char*)arg.as_string.c_str());
                    return (void*)*((uint64_t*)&r);
                }
            }
        } else if (ret_t->isPointer() && ret_t->under()->isChar()) {
            if (param_t->isInt()) {
                if (((IntType*)param_t)->width == 32) {
                    char * (*fn_ptr)(int) = (char*(*)(int))ptr;
                    return (void*)fn_ptr(arg.as_i64); 
                } else if (((IntType*)param_t)->width == 64) {
                    char * (*fn_ptr)(long long) = (char*(*)(long long))ptr;
                    return (void*)fn_ptr(arg.as_i64); 
                }
            } else if (param_t->isChar()) {
                char * (*fn_ptr)(char) = (char*(*)(char))ptr;
                return (void*)fn_ptr(arg.as_i64); 
            } else if (param_t->isFloat()) {
                if (((FloatType*)param_t)->width == 32) {
                    char * (*fn_ptr)(float) = (char*(*)(float))ptr;
                    return (void*)fn_ptr(arg.as_f64); 
                } else if (((FloatType*)param_t)->width == 64) {
                    char * (*fn_ptr)(double) = (char*(*)(double))ptr;
                    return (void*)fn_ptr(arg.as_f64); 
                }
            } else if (param_t->isPointer() && param_t->under()->isChar()) {
                char * (*fn_ptr)(char*) = (char*(*)(char*))ptr;
                return (void*)fn_ptr((char*)arg.as_string.c_str()); 
            }
        }
    }
    internalError("LLVMBackEnd::run(): For now, there is a limit to the type signatures of procedures that we can call.");
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
            defaultTargetMachine->getTargetIRAnalysis()));

        defaultTargetMachine->adjustPassManager(Builder);
        Builder.populateFunctionPassManager(*fpass);

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

            if (ident->unqualified == "_nullptr")
                BJOU_DEBUG_ASSERT(true);

            if (ident->resolved) {
                ASTNode * r = ident->resolved;

                llbe.getOrGenType(r->getType());

                if (r->nodeKind == ASTNode::PROCEDURE ||
                    r->nodeKind == ASTNode::CONSTANT ||
                    (r->nodeKind == ASTNode::VARIABLE_DECLARATION &&
                     (!r->getScope()->parent ||
                      r->getScope()->parent->nspace))) {

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
            if (!_proc.second->isTemplateProc() &&
                proc->getParentStruct() == s &&
                !proc->getFlag(Procedure::IS_INTERFACE_DECL)) {

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

        for (ASTNode * node : copy)
            node->generate(*this);
    }
}

static void * GenerateGlobalVariable(VariableDeclaration * var, llvm::GlobalVariable * gvar,
                                     BackEnd & backEnd, bool flag = false) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * t = var->getType();
    llvm::Type * ll_t = llbe->getOrGenType(t);

    if (!gvar) {
        if (t->isArray()) {
            PointerType * ptr_t = (PointerType *)t->under()->getPointer();
            llvm::Type * ll_ptr_t = llbe->getOrGenType(ptr_t);
            gvar = new llvm::GlobalVariable(*llbe->llModule, ll_ptr_t, false,
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

        return gvar;
    } else {
        size_t align = llbe->layout->getTypeAllocSize(ll_t);
        // nearest power of 2
        align = pow(2, ceil(log(align) / log(2)));

        if (t->isArray()) {
            PointerType * ptr_t = (PointerType *)t->under()->getPointer();
            llvm::Type * ll_ptr_t = llbe->getOrGenType(ptr_t);

            gvar->setAlignment(
                (unsigned int)llbe->layout->getTypeAllocSize(ll_ptr_t));

            llvm::GlobalVariable * under = new llvm::GlobalVariable(
                *llbe->llModule, ll_t, false, llvm::GlobalVariable::ExternalLinkage,
                0, "__bjou_array_under_" + var->getMangledName());
            under->setAlignment(llbe->layout->getPreferredAlignment(under));

            if (var->getInitialization()) {
                if (var->getInitialization()->nodeKind == ASTNode::INITIALZER_LIST)
                    under->setInitializer(llbe->createConstantInitializer(
                        (InitializerList *)var->getInitialization()));
            } else {
                under->setInitializer(llvm::Constant::getNullValue(ll_t));
            }

            gvar->setInitializer((llvm::Constant *)llbe->builder.CreateInBoundsGEP(
                under, {llvm::Constant::getNullValue(
                            llvm::IntegerType::getInt32Ty(llbe->llContext)),
                        llvm::Constant::getNullValue(
                            llvm::IntegerType::getInt32Ty(llbe->llContext))}));
        } else {
            gvar->setAlignment((unsigned int)align);
            if (var->getInitialization())
                gvar->setInitializer(
                    (llvm::Constant *)llbe->getOrGenNode(var->getInitialization()));
            else
                gvar->setInitializer(llvm::Constant::getNullValue(ll_t));
        }
    }

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
    globaltypeinfos.clear();
    generatedTypeMemberConstants.clear();

    frames.clear(); // clear jit frames if any

    pushFrame();
    
    // just in case
    getOrGenNode(compilation->frontEnd.printf_decl);
    getOrGenNode(compilation->frontEnd.malloc_decl);
    getOrGenNode(compilation->frontEnd.free_decl);
    getOrGenNode(compilation->frontEnd.memset_decl);
    getOrGenNode(compilation->frontEnd.memcpy_decl);

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

    llvm::Function * main = createMainEntryPoint();

    completeTypes();
    completeGlobs();
    completeProcs();

    completeMainEntryPoint(main);

    popFrame();

    std::string errstr;
    llvm::raw_string_ostream errstream(errstr);
    if (llvm::verifyModule(*llModule, &errstream)) {
        llModule->dump();
        error(COMPILER_SRC_CONTEXT(), "LLVM:", true, errstream.str());
        internalError("There was an llvm error.");
    }

    generator.generate();

    if (compilation->args.verbose_arg.getValue())
        llModule->dump();

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

milliseconds LLVMBackEnd::LinkingStage() {
    auto start = Clock::now();

    std::string dest =
        compilation->outputpath + compilation->outputbasefilename;
    std::string dest_o = dest + ".o";

    std::vector<const char *> link_args = {dest_o.c_str()};

    for (auto & f : compilation->obj_files)
        link_args.push_back(f.c_str());

    link_args.push_back("-o");
    link_args.push_back(dest.c_str());

    for (auto & l : compilation->args.link_arg.getValue()) {
        link_args.push_back("-l");
        link_args.push_back(l.c_str());
    }

    bool use_system_linker = true;

    if (compilation->args.lld_arg.getValue()) {
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

    if (use_system_linker) {
        const char * cc = getenv("CC");
        if (!cc)
            cc = "cc";

        link_args.insert(link_args.begin(), cc);
        link_args.push_back(NULL);

        int fds[2];
        int status;
        if (pipe(fds) == -1)
            internalError("Error creating pipe to system linker.");

        pid_t childpid = fork();
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

    auto end = Clock::now();
    return duration_cast<milliseconds>(end - start);
}

void LLVMBackEnd::pushFrame() { frames.emplace_back(); }

static llvm::Value * createidestroyCall(LLVMBackEnd * llbe, llvm::Value * val,
                                        StructType * s_t) {
    BJOU_DEBUG_ASSERT(s_t->implementsInterfaces());

    llvm::PointerType * ll_byte_ptr_t =
        llvm::Type::getInt8Ty(llbe->llContext)->getPointerTo();

    Procedure * proc = s_t->idestroy_link;
    BJOU_DEBUG_ASSERT(proc);

    BJOU_DEBUG_ASSERT(s_t->interfaceIndexMap.count(proc));
    unsigned int idx = s_t->interfaceIndexMap[proc];
    llvm::Constant * idx_val =
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), idx);

    llvm::Value * typeinfo =
        (llvm::GlobalVariable *)llbe->getGlobaltypeinfo(s_t->_struct);
    BJOU_DEBUG_ASSERT(typeinfo);
    StructType * typeinfo_t =
        (StructType *)compilation->frontEnd.typeTable["typeinfo"];
    llvm::StructType * ll_typeinfo_t =
        (llvm::StructType *)llbe->getOrGenType(typeinfo_t);

    llvm::Value * typeinfo_cast = llbe->builder.CreateBitCast(
        val, ll_typeinfo_t->getPointerTo()->getPointerTo());
    llvm::Value * typeinfo_addr =
        llbe->builder.CreateLoad(typeinfo_cast, "typeinfo_load");
    llvm::Value * v_table_cast = llbe->builder.CreateBitCast(
        typeinfo_addr, ll_byte_ptr_t->getPointerTo());

    std::vector<llvm::Value *> gep_indices{idx_val};
    llvm::Value * gep =
        llbe->builder.CreateInBoundsGEP(v_table_cast, gep_indices);

    llvm::Value * callee = llbe->builder.CreateLoad(gep, "v_table_load");

    BJOU_DEBUG_ASSERT(callee);

    llvm::Value * fn = llbe->builder.CreateBitCast(
        callee, llbe->getOrGenType(proc->getType()));

    return llbe->builder.CreateCall(fn, {val});
}

static void generateFramePreExit(LLVMBackEnd * llbe, StackFrame f) {
    for (auto it = f.vals.rbegin(); it != f.vals.rend(); it++) {
        if (it->type->isStruct()) {
            StructType * s_t = (StructType *)it->type;
            if (s_t->interfaces.find("idestroy") != s_t->interfaces.end()) {
                createidestroyCall(llbe, it->val, s_t);
            }
        }
    }
}

static void generateFramePreExit(LLVMBackEnd * llbe) {
    generateFramePreExit(llbe, llbe->curFrame());
}

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

llvm::Value * LLVMBackEnd::allocUnnamedVal(const Type * type, bool array2pointer) {
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
    case Type::FLOAT: {
        return llvm::Type::getDoubleTy(llContext);
        /*if (t->size == 32)
            return llvm::Type::getFloatTy(llContext);
        else if (t->size == 64)
            return llvm::Type::getDoubleTy(llContext);*/ }
        case Type::CHAR:
            return llvm::IntegerType::get(llContext, 8);
        case Type::STRUCT: {
            // have to be really sneaky here
            // load the type into the map before we run getOrGenNode()
            // because Struct::generate() will call createGlobaltypeinfo()
            // which will call getOrGenType() which will infinitely recurse
            // unless we load the table preemptively
            llvm::Type * ll_t = llvm::StructType::create(llContext, t->key);
            generated_types[t] = ll_t;
            Struct * s = ((StructType *)t)->_struct;
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
                array_t = (ArrayType*)array_t->under();
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
        case Type::POINTER: {
        case Type::REF:
            if (t->under() == VoidType::get())
                return getOrGenType(
                    IntType::get(Type::UNSIGNED, 8)->getPointer());
            return getOrGenType(t->under())->getPointerTo();
        }
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

    for (ASTNode * node : compilation->frontEnd.AST)
        if (node->isStatement() || node->nodeKind == ASTNode::MULTINODE)
            genDeps(node, *this);

    return func;
}

void LLVMBackEnd::completeMainEntryPoint(llvm::Function * func) {
    BJOU_DEBUG_ASSERT(func);

    builder.SetInsertPoint(&func->back());

    for (ASTNode * node : compilation->frontEnd.AST)
        if (node->isStatement() || node->nodeKind == ASTNode::MULTINODE)
            getOrGenNode(node);

    builder.CreateRet(
        llvm::ConstantInt::get(llContext, llvm::APInt(32, 0, true)));

    std::string errstr;
    llvm::raw_string_ostream errstream(errstr);
    if (llvm::verifyFunction(*func, &errstream)) {
        func->dump();
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
        func->dump();
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

llvm::ConstantArray * LLVMBackEnd::create_v_table_constant_array(Struct * s) {
    StructType * s_t = (StructType *)s->getType();

    BJOU_DEBUG_ASSERT(s_t->implementsInterfaces());

    llvm::PointerType * ll_byte_ptr_t =
        llvm::Type::getInt8Ty(llContext)->getPointerTo();
    std::vector<llvm::Constant *> vals(
        compilation->max_interface_procs,
        llvm::ConstantPointerNull::get(ll_byte_ptr_t));

    int i = 0;

    for (ASTNode * _impl : s->getAllInterfaceImplsSorted()) {
        InterfaceImplementation * impl = (InterfaceImplementation *)_impl;

        for (auto & procs : impl->getProcs()) {
            for (auto & _proc : procs.second) {
                if (!impl->getFlag(
                        InterfaceImplementation::PUNT_TO_EXTENSION)) {
                    Procedure * proc = (Procedure *)_proc;
                    llvm::Constant * fn_val =
                        (llvm::Constant *)getNamedVal(proc->getMangledName());
                    BJOU_DEBUG_ASSERT(fn_val);
                    llvm::Constant * cast =
                        (llvm::Constant *)builder.CreateBitCast(fn_val,
                                                                ll_byte_ptr_t);
                    vals[i] = cast;
                }
                i += 1;
            }
        }
    }

    StructType * typeinfo_t =
        (StructType *)compilation->frontEnd.typeTable["typeinfo"];

    llvm::ArrayType * _v_table_t = (llvm::ArrayType *)getOrGenType(
        typeinfo_t->memberTypes[typeinfo_t->memberIndices["_v_table"]]);

    llvm::ConstantArray * c =
        (llvm::ConstantArray *)llvm::ConstantArray::get(_v_table_t, vals);

    return c;
}

llvm::Value * LLVMBackEnd::createGlobaltypeinfo(Struct * s) {
    if (!compilation->frontEnd.typeinfo_struct)
        internalError("typeinfo_struct unavailable");

    BJOU_DEBUG_ASSERT(((StructType *)s->getType())->implementsInterfaces());

    StructType * typeinfo_t =
        (StructType *)compilation->frontEnd.typeinfo_struct->getType();
    llvm::StructType * ll_typeinfo_t =
        (llvm::StructType *)getOrGenType(typeinfo_t);
    BJOU_DEBUG_ASSERT(typeinfo_t);
    llvm::GlobalVariable * global_var = new llvm::GlobalVariable(
        /*Module=*/*llModule,
        /*Type=*/ll_typeinfo_t,
        /*isConstant=*/true,
        /*Linkage=*/llvm::GlobalValue::PrivateLinkage,
        /*Initializer=*/0, // has initializer, specified below
        /*Name=*/"__bjou_typeinfo_" + s->getMangledName());

    // global_var->setAlignment(layout->getPreferredAlignment(global_var));
    global_var->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);

    std::vector<llvm::Constant *> vals;

    // _v_table
    BJOU_DEBUG_ASSERT(typeinfo_t->memberIndices.count("_v_table"));
    BJOU_DEBUG_ASSERT(typeinfo_t->memberIndices["_v_table"] == 0);
    const Type * _v_table_t = typeinfo_t->memberTypes[0];
    BJOU_DEBUG_ASSERT(_v_table_t->isArray() &&
                      _v_table_t->under()->isPointer());

    vals.push_back(create_v_table_constant_array(s));

    // _typename
    BJOU_DEBUG_ASSERT(typeinfo_t->memberIndices.count("_typename"));
    BJOU_DEBUG_ASSERT(typeinfo_t->memberIndices["_typename"] == 1);
    const Type * _typename_t = typeinfo_t->memberTypes[1];
    BJOU_DEBUG_ASSERT(_typename_t->isPointer() &&
                      _typename_t->under() == CharType::get());

    vals.push_back((llvm::Constant *)createGlobalStringVariable(
        demangledString(s->getMangledName())));

    llvm::Constant * init = llvm::ConstantStruct::get(ll_typeinfo_t, vals);

    BJOU_DEBUG_ASSERT(init);

    global_var->setInitializer(init);

    globaltypeinfos[s->getMangledName()] = global_var;

    return global_var;
}

llvm::Value * LLVMBackEnd::getGlobaltypeinfo(Struct * s) {
    std::string key = s->getMangledName();
    BJOU_DEBUG_ASSERT(globaltypeinfos.count(key));
    return globaltypeinfos[key];
}

/*
void * ASTNode::generate(BackEnd & backEnd, bool flag) {
    errorl(getContext(), "unimplemented generate()", false);
    internalError("Code generation failed.");
    return nullptr;
}
*/

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
    const Type * lt = getLeft()->getType();

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

    llvm::Value * lv = (llvm::Value *)getLeft()->generate(backEnd);
    llvm::Value * rv = (llvm::Value *)getRight()->generate(backEnd);

    if (myt->isPointer()) {
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
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llvm::Value * lv  = (llvm::Value *)getLeft()->generate(backEnd, true),
                * rv  = nullptr;

    if (getLeft()->getType()->isStruct()) { // do memcpy
        rv = (llvm::Value *)getRight()->generate(backEnd, true);
       
        static bool checked_memcpy = false;
        if (!checked_memcpy) {
            if (compilation->frontEnd.memcpy_decl) {
                llbe->getOrGenNode(compilation->frontEnd.memcpy_decl);
                checked_memcpy = true;
            } else
                errorl(getContext(), "bJou is missing a memcpy declaration.", true,
                       "if using --nopreload, an extern declaration must be made "
                       "available");
        }

        llvm::Function * func = llbe->llModule->getFunction("memcpy");
        BJOU_DEBUG_ASSERT(func);

        llvm::PointerType * ll_byte_ptr_t =
            llvm::Type::getInt8Ty(llbe->llContext)->getPointerTo();

        llvm::Value * dest = llbe->builder.CreateBitCast(lv, ll_byte_ptr_t);
        llvm::Value * src  = llbe->builder.CreateBitCast(rv, ll_byte_ptr_t);
        llvm::Value * size = llvm::ConstantInt::get(
            llvm::Type::getInt64Ty(llbe->llContext),
            llbe->layout->getTypeAllocSize(llbe->getOrGenType(getLeft()->getType())));

        std::vector<llvm::Value*> args = { dest, src, size };

        llbe->builder.CreateCall(func, args);
    } else {
        rv = (llvm::Value *)getRight()->generate(backEnd);

        llvm::StoreInst * store = llbe->builder.CreateStore(rv, lv);

        if (getLeft()->getType()->isPointer())
            store->setAlignment(sizeof(void*));
    }

    if (getAddr)
        return lv;

    return llbe->builder.CreateLoad(lv, "assign_load");
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

    const Type * myt = conv(getLeft()->getType(), getRight()->getType())->unRef();

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
        lv = llbe->builder.CreatePtrToInt(lv, llvm::Type::getInt64Ty(llbe->llContext));
        rv = llbe->builder.CreatePtrToInt(rv, llvm::Type::getInt64Ty(llbe->llContext));

        return llbe->builder.CreateICmpULT(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * LeqExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * myt = conv(getLeft()->getType(), getRight()->getType())->unRef();

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
        lv = llbe->builder.CreatePtrToInt(lv, llvm::Type::getInt64Ty(llbe->llContext));
        rv = llbe->builder.CreatePtrToInt(rv, llvm::Type::getInt64Ty(llbe->llContext));

        return llbe->builder.CreateICmpULE(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * GtrExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * myt = conv(getLeft()->getType(), getRight()->getType())->unRef();

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
        lv = llbe->builder.CreatePtrToInt(lv, llvm::Type::getInt64Ty(llbe->llContext));
        rv = llbe->builder.CreatePtrToInt(rv, llvm::Type::getInt64Ty(llbe->llContext));

        return llbe->builder.CreateICmpUGT(lv, rv);
    }

    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

void * GeqExpression::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    const Type * myt = conv(getLeft()->getType(), getRight()->getType())->unRef();

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
        lv = llbe->builder.CreatePtrToInt(lv, llvm::Type::getInt64Ty(llbe->llContext));
        rv = llbe->builder.CreatePtrToInt(rv, llvm::Type::getInt64Ty(llbe->llContext));

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

    if (lt->isInt() || lt->isChar()) {
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

    if (lt->isInt() || lt->isChar()) {
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

static llvm::Value * generateInterfaceFn(BackEnd & backEnd,
                                         CallExpression * call_expr,
                                         llvm::Value * obj_ptr) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;
    llvm::PointerType * ll_byte_ptr_t =
        llvm::Type::getInt8Ty(llbe->llContext)->getPointerTo();

    BJOU_DEBUG_ASSERT(call_expr->resolved_proc);
    Procedure * proc = call_expr->resolved_proc;

    Struct * s = proc->getParentStruct();

    BJOU_DEBUG_ASSERT(s && s->getFlag(Struct::IS_ABSTRACT));

    StructType * s_t = (StructType *)s->getType();
    BJOU_DEBUG_ASSERT(s_t->implementsInterfaces());

    BJOU_DEBUG_ASSERT(s_t->interfaceIndexMap.count(proc));
    unsigned int idx = s_t->interfaceIndexMap[proc];
    llvm::Constant * idx_val =
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), idx);

    llvm::Value * typeinfo = (llvm::GlobalVariable *)llbe->getGlobaltypeinfo(s);
    BJOU_DEBUG_ASSERT(typeinfo);
    StructType * typeinfo_t =
        (StructType *)compilation->frontEnd.typeTable["typeinfo"];
    llvm::StructType * ll_typeinfo_t =
        (llvm::StructType *)llbe->getOrGenType(typeinfo_t);

    llvm::Value * typeinfo_cast = llbe->builder.CreateBitCast(
        obj_ptr, ll_typeinfo_t->getPointerTo()->getPointerTo());
    llvm::LoadInst * typeinfo_addr_load = 
        llbe->builder.CreateLoad(typeinfo_cast, "typeinfo_load");
    typeinfo_addr_load->setAlignment(sizeof(void*));
    llvm::Value * v_table_cast = llbe->builder.CreateBitCast(
        typeinfo_addr_load, ll_byte_ptr_t->getPointerTo());

    std::vector<llvm::Value *> gep_indices{idx_val};
    llvm::Value * gep =
        llbe->builder.CreateInBoundsGEP(v_table_cast, gep_indices);

    llvm::Value * callee = llbe->builder.CreateLoad(gep, "v_table_load");

    BJOU_DEBUG_ASSERT(callee);

    llvm::Value * fn = llbe->builder.CreateBitCast(
        callee, llbe->getOrGenType(proc->getType()));

    return fn;
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

    if (!getFlag(CallExpression::INTERFACE_CALL) && resolved_proc)
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

    if (getFlag(CallExpression::INTERFACE_CALL)) {
        BJOU_DEBUG_ASSERT(arglist->getExpressions().size() > 0);
        Expression * first_arg = (Expression *)arglist->getExpressions()[0];
        Struct * s = resolved_proc->getParentStruct();
        BJOU_DEBUG_ASSERT(s);
        BJOU_DEBUG_ASSERT(conv(s->getType()->getRef(), first_arg->getType()));
        BJOU_DEBUG_ASSERT(args.size() > (0 + payload->sret));
        callee = generateInterfaceFn(backEnd, this, args[0 + payload->sret]);

        llvm::Type * first_arg_t = args[0 + payload->sret]->getType();
        llvm::FunctionType * callee_t =
            (llvm::FunctionType *)callee->getType()->getPointerElementType();
        llvm::Type * first_param_t = callee_t->getParamType(0 + payload->sret);
        if (first_arg_t != first_param_t)
            args[0 + payload->sret] = llbe->builder.CreateBitCast(
                args[0 + payload->sret], first_param_t);
    } else if (!callee)
        callee = (llvm::Value *)llbe->getOrGenNode(getLeft());

    BJOU_DEBUG_ASSERT(callee);

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
        else ret = sret;
    } else {
        if (payload->t->getRetType()->isStruct() && getAddr) {
            llvm::Value * tmp_ret = llbe->allocUnnamedVal(
                                        payload->t->getRetType());
            llbe->builder.CreateStore(ret, tmp_ret);

            ret = tmp_ret;
        } 
    }

    for (int ibyval : byvals)
        callinst->addParamAttr(ibyval, llvm::Attribute::ByVal);
    for (int ibyref : byrefs) {
        const Type * param_t = payload->t->getParamTypes()[ibyref]->unRef();
        unsigned int size = simpleSizer(param_t);
        if (!size)
            size = 1;
        callinst->addParamAttr(ibyref,
                               llvm::Attribute::getWithDereferenceableBytes(
                                   llbe->llContext, size));
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
    llvm::Value * lv = nullptr,
                * rv = nullptr;
    std::vector<llvm::Value *> indices;

    if (!lt->isPointer() && !lt->isArray()) {
        BJOU_DEBUG_ASSERT(false && "Invalid subscript type");
        return nullptr;
    }

    if (lt->isArray()) {
        std::vector<unsigned int> widths, steps;  // number of steps per dimension
                                                  // int[2][2][3] -> { 1, 3, 6 }
        std::vector<llvm::Value*> indices;
        
        auto multiplicative_sum = [&](std::vector<unsigned int>& vec, int take) {
            unsigned int sum = 1;
            for (int i = 0; i < take; i += 1)
                sum *= vec[i];
            return sum;
        };

        
        ASTNode * s = this->getLeft(),
                * l = nullptr;
       
        // find bottom of subscript chain
        while (s && s->nodeKind == ASTNode::SUBSCRIPT_EXPRESSION)
            s = ((Expression*)s)->getLeft();
        // s is the array.. save it
        ASTNode * the_array = s;
        // go back up one to find the last subscript
        s = s->parent;
        // go back up, gathering widths
        while (s != this->parent) {
            Expression * e = (Expression*)s;
            const Type * t = e->getLeft()->getType();
            if (!t->isArray()) break;
            ArrayType * a_t = (ArrayType*)t;

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
            Expression * e = (Expression*)s;
            const Type * t = e->getLeft()->getType();
            if (!t->isArray()) break;
            ArrayType * a_t = (ArrayType*)t;

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
            llvm::Value * step = llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), steps[i]);
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
        t = ((SliceType*)t)->getRealType();
    else if (t->isDynamicArray())
        t = ((DynamicArrayType*)t)->getRealType();

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

            if (s_t->constantMap.count(r_id->getUnqualified()))
                return llbe->getOrGenNode(
                    s_t->constantMap[r_id->getUnqualified()]);

            // regular structure access

            name = "structure_access";
            elem = s_t->memberIndices[r_id->getUnqualified()];
            if (s_t->implementsInterfaces())
                elem += 1; // account for typeinfo member
            // if (s_t->_struct->getMangledName() == "typeinfo")
            // elem -= 1;
        } else if (pt->under()->isTuple()) {
            name = "tuple_access";
            t_t = (TupleType *)t->under();
            elem = (int)r_elem->eval().as_i64;
        }
    } else if (t->isStruct()) {
        s_t = (StructType *)t;
        // @refactor type member constants
        // should this be here or somewhere in the frontend?

        if (s_t->constantMap.count(r_id->getUnqualified()))
            return llbe->getOrGenNode(s_t->constantMap[r_id->getUnqualified()]);

        // regular structure access

        name = "structure_access";
        left_val = (llvm::Value *)llbe->getOrGenNode(getLeft(), true);
        elem = s_t->memberIndices[r_id->getUnqualified()];
        if (s_t->implementsInterfaces())
            elem += 1; // account for typeinfo member
        // if (s_t->_struct->getMangledName() == "typeinfo")
        // elem -= 1;
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
        load->setAlignment(sizeof(void*));

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
        // initialize struct typeinfos in arrays
    } else {
        // store typeinfo
        if (r_t->isStruct()) {
            StructType * s_t = (StructType *)r_t;
            Struct * s = s_t->_struct;

            if (s_t->implementsInterfaces()) {
                llvm::Value * ti_val = llbe->getGlobaltypeinfo(s);
                BJOU_DEBUG_ASSERT(ti_val);
                llbe->builder.CreateStore(
                    ti_val,
                    llbe->builder.CreateInBoundsGEP(
                        val,
                        {llvm::Constant::getNullValue(
                             llvm::IntegerType::getInt32Ty(llbe->llContext)),
                         llvm::Constant::getNullValue(
                             llvm::IntegerType::getInt32Ty(llbe->llContext))}));
            }
        }
        //
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

    llvm::Value * val = (llvm::Value *)llbe->getOrGenNode(getLeft());
    llvm::Type * ll_rt = llbe->getOrGenType(rt);

    if (lt->isInt() && rt->isInt()) {
        return llbe->builder.CreateIntCast(
            val, ll_rt, ((const IntType *)rt)->sign == Type::SIGNED);
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
    }

    // @incomplete
    if (lt->isPointer() && rt->isPointer())
        return llbe->builder.CreateBitCast(val, ll_rt);
    else if (lt->isPointer() && lt->under() == VoidType::get() &&
             rt->isProcedure()) {
        return llbe->builder.CreateBitCast(val, ll_rt);
    } else if (lt->isProcedure() && rt->isPointer() && rt->under() == VoidType::get()) {
        return llbe->builder.CreateBitCast(val, ll_rt);
    } else if (lt->isArray() && rt->isPointer()) {
        ArrayType * a_t = (ArrayType *)lt;
        PointerType * p_t = (PointerType *)rt;

        if (equal(a_t->under(), p_t->under()) ||
            p_t->under() == VoidType::get())
            return llbe->builder.CreateBitCast(val, ll_rt);
    }

    BJOU_DEBUG_ASSERT(false);

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
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext),
                                  getContents(), 10); // base 10
}

void * FloatLiteral::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;
    return llvm::ConstantFP::get(llvm::Type::getDoubleTy(llbe->llContext),
                                 getContents());
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

void * TupleLiteral::generate(BackEnd & backEnd, bool flag) {
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
            (llvm::Value *)llbe->getOrGenNode(subExpressions[i]), elem);
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

    llvm::Value * ptr = llbe->getNamedVal(qualified);

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

    llvm::LoadInst * load = llbe->builder.CreateLoad(ptr, unqualified);
    if (getType()->isPointer() || getType()->isRef())
        load->setAlignment(sizeof(void*));

    return load;
}

llvm::Value * LLVMBackEnd::getPointerToArrayElements(llvm::Value * array) {
    std::vector<llvm::Value *> indices{
        llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llContext)),
        llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llContext))};
    return builder.CreateInBoundsGEP(array, indices);
}

llvm::Value *
LLVMBackEnd::createPointerToArrayElementsOnStack(llvm::Value * array, const Type * t) {
    BJOU_DEBUG_ASSERT(array->getType()->isPointerTy() &&
                      array->getType()->getPointerElementType()->isArrayTy());
    llvm::Value * ptr = allocUnnamedVal(t->under()->getPointer());
    builder.CreateStore(getPointerToArrayElements(array), ptr);
    return ptr;
}

llvm::Value *
LLVMBackEnd::copyConstantInitializerToStack(llvm::Constant * constant_init, const Type * t) {
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

        const unsigned int typeinfo_offset =
            (unsigned int)s_t->implementsInterfaces();

        std::vector<std::string> & names = ilist->getMemberNames();
        std::vector<llvm::Constant *> vals(
            s_t->memberTypes.size() + typeinfo_offset, nullptr);

        if (typeinfo_offset)
            vals[0] = (llvm::Constant *)getGlobaltypeinfo(s_t->_struct);

        for (int i = 0; i < (int)names.size(); i += 1) {
            // we will assume everything works out since it passed analysis
            int index = typeinfo_offset + s_t->memberIndices[names[i]];
            vals[index] = (llvm::Constant *)getOrGenNode(expressions[i]);
        }
        for (int i = typeinfo_offset; i < (int)vals.size(); i += 1)
            if (!vals[i])
                vals[i] = llvm::Constant::getNullValue(
                    getOrGenType(s_t->memberTypes[i - typeinfo_offset]));
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
                ArrayType * a_t = (ArrayType*)t;
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

        const unsigned int typeinfo_offset =
            (unsigned int)s_t->implementsInterfaces();

        std::vector<std::string> & names = getMemberNames();

        alloca = (llvm::AllocaInst*)llbe->allocUnnamedVal(myt);
        ptr = llbe->builder.CreateInBoundsGEP(
            alloca, {llvm::Constant::getNullValue(
                        llvm::IntegerType::getInt32Ty(llbe->llContext))});
        alloca->setName("structinitializer");

        BJOU_DEBUG_ASSERT(names.size() == expressions.size());

        std::vector<llvm::Value *> vals;
        vals.resize(s_t->memberTypes.size() + typeinfo_offset, nullptr);

        for (int i = 0; i < (int)names.size(); i += 1) {
            int index = s_t->memberIndices[names[i]] + typeinfo_offset;
            const Type * dest_t = s_t->memberTypes[s_t->memberIndices[names[i]]];
            if (dest_t->isRef())
                vals[index] = (llvm::Value *)llbe->getOrGenNode(expressions[i], true);
            else 
                vals[index] = (llvm::Value *)llbe->getOrGenNode(expressions[i]);
        }

        if (typeinfo_offset)
            vals[0] = llbe->getGlobaltypeinfo(s_t->_struct);

        // record uninitialized array members so that we can do a 
        // memset to zero later
        std::map<size_t, ArrayType*> uninit_array_elems;
        for (size_t i = 0; i < s_t->memberTypes.size(); i += 1) {
            if (s_t->memberTypes[i]->isArray()) {
                size_t index = i + typeinfo_offset;
                if (!vals[index])
                    uninit_array_elems[index] = (ArrayType*)s_t->memberTypes[i];
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

            const Type * elem_t = nullptr;
            if (i != 0 || !typeinfo_offset)
                elem_t = s_t->memberTypes[i - typeinfo_offset];

            if (!vals[i]) {
                llvm::Value * null_val = llvm::Constant::getNullValue(llbe->getOrGenType(elem_t));
                llbe->builder.CreateStore(null_val, elem);
            } else {
                if (i != 0 || !typeinfo_offset) {
                    llvm::StoreInst * store = llbe->builder.CreateStore(vals[i], elem);
                
                    if (elem_t->isPointer() || elem_t->isRef())
                        store->setAlignment(sizeof(void*));
                } else { // typeinfo * 
                    llvm::StoreInst * store = llbe->builder.CreateStore(vals[i], elem);
                    store->setAlignment(sizeof(void*));
                }
            }
        }

        // memset uninitialized array members to zero
        for (auto uninit : uninit_array_elems) {
            const Type * elem_t = uninit.second->under();

            llvm::PointerType * ll_byte_ptr_t =
             
                llvm::Type::getInt8Ty(llbe->llContext)->getPointerTo();
            if (compilation->frontEnd.malloc_decl)
                llbe->getOrGenNode(compilation->frontEnd.malloc_decl);
            else
                errorl(getContext(), "bJou is missing a memset declaration.", true,
                       "if using --nopreload, an extern declaration must be made "
                       "available");

            llvm::Function * func = llbe->llModule->getFunction("memset");
            BJOU_DEBUG_ASSERT(func);

            llvm::Value * Ptr   = nullptr,
                        * Val   = nullptr,
                        * Size  = nullptr;


            Ptr = llbe->builder.CreateInBoundsGEP(
                ptr, {llvm::Constant::getNullValue(
                          llvm::IntegerType::getInt32Ty(llbe->llContext)),
                      llvm::ConstantInt::get(
                          llvm::Type::getInt32Ty(llbe->llContext), uninit.first)});
            Ptr = llbe->builder.CreateBitCast(Ptr, ll_byte_ptr_t);
            
            Val = llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llbe->llContext));

            Size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(llbe->llContext),
                    uninit.second->width *
                    llbe->layout->getTypeAllocSize(llbe->getOrGenType(elem_t)));
            
            std::vector<llvm::Value*> args = { Ptr, Val, Size };

            llbe->builder.CreateCall(func, args);
        }

        if (getAddr)
            return ptr;

        ldptr = llbe->builder.CreateLoad(alloca, "structinitializer_ld");
    } else if (myt->isArray()) {
        alloca = (llvm::AllocaInst*)llbe->allocUnnamedVal(myt);
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

    if (!getScope()->parent || getScope()->nspace) {
        if (llbe->generated_nodes.find(this) == llbe->generated_nodes.end()) {
            llbe->globs_need_completion.insert(this);
            return GenerateGlobalVariable(this, nullptr, backEnd);
        } else {
            llvm::Value * me = llbe->getOrGenNode(this);
            return GenerateGlobalVariable(this, (llvm::GlobalVariable*)me, backEnd);
        }
    }

    llvm::Type * t = llbe->getOrGenType(getType());
    llvm::Value * val = nullptr;

    if (getType()->isRef()) {
        BJOU_DEBUG_ASSERT(getInitialization());

        if (getType()->unRef()->isArray()) {
            val = llbe->allocNamedVal(getMangledName(), getType()->unRef()->under()->getPointer());
            llbe->builder.CreateStore(llbe->getOrGenNode(getInitialization(), true), val);
        } else {
            val = llbe->addNamedVal(
                getMangledName(),
                (llvm::Value *)llbe->getOrGenNode(getInitialization(), true),
                getType());
        }
    } else {
        val = llbe->allocNamedVal(getMangledName(), getType());
        BJOU_DEBUG_ASSERT(val);

        if (getType()->isArray()) {
            // @incomplete
            // initialize struct typeinfos for arrays
        } else {
            if (getType()->isStruct()          ||
                getType()->isSlice()           ||
                getType()->isDynamicArray()) {

                StructType * s_t = (StructType *)getType();

                if (getType()->isSlice())
                    s_t = (StructType*)((SliceType*)getType())->getRealType();
                if (getType()->isDynamicArray())
                    s_t = (StructType*)((DynamicArrayType*)getType())->getRealType();

                Struct * s = s_t->_struct;
                // store typeinfo
                if (s_t->implementsInterfaces()) {
                    llvm::Value * ti = llbe->getGlobaltypeinfo(s);
                    llbe->builder.CreateStore(
                        ti, llbe->builder.CreateInBoundsGEP(
                                val, {llvm::Constant::getNullValue(
                                          llvm::IntegerType::getInt32Ty(
                                              llbe->llContext)),
                                      llvm::Constant::getNullValue(
                                          llvm::IntegerType::getInt32Ty(
                                              llbe->llContext))}));
                }
            }
            //
        }

        if (getInitialization() && !getType()->isRef()) {
            if (getType()->isStruct()) {
                static bool checked_memcpy = false;
                if (!checked_memcpy) {
                    if (compilation->frontEnd.memcpy_decl) {
                        llbe->getOrGenNode(compilation->frontEnd.memcpy_decl);
                        checked_memcpy = true;
                    } else
                        errorl(getContext(), "bJou is missing a memcpy declaration.", true,
                               "if using --nopreload, an extern declaration must be made "
                               "available");
                }

                llvm::Function * func = llbe->llModule->getFunction("memcpy");
                BJOU_DEBUG_ASSERT(func);

                llvm::Type * ll_t = llbe->getOrGenType(getType());

                llvm::Value * init_v = (llvm::Value *)llbe->getOrGenNode(getInitialization(), true);

                llbe->builder.CreateMemCpy(val, init_v, llbe->layout->getTypeAllocSize(ll_t), llbe->layout->getABITypeAlignment(ll_t));         
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

        const unsigned int typeinfo_offset =
            (unsigned int)s_t->implementsInterfaces();

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
                backEnd, llbe->getOrGenType(s_t), val, i + typeinfo_offset);
            if (!mem_t->isStruct())
                mem_val = llbe->builder.CreateLoad(mem_val);
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
            } else
                val = t->isStruct()
                          ? (llvm::Value *)llbe->getOrGenNode(expr, true)
                          : (llvm::Value *)llbe->getOrGenNode(expr);

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
    llvm::BasicBlock * merge =
        llvm::BasicBlock::Create(llbe->llContext, "merge");

    if (hasElse)
        llbe->builder.CreateCondBr(cond, then, _else);
    else {
        llbe->builder.CreateCondBr(cond, then, merge);
    }

    // Emit then value.
    llbe->builder.SetInsertPoint(then);

    for (ASTNode * statement : getStatements())
        llbe->getOrGenNode(statement);

    if (!getFlag(HAS_TOP_LEVEL_RETURN)) {
        generateFramePreExit(llbe);
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

        if (!_Else->getFlag(HAS_TOP_LEVEL_RETURN)) {
            generateFramePreExit(llbe);
            llbe->builder.CreateBr(merge);
        }

        llbe->popFrame();

        _else = llbe->builder.GetInsertBlock();
    }

    // Codegen of 'Then' can change the current block, update ThenBB for the
    // PHI.
    then = llbe->builder.GetInsertBlock();

    // Emit merge block.
    func->getBasicBlockList().push_back(merge);
    llbe->builder.SetInsertPoint(merge);

    return merge; // what does this accomplish?
}

void * For::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    llbe->pushFrame();

    for (ASTNode * init : getInitializations())
        llbe->getOrGenNode(init);

    llvm::Function * func = llbe->builder.GetInsertBlock()->getParent();

    llvm::BasicBlock * check =
        llvm::BasicBlock::Create(llbe->llContext, "forcheckcond", func);

    llbe->builder.CreateBr(check);

    llbe->builder.SetInsertPoint(check);

    llvm::Value * cond = (llvm::Value *)llbe->getOrGenNode(getConditional());
    // Convert condition to a bool by comparing equal to 1.
    cond = llbe->builder.CreateICmpEQ(
        cond, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)),
        "forcond");

    llvm::BasicBlock * then = llvm::BasicBlock::Create(llbe->llContext, "then");
    llvm::BasicBlock * after =
        llvm::BasicBlock::Create(llbe->llContext, "after");
    llvm::BasicBlock * merge =
        llvm::BasicBlock::Create(llbe->llContext, "merge");

    llbe->loop_continue_stack.push({llbe->curFrame(), after});
    llbe->loop_break_stack.push({llbe->curFrame(), merge});

    llbe->builder.SetInsertPoint(check);

    llbe->builder.CreateCondBr(cond, then, merge);

    // Emit then value.
    func->getBasicBlockList().push_back(then);
    llbe->builder.SetInsertPoint(then);

    for (ASTNode * statement : getStatements())
        llbe->getOrGenNode(statement);

    // afterthoughts
    if (!getFlag(HAS_TOP_LEVEL_RETURN)) {
        generateFramePreExit(llbe);
        llbe->builder.CreateBr(after);
    }

    // afterthoughts and jump back to check
    func->getBasicBlockList().push_back(after);
    llbe->builder.SetInsertPoint(after);

    for (ASTNode * at : getAfterthoughts())
        llbe->getOrGenNode(at);

    // if (!getFlag(HAS_TOP_LEVEL_RETURN)) {
    llbe->builder.CreateBr(check);
    // }

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
        generateFramePreExit(llbe);
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

void * Procedure::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    // due to the nature of the on-demand codegen model,
    // some interface decls may slip in here.. just ignore
    if (getFlag(IS_INTERFACE_DECL))
        return nullptr;

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
        llbe->proc_abi_info[this] = payload;
        func_type = (llvm::FunctionType *)payload->fn_t;
        //

        llvm::Function::LinkageTypes linkage =
            getFlag(Procedure::IS_EXTERN) // || getFlag(Procedure::IS_ANONYMOUS)
                ? llvm::Function::ExternalLinkage
                : llvm::Function::InternalLinkage;

        func = llvm::Function::Create(func_type, linkage, getMangledName(),
                                      llbe->llModule);

        BJOU_DEBUG_ASSERT(func);

        // we go ahead and put func into the generated_nodes table so that
        // any recursive references do not cause problems
        llbe->generated_nodes[this] = func;

        if (linkage == llvm::Function::LinkageTypes::InternalLinkage)
            llbe->procs_need_completion.insert(this);

        int i = 0;
        for (auto & arg : func->args()) {
            if (payload->byval & (1 << i))
                arg.addAttr(llvm::Attribute::ByVal);
            if (payload->ref & (1 << i)) {
                const Type * param_t = payload->t->getParamTypes()[i]->unRef();
                unsigned int size = simpleSizer(param_t);
                if (!size)
                    size = 1;
                arg.addAttr(llvm::Attribute::getWithDereferenceableBytes(
                    llbe->llContext, size));
            } else if (arg.getType()->isPointerTy()) {
                // arg.addAttr(llvm::Attribute::getWithAlignment(llbe->llContext,
                //                                               sizeof(void *)));
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

        // @abi
        ABILowerProcedureTypeData * payload =
            (ABILowerProcedureTypeData *)llbe->proc_abi_info[this];
        //

        int i = 0;
        for (auto & arg : func->args()) {
            if (i == 0 && payload->sret)
                arg.setName("__bjou_sret");
            else
                arg.setName(((VariableDeclaration *)(getParamVarDeclarations()
                                                         [i - payload->sret]))
                                ->getName());
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
            if (j - payload->sret >= 0)
                t = getParamVarDeclarations()[j - payload->sret]->getType();
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
                if (payload->byval & (1 << j))
                    val = llbe->builder.CreateLoad(val, "byval");

                llbe->builder.CreateStore(val, alloca);
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

            generateFramePreExit(llbe);
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
            func->dump();
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
        llvm::Value * val = (llvm::Value *)llbe->getOrGenNode(getExpression(), true);

        const Type * t = getExpression()->getType();
        llvm::Type * ll_t = llbe->getOrGenType(t);

        llvm::PointerType * ll_byte_ptr_t =
            llvm::Type::getInt8Ty(llbe->llContext)->getPointerTo();

        sret = llbe->builder.CreateBitCast(sret, ll_byte_ptr_t);
        val  = llbe->builder.CreateBitCast(val, ll_byte_ptr_t);

        llvm::Value * size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(llbe->llContext),
                                llbe->layout->getTypeAllocSize(ll_t));

        llbe->builder.CreateMemCpy(sret, val, size, llbe->layout->getABITypeAlignment(ll_t));

        generateFramePreExit(llbe);
        return llbe->builder.CreateRetVoid();
    }
    if (getExpression()) {
        llvm::Value * v = nullptr;
        if (pt->getRetType()->isRef()) {
            v = (llvm::Value *)llbe->getOrGenNode(getExpression(), true);
        } else {
            v = (llvm::Value *)llbe->getOrGenNode(getExpression());
        }

        BJOU_DEBUG_ASSERT(v);

        generateFramePreExit(llbe);
        return llbe->builder.CreateRet(v);
    }
    generateFramePreExit(llbe);
    return llbe->builder.CreateRetVoid();
}

void * Namespace::generate(BackEnd & backEnd, bool flag) {
    for (ASTNode * node : getNodes())
        ((LLVMBackEnd *)&backEnd)->getOrGenNode(node);
    return nullptr;
}

void * Struct::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    std::string & mname = getMangledName();
    llvm::StructType * ll_s_ty = nullptr;
    StructType * bjou_s_ty = nullptr;

    Struct * typeinfo_struct = compilation->frontEnd.typeinfo_struct;

    if (llbe->generated_nodes.find(this) == llbe->generated_nodes.end()) {
        genDeps(this, *llbe);
        llbe->types_need_completion.insert(this);
    } else {
        std::vector<llvm::Type *> member_types;

        ll_s_ty = (llvm::StructType *)llbe->getOrGenType(getType());

        bjou_s_ty = (StructType *)getType();

        if (bjou_s_ty->isAbstract && bjou_s_ty->memberTypes.empty() &&
            !bjou_s_ty->implementsInterfaces()) {
            return ll_s_ty; // leave opaque
        }

        if (bjou_s_ty->implementsInterfaces()) {
            if (typeinfo_struct && this != typeinfo_struct)
                member_types.push_back(
                    llbe->getOrGenType(typeinfo_struct->getType())
                        ->getPointerTo());
        }

        for (const Type * bjou_mem_t : bjou_s_ty->memberTypes) {
            llvm::Type * ll_mem_t = llbe->getOrGenType(bjou_mem_t);
            BJOU_DEBUG_ASSERT(ll_mem_t);
            member_types.push_back(ll_mem_t);
        }
        ll_s_ty->setBody(member_types);

        if (bjou_s_ty->implementsInterfaces()) {
            if (typeinfo_struct && this != typeinfo_struct)
                llbe->createGlobaltypeinfo(this);
        }
    }

    return ll_s_ty;
}

void * Break::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    auto & lfi = llbe->loop_break_stack.top();

    generateFramePreExit(llbe, lfi.frame);
    return llbe->builder.CreateBr(lfi.bb);
}

void * Continue::generate(BackEnd & backEnd, bool flag) {
    LLVMBackEnd * llbe = (LLVMBackEnd *)&backEnd;

    auto & lfi = llbe->loop_continue_stack.top();

    generateFramePreExit(llbe, lfi.frame);
    return llbe->builder.CreateBr(lfi.bb);
}

} // namespace bjou
