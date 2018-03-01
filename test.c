#include <clang-c/Index.h>

#define nullptr ((void*)0)

typedef struct {
    long long a, b, c, d, e, f;
    void * g, *h, *i, *j, *l, *m, *n, *p;
} S;

S p(S arg) {
    return arg;
}

int printf(const char*, ...);

unsigned my_visitor(CXCursor parent, CXCursor cursor, CXClientData client_data) {
    printf("AST node\n");
    return CXChildVisit_Recurse;
}

int main(int argc, char ** argv) {
    S s;
    s = p(s);

    CXIndex index = clang_createIndex(0, 0);
  
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index,
        "c.c", nullptr, 0,
        nullptr, 0,
        CXTranslationUnit_None);

    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    printf("{ %u, %d, %p }\n", cursor.kind, cursor.xdata, cursor.data);

    clang_visitChildren(
        cursor,
        my_visitor,
        nullptr);
}
