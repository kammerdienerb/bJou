//
//  Compile.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "Compile.hpp"
#include "BackEnd.hpp"
#include "CLI.hpp"
#include "FrontEnd.hpp"
#include "Parser.hpp"
#include "StringViewableBuffer.hpp"

#include <mutex>
#include <stdlib.h>

#ifdef BJOU_DEBUG_BUILD
#define SAVE_BJOU_DEBUG_BUILD
#endif
#undef BJOU_DEBUG_BUILD
#include "tclap/CmdLine.h"
#ifdef SAVE_BJOU_DEBUG_BUILD
#define BJOU_DEBUG_BUILD
#endif

namespace bjou {
Compilation::Compilation(FrontEnd & _frontEnd, BackEnd & _backEnd,
                         TCLAPArgSet & _args)
    : max_interface_procs(0), frontEnd(_frontEnd), backEnd(_backEnd),
      args(_args) {
    if (args.files.getValue().empty()) {
        Context e_context;
        _error(e_context, "No input files!");
        abort();
    }

    if (args.output_arg.getValue().size()) {
        std::string o = de_quote(args.output_arg.getValue());
        outputpath = o.find('/') == std::string::npos
                         ? ""
                         : o.substr(0, o.find_last_of('/')) + "/";
        outputbasefilename = o.substr(0, o.find_last_of('.'));
        outputbasefilename =
            outputbasefilename.substr(outputbasefilename.find_last_of("/") + 1);
    } else {
        std::string firstfile = args.files.getValue()[0];
        firstfile = de_quote(firstfile);
        outputpath = "";
        outputbasefilename = firstfile.substr(0, firstfile.find_last_of('.'));
        outputbasefilename =
            outputbasefilename.substr(outputbasefilename.find_last_of("/") + 1);
    }

    module_search_paths.push_back("");
    for (auto & _path : args.module_search_path_arg.getValue()) {
        std::string path = _path;
        if (_path.back() != '/')
            path += "/";
        module_search_paths.push_back(path);
    }

    module_search_paths.push_back("modules/");

#ifndef _WIN32
    module_search_paths.push_back("modules/");
    module_search_paths.push_back("/usr/local/lib/bjou/modules/");
#endif
}

void Compilation::go() {
    auto start = Clock::now();

    auto f_time = frontEnd.go();
    if (args.time_arg.getValue())
        prettyPrintTimeMaj(f_time, "Front-end");

    if (!args.justcheck_arg.getValue() && mode != MODULE) {
        auto b_time = backEnd.go();
        if (args.time_arg.getValue())
            prettyPrintTimeMaj(b_time, "Back-end");
    }

    auto end = Clock::now();
    auto compile_time = duration_cast<milliseconds>(end - start);
    if (args.time_arg.getValue())
        prettyPrintTimeMaj(compile_time, "Grand total");
}

Compilation::~Compilation() {}

void Compilation::abort(int exitCode) {
    this->~Compilation();
    exit(exitCode);
}

double RunTimeToSeconds(milliseconds time) {
    return (double)time.count() / (double)1000.0;
}
} // namespace bjou
