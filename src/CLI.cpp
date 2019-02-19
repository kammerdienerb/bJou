//
//  CLI.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/8/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "CLI.hpp"
#include "Context.hpp"
#include "FrontEnd.hpp"
#include "Misc.hpp"
#include "Parser.hpp"
#include "Range.hpp"

#include <fstream>
#include <iterator>

using namespace rlutil;

bJouOutput::bJouOutput() { saveDefaultColor(); }

void bJouOutput::failure(TCLAP::CmdLineInterface & c, TCLAP::ArgException & e) {
    bjouSetColor(GREEN);
    std::cerr << e.what();
    bjouResetColor();
    std::cerr << std::endl;
    exit(1);
}

void bJouOutput::usage(TCLAP::CmdLineInterface & c) {
    std::cout << "Usage: bJou [options] input...\n\n";
    std::cout << "Options:\n";
    bjouSetColor(GREEN);
    std::cout << std::endl;
    auto args = c.getArgList();
    int maxlen = 0;
    for (auto it = args.begin(); it != args.end(); it++)
        if ((int)(*it)->longID().size() > maxlen)
            maxlen = (int)(*it)->longID().size();
    for (auto it = args.begin(); it != args.end(); it++) {
        int IDlen = (int)(*it)->longID().size();
        int Desclen = (int)(*it)->getDescription().size();

        if (IDlen + Desclen < (maxlen - 1)) {
            bjouSetColor(GREEN);
            printf("%s", (*it)->longID().c_str());
            bjouResetColor();
            std::string dots((maxlen - IDlen - Desclen) - 2, '.');
            printf(" %s %s\n\n", dots.c_str(), (*it)->getDescription().c_str());
        } else {
            bjouSetColor(GREEN);
            std::string dots((maxlen - IDlen), '.');
            printf("%s", (*it)->longID().c_str());
            bjouResetColor();
            printf("%s\n", dots.c_str());
            dots = std::string((maxlen - Desclen) - 1, '.');
            printf("%s %s\n\n", dots.c_str(), (*it)->getDescription().c_str());
        }

        // std::cout << "  (" << (*it)->getDescription() << ")" << std::endl;
    }
}

void bJouOutput::version(TCLAP::CmdLineInterface & c) {
    bjouSetColor(BJOU_VER_COLOR);
    std::cout << "bJou \t" << BJOU_VER_COLOR_STR;
    bjouResetColor();
    std::cout << "\t(ver " << c.getVersion() << ")" << std::endl;
#ifdef BJOU_DEBUG_BUILD
    std::cout << "(debug)\n";
#endif
}

