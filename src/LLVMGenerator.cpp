//
//  LLVMGenerator.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "LLVMGenerator.hpp"

#include "ASTNode.hpp"
#include "CLI.hpp"
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
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

static void AddOptimizationPasses(llvm::legacy::PassManagerBase & MPM,
                                  llvm::legacy::FunctionPassManager & FPM,
                                  llvm::TargetMachine * TM, unsigned OptLevel,
                                  unsigned SizeLevel) {
    llvm::PassManagerBuilder Builder;
    Builder.OptLevel = OptLevel;
    Builder.SizeLevel = SizeLevel;

    if (OptLevel > 1) {
        Builder.Inliner =
            llvm::createFunctionInliningPass(OptLevel, SizeLevel, false);
    } else {
        Builder.Inliner = llvm::createAlwaysInlinerLegacyPass();
    }
    Builder.DisableUnitAtATime = false;
    Builder.DisableUnrollLoops = false;

    Builder.LoopVectorize = OptLevel > 1 && SizeLevel < 2;
    Builder.SLPVectorize = OptLevel > 1 && SizeLevel < 2;

    if (TM)
        TM->adjustPassManager(Builder);

    Builder.populateFunctionPassManager(FPM);
    Builder.populateModulePassManager(MPM);
}

namespace bjou {
LLVMGenerator::LLVMGenerator(LLVMBackEnd & _backEnd) : backEnd(_backEnd) {}

void LLVMGenerator::generate() {
    std::error_code EC;
    llvm::raw_fd_ostream dest(
        (compilation->outputpath + compilation->outputbasefilename + ".o"), EC,
        llvm::sys::fs::F_None);

    if (EC)
        error(Context(),
              "Could not open output file for object code emission.");

    llvm::legacy::PassManager pass;
    llvm::legacy::FunctionPassManager fpass(backEnd.llModule);
    llvm::TargetMachine::CodeGenFileType ftype =
        llvm::TargetMachine::CGFT_ObjectFile;

    // Add an appropriate TargetLibraryInfo pass for the module's triple.
    llvm::TargetLibraryInfoImpl TLII(
        llvm::Triple(backEnd.llModule->getTargetTriple()));

    pass.add(new llvm::TargetLibraryInfoWrapperPass(TLII));
    pass.add(createTargetTransformInfoWrapperPass(
        backEnd.defaultTargetMachine->getTargetIRAnalysis()));

    if (compilation->args.opt_arg.getValue()) {
        fpass.add(createTargetTransformInfoWrapperPass(
            backEnd.defaultTargetMachine->getTargetIRAnalysis()));
        AddOptimizationPasses(pass, fpass, backEnd.defaultTargetMachine, 3, 0);
    }

    if (backEnd.defaultTargetMachine->addPassesToEmitFile(pass, dest, ftype))
        error(Context(), "TargetMachine can't emit a file of this type");

    pass.run(*(backEnd.llModule));
    dest.flush();
}
} // namespace bjou
