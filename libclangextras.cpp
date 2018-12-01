// libclangextras.cpp
// shamelessly stolen from https://github.com/pybee/sealang



#include "clang/AST/OperationKinds.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Type.h"

namespace clang {
  class ASTUnit;
  class CIndexer;
namespace cxstring {
  class CXStringPool;
} // namespace cxstring
namespace index {
class CommentToXMLConverter;
} // namespace index
} // namespace clang

struct CXTranslationUnitImpl {
  clang::CIndexer *CIdx;
  clang::ASTUnit *TheASTUnit;
  clang::cxstring::CXStringPool *StringPool;
  void *Diagnostics;
  void *OverridenCursorsPool;
  clang::index::CommentToXMLConverter *CommentToXML;
  unsigned ParsingOptions;
  std::vector<std::string> Arguments;
};

#include "clang-c/CXString.h"
#include "clang-c/Index.h"

static CXTypeKind GetBuiltinTypeKind(const clang::BuiltinType *BT) {
#define BTCASE(K) case clang::BuiltinType::K: return CXType_##K
  switch (BT->getKind()) {
    BTCASE(Void);
    BTCASE(Bool);
    BTCASE(Char_U);
    BTCASE(UChar);
    BTCASE(Char16);
    BTCASE(Char32);
    BTCASE(UShort);
    BTCASE(UInt);
    BTCASE(ULong);
    BTCASE(ULongLong);
    BTCASE(UInt128);
    BTCASE(Char_S);
    BTCASE(SChar);
    case clang::BuiltinType::WChar_S: return CXType_WChar;
    case clang::BuiltinType::WChar_U: return CXType_WChar;
    BTCASE(Short);
    BTCASE(Int);
    BTCASE(Long);
    BTCASE(LongLong);
    BTCASE(Int128);
    BTCASE(Half);
    BTCASE(Float);
    BTCASE(Double);
    BTCASE(LongDouble);
    BTCASE(Float16);
    BTCASE(Float128);
    BTCASE(NullPtr);
    BTCASE(Overload);
    BTCASE(Dependent);
    BTCASE(OCLSampler);
    BTCASE(OCLEvent);
    BTCASE(OCLQueue);
    BTCASE(OCLReserveID);
  default:
    return CXType_Unexposed;
  }
#undef BTCASE
}

static CXTypeKind GetTypeKind(clang::QualType T) {
  const clang::Type *TP = T.getTypePtrOrNull();
  if (!TP)
    return CXType_Invalid;

#define TKCASE(K) case clang::Type::K: return CXType_##K
  switch (TP->getTypeClass()) {
      case clang::Type::Builtin:
          return GetBuiltinTypeKind(clang::cast<clang::BuiltinType>(TP));
    TKCASE(Complex);
    TKCASE(Pointer);
    TKCASE(BlockPointer);
    TKCASE(LValueReference);
    TKCASE(RValueReference);
    TKCASE(Record);
    TKCASE(Enum);
    TKCASE(Typedef);
    TKCASE(FunctionNoProto);
    TKCASE(FunctionProto);
    TKCASE(ConstantArray);
    TKCASE(IncompleteArray);
    TKCASE(VariableArray);
    TKCASE(DependentSizedArray);
    TKCASE(Vector);
    TKCASE(MemberPointer);
    TKCASE(Auto);
    TKCASE(Elaborated);
    TKCASE(Pipe);
    default:
      return CXType_Unexposed;
  }
#undef TKCASE
}

namespace clang{
namespace cxtu {
static inline ASTUnit *getASTUnit(CXTranslationUnit TU) {
    if (!TU)
        return nullptr;
    return TU->TheASTUnit;
}
} // namespace cxtu
} // namespace clang

#include "llvm/ADT/SmallString.h"

static clang::Expr* CursorGetExpr(CXCursor cursor) {
    return clang::dyn_cast_or_null<clang::Expr>((clang::Stmt*)(cursor.data[1]));
}

