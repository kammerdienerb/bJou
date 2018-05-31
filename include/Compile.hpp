//
//  Compile.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Compile_hpp
#define Compile_hpp

#include <chrono>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::time_point;

namespace bjou {
struct FrontEnd;
struct BackEnd;
struct ArgSet;

struct Type;

struct Compilation {
    Compilation(FrontEnd & _frontEnd, BackEnd & _backEnd, ArgSet & _args);
    ~Compilation();

    enum Mode { NORMAL, MODULE, CT_EXEC };

    Mode mode;
    std::string outputbasefilename;
    std::string outputpath;
    std::vector<std::string> obj_files;
    std::vector<std::string> module_search_paths;

    unsigned int max_interface_procs;

    FrontEnd & frontEnd;
    BackEnd & backEnd;

    ArgSet & args;

    void go();
    void abort(int exitCode = 1);
};

void StartDefaultCompilation(ArgSet & args);

double RunTimeToSeconds(milliseconds time);
} // namespace bjou

#endif /* Compile_hpp */
