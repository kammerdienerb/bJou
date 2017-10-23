//
//  Context.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/4/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "Context.hpp"

namespace bjou {
Context::Context() {
    filename = "<?>";
    begin = {0, 0};
    end = {0, 0};
}

Context::Context(std::string _filename, Loc _begin) {
    filename = _filename;
    begin = _begin;
}

void Context::start(Context * context) {
    filename = context->filename;
    begin = context->end;
}

void Context::finish(Context * context, Context * wtspccontext) {
    end = diff(context, wtspccontext).end;
}

Context Context::lastchar() {
    Context c = *this;
    c.begin = c.end;
    if (c.begin.character != 1)
        c.begin.character -= 1;
    return c;
}

Context diff(Context * c1, Context * c2) {
    // Assumes c2 is contained within c1
    if (!c2)
        return *c1;
    if (c2->begin.line == c1->end.line) {
        if (c2->begin.character < c1->begin.character)
            return *c1;
    } else if (c2->begin.line < c1->begin.line)
        return *c1;

    Context result = *c1;
    result.end = c2->begin;
    return result;
}
} // namespace bjou
