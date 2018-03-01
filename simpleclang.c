#include <clang-c/Index.h>

#define nullptr ((void*)0)

int printf(const char*, ...);

unsigned my_visitor(CXCursor parent, CXCursor cursor, CXClientData client_data) {
    printf("AST node\n");
    return CXChildVisit_Recurse;
}

int main() {
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

    return 0;
}
