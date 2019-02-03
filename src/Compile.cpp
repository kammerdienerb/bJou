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

#ifdef BJOU_INSTALL_PREFIX
    #define BJOU_DEFAULT_SEARCH BJOU_INSTALL_PREFIX "/lib/bjou/modules/"
#else
    #define BJOU_DEFAULT_SEARCH "/usr/local/lib/bjou/modules/"
#endif

namespace bjou {
Compilation::Compilation(FrontEnd & _frontEnd, BackEnd & _backEnd,
                         ArgSet & _args)
    : max_interface_procs(0), frontEnd(_frontEnd), backEnd(_backEnd),
      args(_args) {
    if (args.files.empty()) {
        Context e_context;
        _error(e_context, "No input files!");
        abort();
    }

    if (args.noabc_arg)
        frontEnd.abc = false;

    if (args.output_arg.size()) {
        std::string o = de_quote(args.output_arg);
        outputpath = o.find('/') == std::string::npos
                         ? ""
                         : o.substr(0, o.find_last_of('/')) + "/";
        outputbasefilename = o.substr(0, o.find_last_of('.'));
        outputbasefilename =
            outputbasefilename.substr(outputbasefilename.find_last_of("/") + 1);
    } else {
        std::string firstfile = args.files[0];
        firstfile = de_quote(firstfile);
        outputpath = "";
        outputbasefilename = firstfile.substr(0, firstfile.find_last_of('.'));
        outputbasefilename =
            outputbasefilename.substr(outputbasefilename.find_last_of("/") + 1);
    }

    for (auto & f : args.files) {
        if (has_suffix(f, ".o") || has_suffix(f, ".a") ||
            has_suffix(f, ".so") || has_suffix(f, ".dylib"))
            obj_files.push_back(f);

        else if (!has_suffix(f, ".bjou")) {
            warning("Ignoring file '" + f + "'.");
        }
    }

    module_search_paths.push_back("");
    for (auto & _path : args.module_search_path_arg) {
        std::string path = _path;
        path = de_quote(path);
        if (_path.back() != '/')
            path += "/";
        module_search_paths.push_back(path);
    }

    module_search_paths.push_back("modules/");

#ifndef _WIN32
    module_search_paths.push_back(BJOU_DEFAULT_SEARCH);
#endif
}

void Compilation::go() {
    auto start = Clock::now();

    backEnd.init();

    auto f_time = frontEnd.go();
    if (args.time_arg)
        prettyPrintTimeMaj(f_time, "Front-end");

    if (!args.front_arg && mode != MODULE) {
        auto b_time = backEnd.go();
        if (args.time_arg)
            prettyPrintTimeMaj(b_time, "Back-end");
    }

    auto end = Clock::now();
    auto compile_time = duration_cast<milliseconds>(end - start);
    if (args.time_arg)
        prettyPrintTimeMaj(compile_time, "Grand total");
    if (args.time_arg) {
        bjouSetColor(LIGHTCYAN);
        float s = RunTimeToSeconds(compile_time);
        float per_s = ((float)frontEnd.n_lines) / s;
        printf("*** %u lines @ %g lines/s\n", frontEnd.n_lines, per_s);
        bjouResetColor();
        fflush(stdout);
    }
}

Compilation::~Compilation() {}

void Compilation::abort(int exitCode) {
    // @bad form.. we should not be using exit since
    // we probably have multiple threads running..

    // this->~Compilation();
    exit(exitCode);
}

double RunTimeToSeconds(milliseconds time) {
    return (double)time.count() / (double)1000.0;
}
} // namespace bjou
