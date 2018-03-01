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

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace bjou {
struct ASTNode;
struct Procedure;
struct Constant;
struct Struct;
struct Alias;
struct Expression;
struct Identifier;
struct Declarator;
struct TemplateInstantiation;
struct Symbol;
struct ProcSet;

struct Type {
    enum Kind {
        PLACEHOLDER,
        VOID,
        BOOL,
        INT,
        FLOAT,
        CHAR,
        POINTER,
        REF,
        ARRAY,
        SLICE,
        DYNAMIC_ARRAY,
        STRUCT,
        TUPLE,
        PROCEDURE
    };

    enum Sign { UNSIGNED = 0, SIGNED = 1 };

    Kind kind;
    std::string key;

    Type(Kind _kind, const std::string _key);

    bool isPlaceholder()  const;
    bool isVoid()         const;
    bool isBool()         const;
    bool isInt()          const;
    bool isFloat()        const;
    bool isChar()         const;
    bool isPointer()      const;
    bool isRef()          const;
    bool isMaybe()        const;
    bool isArray()        const;
    bool isSlice()        const;
    bool isDynamicArray() const;
    bool isStruct()       const;
    bool isTuple()        const;
    bool isProcedure()    const;

    bool isPrimative()    const;

    const Type * getPointer() const;
    const Type * getRef() const;
    const Type * getMaybe() const;

    const Type * unRef() const;

    const Type * getBase() const;

    virtual const Type * under() const;

    virtual Declarator * getGenericDeclarator() const = 0;
    virtual std::string getDemangledName() const;
    virtual const Type * replacePlaceholders(const Type * t) const;
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
    Procedure * idestroy_link;
    std::unordered_map<std::string, int> memberIndices;
    std::vector<const Type *> memberTypes;
    std::unordered_map<std::string, Constant *> constantMap;
    // std::unordered_map<std::string, std::vector<ASTNode*> > memberProcs;
    std::unordered_map<std::string, ProcSet *> memberProcs;
    std::set<std::string> interfaces;
    std::unordered_map<Procedure *, unsigned int> interfaceIndexMap;

    StructType(std::string & name, Struct * __struct = nullptr,
               TemplateInstantiation * _inst = nullptr);

    static const Type * get(std::string name);
    static const Type * get(std::string name, Struct * _struct,
                            TemplateInstantiation * inst = nullptr);

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;

    void complete();

    bool implementsInterfaces() const;
};

struct TupleType : Type {
    std::vector<const Type *> types;

    TupleType(const std::vector<const Type *> & _types);

    static const Type * get(const std::vector<const Type *> & types);

    const std::vector<const Type *> & getTypes() const;

    Declarator * getGenericDeclarator() const;

    std::string getDemangledName() const;

    const Type * replacePlaceholders(const Type * t) const;
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
} // namespace bjou
#endif /* Type_hpp */