namespace bjou {

using bjou::compilation;

ArgSet::ArgSet(bool _verbose_arg, bool _front_arg, bool _time_arg,
               bool _symbols_arg, bool _noparallel_arg, bool _opt_arg,
               bool _noabc_arg, bool _module_arg, bool _nopreload_arg,
               bool _lld_arg, bool _c_arg, bool _emitllvm_arg,
               const std::vector<std::string> & _module_search_path_arg,
               const std::string & _output_arg, const std::string & _target_triple_arg, const std::string & _march_arg ,const std::string & _mfeat_arg, const std::vector<std::string> & _link_arg,
               const std::vector<std::string> & _files)
    : verbose_arg(_verbose_arg), front_arg(_front_arg), time_arg(_time_arg),
      symbols_arg(_symbols_arg), noparallel_arg(_noparallel_arg),
      opt_arg(_opt_arg), noabc_arg(_noabc_arg), module_arg(_module_arg),
      nopreload_arg(_nopreload_arg), lld_arg(_lld_arg), c_arg(_c_arg),
      emitllvm_arg(_emitllvm_arg),
      module_search_path_arg(_module_search_path_arg), output_arg(_output_arg),
      target_triple_arg(_target_triple_arg), march_arg(_march_arg), mfeat_arg(_mfeat_arg), link_arg(_link_arg), files(_files) { }

void ArgSet::print() {
    printf("verbose             = %s\n", (verbose_arg ? "true" : "false"));
    printf("front               = %s\n", (front_arg ? "true" : "false"));
    printf("time                = %s\n", (time_arg ? "true" : "false"));
    printf("symbols             = %s\n", (symbols_arg ? "true" : "false"));
    printf("noparallel          = %s\n", (noparallel_arg ? "true" : "false"));
    printf("opt                 = %s\n", (opt_arg ? "true" : "false"));
    printf("noabc               = %s\n", (noabc_arg ? "true" : "false"));
    printf("module              = %s\n", (module_arg ? "true" : "false"));
    printf("nopreload           = %s\n", (nopreload_arg ? "true" : "false"));
    printf("lld                 = %s\n", (lld_arg ? "true" : "false"));
    printf("c                   = %s\n", (c_arg ? "true" : "false"));
    printf("emitllvm            = %s\n", (emitllvm_arg ? "true" : "false"));

    const char * comma = ", ";

    printf("module search paths = { ");
    if (module_search_path_arg.empty()) {
        printf(" }\n");
    } else {
        for (auto & path : module_search_path_arg) {
            if (&path == &module_search_path_arg.back())
                comma = " }\n";
            printf("%s%s", path.c_str(), comma);
        }
    }

    printf("output              = %s\n", output_arg.c_str());
    printf("targettriple        = %s\n", target_triple_arg.c_str());
    printf("march               = %s\n", march_arg.c_str());
    printf("mfeat               = %s\n", mfeat_arg.c_str());

    printf("link                = { ");
    if (link_arg.empty()) {
        printf(" }\n");
    } else {
        comma = ", ";
        for (auto & l : link_arg) {
            if (&l == &link_arg.back())
                comma = " }\n";
            printf("%s%s", l.c_str(), comma);
        }
    }

    printf("files               = { ");
    if (files.empty()) {
        printf(" }\n");
    } else {
        comma = ", ";
        for (auto & f : files) {
            if (&f == &files.back())
                comma = " }\n";
            printf("%s%s", f.c_str(), comma);
        }
    }
}

void prettyPrintTimeMaj(milliseconds ms, std::string label) {
    bjouSetColor(GREEN);
    float s = RunTimeToSeconds(ms);
    if (s > 0)
        printf("*** %s: %gs\n", label.c_str(), s);
    else
        printf("*** %s: %lldms\n", label.c_str(), ms.count());
    bjouResetColor();
    fflush(stdout);
}
void prettyPrintTimeMin(milliseconds ms, std::string label) {
    bjouSetColor(YELLOW);
    float s = RunTimeToSeconds(ms);
    if (s > 0)
        printf("*** %s: %gs\n", label.c_str(), s);
    else
        printf("*** %s: %lldms\n", label.c_str(), ms.count());
    bjouResetColor();
    fflush(stdout);
}

void internalError(std::string message) {
    std::lock_guard<std::mutex> lock(cli_mtx);
    std::cout << "bJou :: ";
    bjouSetColor(RED);
    std::cout << "INTERNAL ERROR: " << message;
    bjouResetColor();
    std::cout << "\n";

    compilation->abort();
}

void _more(std::string message) {
    std::cout << "        *** ";
    bjouSetColor(GREEN);
    std::cout << message;
    bjouResetColor();
    std::cout << "\n";
}

std::string linenobuf(int ln, bool mark) {
    static const char * side = "\xE2\x95\x91";
    char buf[256];
    if (mark) {
        char b[10];
        sprintf(b, "=> %d", ln);
        sprintf(buf, "%10s%s ", b, side);
    } else
        sprintf(buf, "%10d%s ", ln, side);
    return buf;
}

void _here(Context & context) {
    std::ifstream file(context.filename);
    if (file) {
        std::vector<std::string> lines;
        for (std::string line; std::getline(file, line);)
            lines.push_back(line);
        if (lines.size() == 0)
            return;

        bjouSetColor(RED);
        std::cout << "     Here : \n";
        bjouResetColor();

        if (context.begin.line < 3) {
            for (int i = 0; i < context.begin.line - 1; i += 1) {
                // std::cout << "            " << lines[i] << "\n";
                std::cout << linenobuf(i + 1) << lines[i] << "\n";
            }
        } else {
            // std::cout << "            " << lines[context.begin.line - 3] <<
            // "\n";  std::cout << "            " << lines[context.begin.line -
            // 2]
            // << "\n";
            std::cout << linenobuf(context.begin.line - 2)
                      << lines[context.begin.line - 3] << "\n";
            std::cout << linenobuf(context.begin.line - 1)
                      << lines[context.begin.line - 2] << "\n";
        }

        if (context.begin.line == context.end.line) {
            // std::cout << "            ";
            bjouSetColor(RED);
            std::cout << linenobuf(context.begin.line, true);
            bjouResetColor();
            std::string line = lines[context.begin.line - 1];
            std::cout << line.substr(0, context.begin.character - 1);
            bjouSetBackgroundColor(RED);
            std::cout << line.substr(context.begin.character - 1,
                                     context.end.character -
                                         context.begin.character);
            bjouResetColor();
            std::cout << line.substr(context.end.character - 1);
            std::cout << "\n";
        } else {
            bjouSetColor(RED);
            std::cout << linenobuf(context.begin.line, true);
            bjouResetColor();

            auto _lines = bjou::Range<std::string>(
                &lines[context.begin.line - 1], &lines[context.end.line - 1]);
            std::cout << _lines[0].substr(0, context.begin.character - 1);
            bjouSetBackgroundColor(RED);
            std::cout << _lines[0].substr(context.begin.character - 1);
            std::cout << "\n";

            std::string last = *(_lines.end() - 1);
            _lines =
                bjou::Range<std::string>(_lines.begin() + 1, _lines.end() - 2);

            int ln = context.begin.line + 1;
            for (auto & line : _lines) {
                bjouResetColor();
                bjouSetColor(RED);
                std::cout << linenobuf(ln, true);
                bjouResetColor();
                bjouSetBackgroundColor(RED);
                std::cout << line << "\n";
                ln += 1;
            }

            bjouResetColor();
            bjouSetColor(RED);
            std::cout << linenobuf(context.end.line, true);
            bjouResetColor();
            bjouSetBackgroundColor(RED);
            std::cout << last.substr(0, context.end.character - 1);
            bjouResetColor();
            std::cout << last.substr(context.end.character - 1,
                                     last.size() - 1);
            std::cout << "\n";
        }

        if (lines.size() - context.end.line < 3) {
            for (int i = 0; i < lines.size() - context.end.line; i += 1) {
                std::cout << linenobuf(context.end.line + i + 1)
                          << lines[context.end.line + i] << "\n";
            }
        } else {
            // std::cout << "            " << lines[context.end.line + 1] <<
            // "\n";  std::cout << "            " << lines[context.end.line + 2]
            // << "\n";
            std::cout << linenobuf(context.end.line + 1)
                      << lines[context.end.line] << "\n";
            std::cout << linenobuf(context.end.line + 2)
                      << lines[context.end.line + 1] << "\n";
        }
    }
}

void error(std::string message, bool exit) {
    std::lock_guard<std::mutex> lock(cli_mtx);
    _error(message);
    if (exit)
        compilation->abort();
}

void error(std::string message, std::vector<std::string> continuations,
           bool exit) {
    std::lock_guard<std::mutex> lock(cli_mtx);
    _error(message);
    for (std::string & c : continuations)
        _more(c);
    if (exit)
        compilation->abort();
}

void error(Context context, std::string message, bool exit) {
    std::lock_guard<std::mutex> lock(cli_mtx);
    _error(context, message);
    if (exit)
        compilation->abort();
}

void error(Context context, std::string message, bool exit,
           std::vector<std::string> continuations) {
    std::lock_guard<std::mutex> lock(cli_mtx);
    _error(context, message);
    for (std::string & c : continuations)
        _more(c);
    if (exit)
        compilation->abort();
}

void errorl(Context context, std::string message, bool exit) {
    std::lock_guard<std::mutex> lock(cli_mtx);
    _error(context, message);
    _here(context);
    if (exit)
        compilation->abort();
}

void errorl(Context context, std::string message, bool exit,
            std::vector<std::string> continuations) {
    std::lock_guard<std::mutex> lock(cli_mtx);
    _error(context, message);
    _here(context);
    for (std::string & c : continuations)
        _more(c);
    if (exit)
        compilation->abort();
}

Context errornextGetContext(Parser & parser) {
    Context e_context;
    e_context.start(&parser.currentContext);
    parser.optional(bjou::GENERIC_TOKEN, false, true);
    e_context.finish(&parser.currentContext, &parser.justCleanedContext);
    return e_context;
}

void errornext(Parser & parser, std::string message, bool exit) {
    Context e_context = errornextGetContext(parser);
    errorl(e_context, message, exit);
}

void errornext(Parser & parser, std::string message, bool exit,
               std::vector<std::string> continuations) {
    Context e_context = errornextGetContext(parser);
    errorl(e_context, message, exit, continuations);
}

void warning(std::string message) {
    std::lock_guard<std::mutex> lock(cli_mtx);
    _warning(message);
}

void warning(std::string message, std::vector<std::string> continuations) {
    std::lock_guard<std::mutex> lock(cli_mtx);
    _warning(message);
    for (std::string & c : continuations)
        _more(c);
}

void warning(Context context, std::string message) {
    std::lock_guard<std::mutex> lock(cli_mtx);
    _warning(context, message);
}

void warning(Context context, std::string message,
             std::vector<std::string> continuations) {
    std::lock_guard<std::mutex> lock(cli_mtx);
    _error(context, message);
    for (std::string & c : continuations)
        _more(c);
}

void warningl(Context context, std::string message) {
    std::lock_guard<std::mutex> lock(cli_mtx);
    _warning(context, message);
    _here(context);
}

void warningl(Context context, std::string message,
              std::vector<std::string> continuations) {
    std::lock_guard<std::mutex> lock(cli_mtx);
    _warning(context, message);
    _here(context);
    for (std::string & c : continuations)
        _more(c);
}

} // namespace bjou
