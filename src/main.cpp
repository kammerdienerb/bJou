//
//  main.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/4/17.
//  Copyright © 2017 me. All rights reserved.
//

#include <bitset>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>

#include "ASTNode.hpp"
#include "BackEnd.hpp"
#include "CLI.hpp"
#include "Compile.hpp"
#include "Defaults.hpp"
#include "FrontEnd.hpp"
#include "Global.hpp"
#include "LLVMBackEnd.hpp"
#include "Misc.hpp"

#ifdef BJOU_DEBUG_BUILD
#define SAVE_BJOU_DEBUG_BUILD
#endif
#undef BJOU_DEBUG_BUILD
#include "tclap/CmdLine.h"
#ifdef SAVE_BJOU_DEBUG_BUILD
#define BJOU_DEBUG_BUILD
#endif

namespace bjou {
struct ASTNode;
Compilation * compilation = nullptr;
} // namespace bjou
std::mutex cli_mtx;

int main(int argc, const char ** argv) {
    std::srand(std::time(NULL));

    // below are the command line options that this compiler takes.
    TCLAP::CmdLine cmd_line("bJou\nA friendly language and compiler written by "
                            "Brandon Kammerdiener",
                            ' ', BJOU_VER_STR);
    bJouOutput output;
    cmd_line.setOutput(&output);

    TCLAP::SwitchArg verbose_arg("v", "verbose", "Print the LLVM IR to STDOUT.",
                                 cmd_line);
    TCLAP::SwitchArg front_arg(
        "", "front", "Only run the front end of the compiler.", cmd_line);
    TCLAP::SwitchArg time_arg(
        "", "time", "Print the running times of compilation stages to STDOUT.",
        cmd_line);
    TCLAP::SwitchArg symbols_arg("", "symbols",
                                 "Print symbol tables to STDOUT.", cmd_line);
    TCLAP::SwitchArg noparallel_arg(
        "", "noparallel", "Turn compilation parallelization off.", cmd_line);
    TCLAP::SwitchArg opt_arg("O", "optimize", "Run LLVM optimization passes.",
                             cmd_line);
    TCLAP::SwitchArg noabc_arg("", "noabc", "Turn off array bounds checking.",
                               cmd_line);
    TCLAP::SwitchArg module_arg(
        "m", "module", "Create a module file instead of an executable.",
        cmd_line);
    TCLAP::MultiArg<std::string> module_search_path_arg(
        "I", "searchpath", "Add an additional module search path.", false,
        "path", cmd_line);
    TCLAP::SwitchArg nopreload_arg(
        "", "nopreload", "Do not automatically import preload modules.",
        cmd_line);
    TCLAP::SwitchArg lld_arg(
        "", "lld",
        "Attempt to use lld to link. If unsuccessful, use system linker.",
        cmd_line);
    TCLAP::SwitchArg c_arg("c", "nolink", "Compile but do not link.", cmd_line);
    TCLAP::SwitchArg emitllvm_arg("", "emitllvm",
                                  "Output an LLVM bytecode file.", cmd_line);
    TCLAP::ValueArg<std::string> output_arg("o", "output",
                                            "Name of target output file.",
                                            false, "", "file name", cmd_line);
    TCLAP::ValueArg<std::string> target_triple_arg("t", "targettriple",
                                            "LLVM target triple string to target.",
                                            false, "", "triple", cmd_line);
    TCLAP::ValueArg<std::string> march_arg("", "march",
                                            "Architecture string to target.", false, "", "architecture", cmd_line);
    TCLAP::MultiArg<std::string> link_arg("l", "link",
                                          "Name of libraries to link.", false,
                                          "library name", cmd_line);
    TCLAP::UnlabeledMultiArg<std::string> files(
        "files", "Input source files.", false, "file name(s)",
        cmd_line); // this actually is required, but I wanted to provide the
                   // error message instead of tclap

    cmd_line.parse(argc, (char **)argv); // cast away constness
    // end command line options

    bjou::ArgSet args = {verbose_arg.getValue(),
                         front_arg.getValue(),
                         time_arg.getValue(),
                         symbols_arg.getValue(),
                         noparallel_arg.getValue(),
                         opt_arg.getValue(),
                         noabc_arg.getValue(),
                         module_arg.getValue(),
                         nopreload_arg.getValue(),
                         lld_arg.getValue(),
                         c_arg.getValue(),
                         emitllvm_arg.getValue(),
                         module_search_path_arg.getValue(),
                         output_arg.getValue(),
                         target_triple_arg.getValue(),
                         march_arg.getValue(),
                         link_arg.getValue(),
                         files.getValue()};

    /* args.print(); */

    StartDefaultCompilation(args);

    return 0;
}
