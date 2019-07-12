//
//  Type.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/8/18.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Type_hpp
#define Type_hpp

#include "Maybe.hpp"
#include "Misc.hpp"
#include "hash_table.h"

#include <set>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace bjou {
struct ASTNode;
struct Procedure;
struct Constant;
struct VariableDeclaration;
struct Struct;
struct Enum;
struct Alias;
struct Expression;
struct IntegerLiteral;
struct Identifier;
struct Declarator;
struct TemplateInstantiation;
struct Symbol;
struct Scope;
struct ProcSet;

struct Type;

struct CycleDetectEdge {
    const Type *from, *to;
    Declarator * decl;

    CycleDetectEdge(const Type *_from, const Type *_to, Declarator *_decl)
        : from(_from), to(_to), decl(_decl) { }
};

struct Type {
    enum Kind {
        PLACEHOLDER   = 0,
        VOID          = 1,
        BOOL          = 2,
        INT           = 3,
        FLOAT         = 4,
        CHAR          = 5,
        NONE          = 6,
        POINTER       = 7,
        REF           = 8,
        ARRAY         = 9,
        SLICE         = 10,
        DYNAMIC_ARRAY = 11,
        STRUCT        = 12,
        ENUM          = 13,
        SUM           = 14,
        TUPLE         = 15,
        PROCEDURE     = 16
    };

    enum Sign { UNSIGNED = 0, SIGNED = 1 };

    Kind kind;
    std::string key;

    Type(Kind _kind, const std::string _key);

    static void alias(std::string name, const Type * t);

    bool isPlaceholder() const;
    bool isVoid() const;
    bool isBool() const;
    bool isInt() const;
    bool isFloat() const;
    bool isChar() const;
    bool isNone() const;
    bool isPointer() const;
    bool isRef() const;
    bool isMaybe() const;
    bool isArray() const;
    bool isSlice() const;
    bool isDynamicArray() const;
    bool isStruct() const;
    bool isEnum() const;
    bool isSum() const;
    bool isTuple() const;
    bool isProcedure() const;

    bool isPrimative() const;

    const Type * getPointer() const;
    const Type * getRef() const;
    const Type * getMaybe() const;

    const Type * unRef() const;

    const Type * getBase() const;

    virtual const Type * under() const;

    virtual Declarator * getGenericDeclarator() const = 0;
    virtual std::string getDemangledName() const;
    virtual const Type * replacePlaceholders(const Type * t) const;
    virtual void checkForCycles() const;
    virtual bool _checkForCycles(std::set<const Type*>& visited, std::vector<CycleDetectEdge>& path) const;
};

struct PlaceholderType : Type {
    static const std::string plkey;

    PlaceholderType();

    static const Type * get();

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;

    const Type * replacePlaceholders(const Type * t) const;
};

struct VoidType : Type {
    static const std::string vkey;

    VoidType();

    static const Type * get();

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;
};

struct BoolType : Type {
    static const std::string bkey;

    BoolType();

    static const Type * get();

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;
};

struct IntType : Type {
    Sign sign;
    int width;

    IntType(Sign _sign, int _width);

    static const Type * get(Sign sign, int width);

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;
};

struct FloatType : Type {
    int width;

    FloatType(int _width);

    static const Type * get(int width);

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;
};

struct CharType : Type {
    static const std::string ckey;

    CharType();

    static const Type * get();

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;
};

struct NoneType : Type {
    static const std::string nkey;

    NoneType();

    static const Type * get();

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;
};

struct PointerType : Type {
    const Type * elem_t;

    PointerType(const Type * _elem_t);

    static const Type * get(const Type * elem_t);

    const Type * under() const;

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;

    const Type * replacePlaceholders(const Type * t) const;
};

struct RefType : Type {
    const Type * t;

    RefType(const Type * _t);

    static const Type * get(const Type * t);

    const Type * under() const;

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;

    const Type * replacePlaceholders(const Type * t) const;
};

struct ArrayType : Type {
    const Type * elem_t;
    int width;

    ArrayType(const Type * _elem_t, int _width);

    static const Type * get(const Type * elem_t, int width);

    const Type * under() const;

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;

    const Type * replacePlaceholders(const Type * t) const;
};

struct SliceType : Type {
    const Type * elem_t;
    
    const Type * real_t;

    SliceType(const Type * _elem_t);

    static const Type * get(const Type * elem_t);

    const Type * getRealType() const;

    const Type * under() const;

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;

    const Type * replacePlaceholders(const Type * t) const;
};

struct DynamicArrayType : Type {
    const Type * elem_t;