static CXType MakeCXType(clang::QualType T, CXTranslationUnit TU) {
    using namespace clang;

    CXTypeKind TK = CXType_Invalid;

    if (TU && !T.isNull()) {
        // Handle paren types as the original type
        if (auto *PTT = T->getAs<ParenType>()) {
            return MakeCXType(PTT->getInnerType(), TU);
        }

        /* Handle decayed types as the original type */
        if (const DecayedType *DT = T->getAs<DecayedType>()) {
            return MakeCXType(DT->getOriginalType(), TU);
        }
    }
    if (TK == CXType_Invalid)
        TK = GetTypeKind(T);

    CXType CT = { TK, { TK == CXType_Invalid ? nullptr
        : T.getAsOpaquePtr(), TU } };
    return CT;
}

extern "C" bool clangextras_typeIsUnion(CXType cxt) {
    clang::Type * t = (clang::Type*)cxt.data[0];
    clang::RecordType * r_t = clang::dyn_cast<clang::RecordType>(t);
    if (!r_t)
        return false;
    return r_t->isUnionType();
}

extern "C" CXType clangextras_atomicGetValueType(CXType a_cxt, CXTranslationUnit tu) {
    clang::Type * t = (clang::Type*)a_cxt.data[0];
    clang::AtomicType * a_t = clang::dyn_cast_or_null<clang::AtomicType>(t);
    clang::QualType v_t = a_t->getValueType(); 

    return MakeCXType(v_t, tu);
}

extern "C" bool clangextras_isFunctionInline(CXCursor cursor) {
    clang::FunctionDecl * fn = clang::dyn_cast_or_null<clang::FunctionDecl>((clang::Decl*)cursor.data[0]);
    return fn && fn->isInlined();
}

extern "C" bool clangextras_isVarInitialized(CXCursor cursor) {
    clang::VarDecl * var = clang::dyn_cast_or_null<clang::VarDecl>((clang::Decl*)cursor.data[0]);
    return var && var->getInit() != nullptr;
}

extern "C" char * clangextras_getOperatorString(CXCursor cursor) {
    if (cursor.kind == CXCursor_BinaryOperator) {
        clang::BinaryOperator *op = (clang::BinaryOperator *) CursorGetExpr(cursor);
        return strdup(clang::BinaryOperator::getOpcodeStr(op->getOpcode()).str().c_str());
    }

    if (cursor.kind == CXCursor_CompoundAssignOperator) {
        clang::CompoundAssignOperator *op = (clang::CompoundAssignOperator*) CursorGetExpr(cursor);
        return strdup(clang::BinaryOperator::getOpcodeStr(op->getOpcode()).str().c_str());
    }

    if (cursor.kind == CXCursor_UnaryOperator) {
        clang::UnaryOperator *op = (clang::UnaryOperator*) CursorGetExpr(cursor);
        return strdup(clang::UnaryOperator::getOpcodeStr(op->getOpcode()).str().c_str());
    }

    return NULL;
}

extern "C" char * clangextras_getLiteralString(CXCursor cursor) {
    if (cursor.kind == CXCursor_IntegerLiteral) {
        clang::IntegerLiteral *intLiteral = (clang::IntegerLiteral *) CursorGetExpr(cursor);
        return strdup(intLiteral->getValue().toString(10, true).c_str());
    }

    if (cursor.kind == CXCursor_FloatingLiteral) {
        clang::FloatingLiteral *floatLiteral = (clang::FloatingLiteral *) CursorGetExpr(cursor);
        llvm::SmallString<1024> str;
        floatLiteral->getValue().toString(str);
        return strdup(str.c_str());
    }

    if (cursor.kind == CXCursor_CharacterLiteral) {
        clang::CharacterLiteral *charLiteral = (clang::CharacterLiteral *) CursorGetExpr(cursor);
        char c[2];
        c[0] = (char) charLiteral->getValue();
        c[1] = '\0';
        return strdup(c);
    }

    if (cursor.kind == CXCursor_StringLiteral) {
        clang::StringLiteral *stringLiteral = (clang::StringLiteral *) CursorGetExpr(cursor);
        return strdup(stringLiteral->getBytes().str().c_str());
    }

    if (cursor.kind == CXCursor_CXXBoolLiteralExpr) {
        clang::CXXBoolLiteralExpr *boolLiteral = (clang::CXXBoolLiteralExpr *) CursorGetExpr(cursor);
        return strdup(boolLiteral->getValue() ? "true" : "false");
    }

    return NULL;
}
