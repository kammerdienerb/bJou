//
//  LLVMGenerator.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "LLVMGenerator.hpp"

#include "CLI.hpp"
#include "ASTNode.hpp"
#include "LLVMBackEnd.hpp"

#ifdef BJOU_DEBUG_BUILD
#define SAVE_BJOU_DEBUG_BUILD
#endif
#undef BJOU_DEBUG_BUILD
#include "tclap/CmdLine.h"
#ifdef SAVE_BJOU_DEBUG_BUILD
#define BJOU_DEBUG_BUILD
#endif

#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Target/TargetMachine.h>


static void AddOptimizationPasses(llvm::legacy::PassManagerBase &MPM,
                                  llvm::legacy::FunctionPassManager &FPM,
                                  llvm::TargetMachine *TM, unsigned OptLevel,
                                  unsigned SizeLevel) {
    llvm::PassManagerBuilder Builder;
    Builder.OptLevel = OptLevel;
    Builder.SizeLevel = SizeLevel;
    
    if (OptLevel > 1) {
        Builder.Inliner = llvm::createFunctionInliningPass(OptLevel, SizeLevel, false);
    } else {
        Builder.Inliner = llvm::createAlwaysInlinerLegacyPass();
    }
    Builder.DisableUnitAtATime = false;
    Builder.DisableUnrollLoops = false;
    
    // If option wasn't forced via cmd line (-vectorize-loops, -loop-vectorize)
    Builder.LoopVectorize = OptLevel > 1 && SizeLevel < 2;
    
    // When #pragma vectorize is on for SLP, do the same as above
    Builder.SLPVectorize = OptLevel > 1 && SizeLevel < 2;
    
    if (TM)
        TM->adjustPassManager(Builder);
    
    Builder.populateFunctionPassManager(FPM);
    Builder.populateModulePassManager(MPM);
}


namespace bjou {
    LLVMGenerator::LLVMGenerator(LLVMBackEnd& _backEnd) : backEnd(_backEnd) {  }
    
    void LLVMGenerator::generate() {
        std::error_code EC;
        llvm::raw_fd_ostream dest((compilation->outputpath + compilation->outputbasefilename + ".o"), EC, llvm::sys::fs::F_None);
        
        if (EC)
            error(Context(), "Could not open output file for object code emission.");
        
        llvm::legacy::PassManager pass;
        llvm::legacy::FunctionPassManager fpass(backEnd.llModule);
        llvm::PassRegistry& Registry = *llvm::PassRegistry::getPassRegistry();
        llvm::TargetMachine::CodeGenFileType ftype = llvm::TargetMachine::CGFT_ObjectFile;
        
        // Add an appropriate TargetLibraryInfo pass for the module's triple.
        llvm::TargetLibraryInfoImpl TLII(llvm::Triple(backEnd.llModule->getTargetTriple()));
        
        pass.add(new llvm::TargetLibraryInfoWrapperPass(TLII));
        pass.add(createTargetTransformInfoWrapperPass(backEnd.defaultTargetMachine->getTargetIRAnalysis()));
        
        if (compilation->args.opt_arg.getValue()) {
            initializeCore(Registry);
            initializeCoroutines(Registry);
            initializeScalarOpts(Registry);
            initializeObjCARCOpts(Registry);
            initializeVectorization(Registry);
            initializeIPO(Registry);
            initializeAnalysis(Registry);
            initializeTransformUtils(Registry);
            initializeInstCombine(Registry);
            initializeInstrumentation(Registry);
            initializeTarget(Registry);
            // For codegen passes, only passes that do IR to IR transformation are
            // supported.
            initializeCodeGenPreparePass(Registry);
            initializeAtomicExpandPass(Registry);
            initializeRewriteSymbolsLegacyPassPass(Registry);
            initializeWinEHPreparePass(Registry);
            initializeDwarfEHPreparePass(Registry);
            initializeSafeStackLegacyPassPass(Registry);
            initializeSjLjEHPreparePass(Registry);
            initializePreISelIntrinsicLoweringLegacyPassPass(Registry);
            initializeGlobalMergePass(Registry);
            initializeInterleavedAccessPass(Registry);
            initializeCountingFunctionInserterPass(Registry);
            initializeUnreachableBlockElimLegacyPassPass(Registry);
            
            fpass.add(createTargetTransformInfoWrapperPass(backEnd.defaultTargetMachine->getTargetIRAnalysis()));
            
            AddOptimizationPasses(pass, fpass, backEnd.defaultTargetMachine, 3, 0);
        }
        
        if (backEnd.defaultTargetMachine->addPassesToEmitFile(pass, dest, ftype))
            error(Context(), "TargetMachine can't emit a file of this type");
        
        pass.run(*(backEnd.llModule));
        dest.flush();
    }
}
