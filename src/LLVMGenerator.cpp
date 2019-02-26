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
            // llvm::createFunctionInliningPass(OptLevel, SizeLevel, false);
            llvm::createFunctionInliningPass(
                OptLevel, SizeLevel
#if LLVM_VERSION_MAJOR > 4
                ,
                false /* DisableInlineHotCallSite */
#endif
            );
    } else {
        Builder.Inliner = llvm::createAlwaysInlinerLegacyPass();
    }
    Builder.DisableUnitAtATime = false;
    Builder.DisableUnrollLoops = false;

    Builder.LoopVectorize = OptLevel > 1 && SizeLevel < 2;
    Builder.SLPVectorize = OptLevel > 1 && SizeLevel < 2;

    if (TM) {
#if LLVM_VERSION_MAJOR > 4
        TM->adjustPassManager(Builder);
#endif
    }

    Builder.populateFunctionPassManager(FPM);
    Builder.populateModulePassManager(MPM);

#if !LLVM_VERSION_MAJOR > 4
    TM->addEarlyAsPossiblePasses(FPM);
    TM->addEarlyAsPossiblePasses(MPM);
#endif
}

namespace bjou {
LLVMGenerator::LLVMGenerator(LLVMBackEnd & _backEnd) : backEnd(_backEnd) {}

void LLVMGenerator::generate() {
    llvm::legacy::PassManager pass;
    llvm::legacy::FunctionPassManager fpass(backEnd.llModule);
    llvm::TargetMachine::CodeGenFileType ftype =
        llvm::TargetMachine::CGFT_ObjectFile;

    // Add an appropriate TargetLibraryInfo pass for the module's triple.
    llvm::TargetLibraryInfoImpl TLII(
        llvm::Triple(backEnd.llModule->getTargetTriple()));

    pass.add(new llvm::TargetLibraryInfoWrapperPass(TLII));
    pass.add(createTargetTransformInfoWrapperPass(
        backEnd.targetMachine->getTargetIRAnalysis()));

    if (compilation->args.opt_arg) {
        fpass.add(createTargetTransformInfoWrapperPass(
            backEnd.targetMachine->getTargetIRAnalysis()));
        AddOptimizationPasses(pass, fpass, backEnd.targetMachine, 3, 0);
    }

    llvm::raw_fd_ostream * dest = nullptr;
    std::error_code EC;

    if (!(compilation->args.c_arg && compilation->args.emitllvm_arg)) {
        std::string output;
        if (!compilation->args.output_arg.empty()) {
            output = compilation->args.output_arg;
            if (!compilation->args.c_arg)
                output += ".o";
        } else {
            output = compilation->outputpath + compilation->outputbasefilename +
                     ".o";
        }

        dest = new llvm::raw_fd_ostream(output, EC, llvm::sys::fs::F_None);

        if (EC) {
            error(Context(),
                  "Could not open output file '" + output + "' for object code emission.");
        }

#if LLVM_VERSION_MAJOR >= 7
        if (backEnd.targetMachine->addPassesToEmitFile(pass, *dest, nullptr,
                                                              ftype))
#else
        if (backEnd.targetMachine->addPassesToEmitFile(pass, *dest,
                                                              ftype))
#endif
            error(Context(), "TargetMachine can't emit a file of this type");
    }

    pass.run(*(backEnd.llModule));

    if (dest) {
        dest->flush();
        delete dest;
    }

    if (compilation->args.emitllvm_arg) {
        std::string output;
        if (!compilation->args.output_arg.empty()) {
            output = compilation->args.output_arg;
            if (!compilation->args.c_arg)
                output += ".ll";
        } else {
            output = compilation->outputpath + compilation->outputbasefilename +
                     ".ll";
        }

        llvm::raw_fd_ostream ll_dest(output, EC, llvm::sys::fs::F_None);

        backEnd.llModule->print(ll_dest, nullptr, true);
    }
}
} // namespace bjou
