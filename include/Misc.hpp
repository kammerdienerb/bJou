//
//  Misc.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/8/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Misc_h
#define Misc_h

#include "Context.hpp"

#include <algorithm>
#include <assert.h>
#include <cstring>
#include <future>

#define STR(x) _STR(x)
#define _STR(x) #x

#define CAT2(x, y) _CAT2(x, y)
#define _CAT2(x, y) x##y

#define CAT3(x, y, z) _CAT3(x, y, z)
#define _CAT3(x, y, z) x##y##z

#define CAT4(a, b, c, d) _CAT4(a, b, c, d)
#define _CAT4(a, b, c, d) a##b##c##d

#define B_MAX(a, b) ((a) > (b) ? (a) : (b))

char * calculateSize(size_t size);

template <size_t N> bool s_in_a(const char * s, const char * (&a)[N]) {
    for (auto elem : a)
        if (std::strcmp(elem, s) == 0)
            return true;
    return false;
}

namespace bjou {
template <typename F, typename R> inline std::future<R> runasync(F & f) {
    return std::async(std::launch::async, f);
}

template <typename F, typename... Ts, typename R>
inline std::future<R> runasync(F & f, Ts &... params) {
    return std::async(std::launch::async, std::forward<F>(f),
                      std::forward<Ts>(params)...);
}
} // namespace bjou

std::string de_quote(std::string & str);
bool has_suffix(const std::string & s, std::string suffix);
std::string str_escape(std::string & str);
char get_ch_value(std::string & str);

#ifdef BJOU_DEBUG_BUILD
#include <string>
namespace bjou {
void internalError(std::string message);
void errorl(Context context, std::string message, bool exit);
} // namespace bjou
inline void _BJOU_TRIGGER_DEBUG_ASSERT(int line, const char * file) {
    bjou::Context context;
    context.filename = file;
    context.begin.line = context.end.line = line;
    context.begin.character = context.end.character = 1;
    bjou::errorl(context,
                 "assertion failed on line " + std::to_string(line) + " of " +
                     std::string(file),
                 false);
    bjou::internalError("exiting due to a failed assertion");
}
#define BJOU_DEBUG_ASSERT(expr)                                                \
    if (!(expr))                                                               \
    _BJOU_TRIGGER_DEBUG_ASSERT(__LINE__, __FILE__)
#else
#define BJOU_DEBUG_ASSERT(expr) ;
#endif

// I really hate the C preprocessor.. let's do something about that.
#define EVAL(x) x
#define _STR(x) #x
#define STR(x) _STR(x)
#define _CAT3(x, y, z) x##y##z
#define CAT3(x, y, z) _CAT3(x, y, z)

#define BJOU_VER_MAJ 0
#define BJOU_VER_MIN 7
#define BJOU_VER CAT3(BJOU_VER_MAJ, ., BJOU_VER_MIN)
#define BJOU_VER_STR STR(BJOU_VER)

#define BJOU_VER_COLOR RED

#define _BJOU_VER_COLOR_STR STR(BJOU_VER_COLOR)
#define BJOU_VER_COLOR_STR _BJOU_VER_COLOR_STR

#endif /* Misc_h */