    const Type * real_t;

    DynamicArrayType(const Type * _elem_t);

    static const Type * get(const Type * elem_t);

    const Type * getRealType() const;

    const Type * under() const;

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;

    const Type * replacePlaceholders(const Type * t) const;
};

struct StructType : Type {
    bool isAbstract;
    bool isComplete;
    Struct * _struct;
    TemplateInstantiation * inst;
    Type * extends;
    hash_table_t<std::string, int, STRING_HASHER> memberIndices;
    std::vector<const Type *> memberTypes;
    std::vector<VariableDeclaration*> memberVarDecls;
    hash_table_t<std::string, Constant *, STRING_HASHER> constantMap;
    hash_table_t<std::string, ProcSet *, STRING_HASHER> memberProcs;
    hash_table_t<std::string, const StructType*, STRING_HASHER> inheritedProcsToBaseStructType;

    StructType(std::string & name, Struct * __struct = nullptr,
               TemplateInstantiation * _inst = nullptr);

    static const Type * get(std::string name);
    static const Type * get(std::string name, Struct * _struct,
                            TemplateInstantiation * inst = nullptr);

    Declarator * getGenericDeclarator() const;

    void complete();

    bool containsRefs() const;
    
    bool _checkForCycles(std::set<const Type*>& visited, std::vector<CycleDetectEdge>& path) const;
};

struct EnumType : Type {
    Enum * _enum;
    std::vector<std::string> identifiers;

    EnumType(std::string & name, Enum * _enum = nullptr);

    static const Type * get(std::string name);
    static const Type * get(std::string name, Enum * __enum);

    Declarator * getGenericDeclarator() const;

    IntegerLiteral * getValueLiteral(std::string& identifier, Context & context, Scope * scope) const;
};

struct SumType : Type {
    Declarator * first_decl;

    std::vector<const Type *> types;

    std::vector<const Type*> flatten_sub_types() const;
    const Type * oro_ref_type() const;

    unsigned n_eq_bits;

    SumType(const std::vector<const Type *> & _types, Declarator * _first_decl);

    static const Type * get(const std::vector<const Type *> & types, Declarator * _first_decl);

    const std::vector<const Type *> & getTypes() const;

    Declarator * getGenericDeclarator() const;

    const Type * replacePlaceholders(const Type * t) const;
    
    bool _checkForCycles(std::set<const Type*>& visited, std::vector<CycleDetectEdge>& path) const;
};

uint32_t get_static_sum_tag_store(const Type * dest_t, const SumType * sum_t);
uint32_t get_static_sum_tag_cmp(const Type * dest_t, const SumType * sum_t, bool *by_ext);

struct TupleType : Type {
    Declarator * first_decl;

    std::vector<const Type *> types;

    TupleType(const std::vector<const Type *> & _types, Declarator * _first_decl);

    static const Type * get(const std::vector<const Type *> & types, Declarator * _first_decl);

    const std::vector<const Type *> & getTypes() const;

    Declarator * getGenericDeclarator() const;

    const Type * replacePlaceholders(const Type * t) const;
    
    bool _checkForCycles(std::set<const Type*>& visited, std::vector<CycleDetectEdge>& path) const;
};

struct ProcedureType : Type {
    std::vector<const Type *> paramTypes;
    bool isVararg;
    const Type * retType;

    ProcedureType(const std::vector<const Type *> & _paramTypes,
                  const Type * _retType, bool _isVararg = false);

    static const Type * get(const std::vector<const Type *> & paramTypes,
                            const Type * retType, bool _isVararg = false);

    const std::vector<const Type *> & getParamTypes() const;
    const Type * getRetType() const;

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;

    const Type * replacePlaceholders(const Type * t) const;
};

bool equal(const Type * t1, const Type * t2);
const Type * conv(const Type * t1, const Type * t2);
bool argMatch(const Type * t1, const Type * t2, bool exact = false);

const Type * getTypeFromTable(const std::string & key);
void addTypeToTable(const Type * t, const std::string & key);

template <typename T, typename... argsT>
const Type * getOrAddType(const std::string & key, argsT... args) {
    const Type * t = getTypeFromTable(key);

    if (!t) {
        t = new T(args...);
        addTypeToTable(t, key);
    }

    return t;
}

std::vector<const Type *>
typesSortedByDepencencies(std::vector<const Type *> nonPrimatives);

void compilationAddPrimativeTypes();

unsigned int simpleSizer(const Type * t);

int countConversions(ProcedureType * compare_type,
                     ProcedureType * candidate_type);

} // namespace bjou
#endif /* Type_hpp */
