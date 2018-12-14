//
//  Type.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/8/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "Type.hpp"
#include "CLI.hpp"
#include "Compile.hpp"
#include "FrontEnd.hpp"
#include "Global.hpp"
#include "Misc.hpp"
#include "Operator.hpp"

#include <set>
#include <vector>

namespace bjou {

const char * signLtr = "ui";
#define VALID_IWIDTH(w) ((w) == 8 || (w) == 16 || (w) == 32 || (w) == 64)
#define VALID_FWIDTH(w) ((w) == 32 || (w) == 64 || (w) == 128)

Type::Type(Kind _kind, const std::string _key) : kind(_kind), key(_key) {}

void Type::alias(std::string name, const Type * t) { addTypeToTable(t, name); }

bool Type::isPlaceholder() const { return kind == PLACEHOLDER; }
bool Type::isVoid() const { return kind == VOID; }
bool Type::isBool() const { return kind == BOOL; }
bool Type::isInt() const { return kind == INT; }
bool Type::isFloat() const { return kind == FLOAT; }
bool Type::isChar() const { return kind == CHAR; }
bool Type::isPointer() const { return kind == POINTER; }
bool Type::isRef() const { return kind == REF; }
bool Type::isMaybe() const {
    BJOU_DEBUG_ASSERT(false);
    return false;
}
bool Type::isArray() const { return kind == ARRAY; }
bool Type::isSlice() const { return kind == SLICE; }
bool Type::isDynamicArray() const { return kind == DYNAMIC_ARRAY; }
bool Type::isStruct() const { return kind == STRUCT; }
bool Type::isEnum() const { return kind == ENUM; }
bool Type::isTuple() const { return kind == TUPLE; }
bool Type::isProcedure() const { return kind == PROCEDURE; }

bool Type::isPrimative() const {
    return kind == VOID || kind == BOOL || kind == INT || kind == FLOAT ||
           kind == CHAR;
}

const Type * Type::getPointer() const { return PointerType::get(this); }

const Type * Type::getRef() const { return RefType::get(this); }

const Type * Type::getMaybe() const {
    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}

const Type * Type::unRef() const {
    if (isRef())
        return under();
    return this;
}

const Type * Type::getBase() const {
    const Type * u = under();
    if (this == u)
        return this;
    return u->getBase();
}

const Type * Type::under() const { return this; }

std::string Type::getDemangledName() const { return demangledString(key); }

const Type * Type::replacePlaceholders(const Type * t) const { return this; }

const std::string PlaceholderType::plkey = "_";

const std::string VoidType::vkey = "void";

const std::string BoolType::bkey = "bool";

static inline std::string ikey(Type::Sign sign, int width) {
    return std::string() + signLtr[(int)sign] + std::to_string(width);
}

static inline std::string fkey(int width) {
    return std::string("f") + std::to_string(width);
}

const std::string CharType::ckey = "char";

static inline std::string pkey(const Type * elem_t) {
    return elem_t->key + "*";
}

static inline std::string rkey(const Type * t) { return t->key + " ref"; }

static inline std::string akey(const Type * t, int width) {
    return t->key + "[" + std::to_string(width) + "]";
}

static inline std::string skey(const Type * t) { return t->key + "[]"; }

static inline std::string dkey(const Type * t) { return t->key + "[...]"; }

static inline std::string tkey(const std::vector<const Type *> & types) {
    std::string key = "(";
    for (const Type * const & t : types) {
        key += t->key;
        if (&t != &types.back())
            key += ", ";
    }

    return key + ")";
}

static inline std::string prkey(const std::vector<const Type *> & paramTypes,
                                const Type * retType, bool isVararg) {
    std::string key = "<(";
    for (const Type * const & t : paramTypes) {
        key += t->key;
        if (&t != &paramTypes.back() || isVararg)
            key += ", ";
    }
    if (isVararg)
        key += "...";
    key += ")";
    if (retType->key != VoidType::vkey)
        key += " : " + retType->key;

    return key + ">";
}

static Declarator * basicDeclarator(const Type * t) {
    Declarator * declarator = new Declarator();
    declarator->context.filename = "<declarator created internally>";
    declarator->setIdentifier(mangledStringtoIdentifier(t->key));
    /*
    if (this->isStruct()) {
        StructType * s_t = (StructType*)this;
        if (s_t->inst)
            declarator->setTemplateInst(s_t->inst);
    }
     */
    declarator->createdFromType = true;
    return declarator;
}

PlaceholderType::PlaceholderType()
    : Type(PLACEHOLDER, PlaceholderType::plkey) {}

const Type * PlaceholderType::get() {
    return getOrAddType<PlaceholderType>(PlaceholderType::plkey);
}

Declarator * PlaceholderType::getGenericDeclarator() const {
    PlaceholderDeclarator * declarator = new PlaceholderDeclarator;
    declarator->context.filename = "<declarator created internally>";
    declarator->createdFromType = true;
    return declarator;
}

std::string PlaceholderType::getDemangledName() const { return key; }

const Type * PlaceholderType::replacePlaceholders(const Type * t) const {
    return t;
}

VoidType::VoidType() : Type(VOID, VoidType::vkey) {}

const Type * VoidType::get() { return getOrAddType<VoidType>(VoidType::vkey); }

Declarator * VoidType::getGenericDeclarator() const {
    return basicDeclarator(this);
}

std::string VoidType::getDemangledName() const { return key; }

BoolType::BoolType() : Type(BOOL, BoolType::bkey) {}

const Type * BoolType::get() { return getOrAddType<BoolType>(BoolType::bkey); }

Declarator * BoolType::getGenericDeclarator() const {
    return basicDeclarator(this);
}

std::string BoolType::getDemangledName() const { return key; }

IntType::IntType(Sign _sign, int _width)
    : Type(INT, ikey(_sign, _width)), sign(_sign), width(_width) {
    BJOU_DEBUG_ASSERT(VALID_IWIDTH(_width));
}

const Type * IntType::get(Sign sign, int width) {
    return getOrAddType<IntType>(ikey(sign, width), sign, width);
}

Declarator * IntType::getGenericDeclarator() const {
    return basicDeclarator(this);
}

std::string IntType::getDemangledName() const { return key; }

FloatType::FloatType(int _width) : Type(FLOAT, fkey(_width)), width(_width) {

    BJOU_DEBUG_ASSERT(VALID_FWIDTH(_width));
}

const Type * FloatType::get(int width) {
    return getOrAddType<FloatType>(fkey(width), width);
}

Declarator * FloatType::getGenericDeclarator() const {
    return basicDeclarator(this);
}

std::string FloatType::getDemangledName() const { return key; }

CharType::CharType() : Type(CHAR, CharType::ckey) {}

const Type * CharType::get() { return getOrAddType<CharType>(CharType::ckey); }

Declarator * CharType::getGenericDeclarator() const {
    return basicDeclarator(this);
}

std::string CharType::getDemangledName() const { return key; }

PointerType::PointerType(const Type * _elem_t)
    : Type(POINTER, pkey(_elem_t)), elem_t(_elem_t) {}

const Type * PointerType::get(const Type * elem_t) {
    return getOrAddType<PointerType>(pkey(elem_t), elem_t);
}

const Type * PointerType::under() const { return elem_t; }

Declarator * PointerType::getGenericDeclarator() const {
    PointerDeclarator * decl =
        new PointerDeclarator(under()->getGenericDeclarator());
    decl->createdFromType = true;
    return decl;
}

std::string PointerType::getDemangledName() const {
    return under()->getDemangledName() + "*";
}

const Type * PointerType::replacePlaceholders(const Type * t) const {
    return PointerType::get(under()->replacePlaceholders(t));
}

RefType::RefType(const Type * _t) : Type(REF, rkey(_t)), t(_t) {}

const Type * RefType::get(const Type * t) {
    return getOrAddType<RefType>(rkey(t), t);
}

const Type * RefType::under() const { return t; }

Declarator * RefType::getGenericDeclarator() const {
    RefDeclarator * decl = new RefDeclarator(under()->getGenericDeclarator());
    decl->createdFromType = true;
    return decl;
}

std::string RefType::getDemangledName() const {
    return under()->getDemangledName() + " ref";
}

const Type * RefType::replacePlaceholders(const Type * t) const {
    return RefType::get(under()->replacePlaceholders(t));
}

ArrayType::ArrayType(const Type * _elem_t, int _width)
    : Type(ARRAY, akey(_elem_t, _width)), elem_t(_elem_t), width(_width) {}

const Type * ArrayType::get(const Type * elem_t, int width) {
    return getOrAddType<ArrayType>(akey(elem_t, width), elem_t, width);
}

const Type * ArrayType::under() const { return elem_t; }

Declarator * ArrayType::getGenericDeclarator() const {
    IntegerLiteral * expr = new IntegerLiteral;
    ArrayDeclarator * decl =
        new ArrayDeclarator(under()->getGenericDeclarator(), expr);
    decl->createdFromType = true;
    expr->scope = decl->scope;
    expr->context = decl->context;
    expr->contents = std::to_string(width);
    return decl;
}

std::string ArrayType::getDemangledName() const {
    return under()->getDemangledName() + "[" + std::to_string(width) + "]";
}

const Type * ArrayType::replacePlaceholders(const Type * t) const {
    return ArrayType::get(under()->replacePlaceholders(t), width);
}

SliceType::SliceType(const Type * _elem_t)
    : Type(SLICE, skey(_elem_t)), elem_t(_elem_t) {

    // This will force template instantiation of the real type
    // in the front end. If we don't do this we will still instantiate,
    // but it will be lazy and might be in the back end. If that happens,
    // some things could be off like max_interface_procs

    getRealType();
}

const Type * SliceType::get(const Type * elem_t) {
    return getOrAddType<SliceType>(skey(elem_t), elem_t);
}

const Type * SliceType::getRealType() const {
    /////////////////////////////////////////////////////// @bad hack
    /////////////////////////////////////////////////////////////////
    Declarator * elem_decl = under()->getGenericDeclarator();

    Declarator * new_decl = new Declarator;
    new_decl->setScope(compilation->frontEnd.globalScope);

    Identifier * ident = new Identifier;
    ident->setScope(compilation->frontEnd.globalScope);
    ident->setUnqualified("__bjou_slice");

    TemplateInstantiation * new_inst = new TemplateInstantiation;
    new_inst->setScope(compilation->frontEnd.globalScope);
    new_inst->addElement(elem_decl);

    new_decl->setIdentifier(ident);
    new_decl->setTemplateInst(new_inst);

    new_decl->addSymbols(compilation->frontEnd.globalScope);

    PointerDeclarator * holder = new PointerDeclarator(new_decl);

    new_decl->analyze();
    new_decl->desugar();

    const Type * t = holder->getPointerOf()->getType();

    delete holder;

    return t;
}

const Type * SliceType::under() const { return elem_t; }

Declarator * SliceType::getGenericDeclarator() const {
    SliceDeclarator * decl =
        new SliceDeclarator(under()->getGenericDeclarator());
    decl->createdFromType = true;
    return decl;
}

std::string SliceType::getDemangledName() const {
    return under()->getDemangledName() + "[]";
}

const Type * SliceType::replacePlaceholders(const Type * t) const {
    return SliceType::get(under()->replacePlaceholders(t));
}

DynamicArrayType::DynamicArrayType(const Type * _elem_t)
    : Type(DYNAMIC_ARRAY, dkey(_elem_t)), elem_t(_elem_t) {

    // This will force template instantiation of the real type
    // in the front end. If we don't do this we will still instantiate,
    // but it will be lazy and might be in the back end. If that happens,
    // some things could be off like max_interface_procs

    getRealType();
}

const Type * DynamicArrayType::get(const Type * elem_t) {
    return getOrAddType<DynamicArrayType>(dkey(elem_t), elem_t);
}

const Type * DynamicArrayType::getRealType() const {
    /////////////////////////////////////////////////////// @bad hack
    /////////////////////////////////////////////////////////////////
    Declarator * elem_decl = under()->getGenericDeclarator();

    Declarator * new_decl = new Declarator;
    new_decl->setScope(compilation->frontEnd.globalScope);

    Identifier * ident = new Identifier;
    ident->setScope(compilation->frontEnd.globalScope);
    ident->setUnqualified("__bjou_dyn_array");

    new_decl->setIdentifier(ident);

    TemplateInstantiation * new_inst = new TemplateInstantiation;
    new_inst->setScope(compilation->frontEnd.globalScope);
    new_inst->addElement(elem_decl);

    new_decl->setTemplateInst(new_inst);

    new_decl->addSymbols(compilation->frontEnd.globalScope);

    PointerDeclarator * holder = new PointerDeclarator(new_decl);

    new_decl->analyze();
    new_decl->desugar();

    const Type * t = holder->getPointerOf()->getType();

    delete holder;

    return t;
}

const Type * DynamicArrayType::under() const { return elem_t; }

Declarator * DynamicArrayType::getGenericDeclarator() const {
    DynamicArrayDeclarator * decl =
        new DynamicArrayDeclarator(under()->getGenericDeclarator());
    decl->createdFromType = true;
    return decl;
}

std::string DynamicArrayType::getDemangledName() const {
    return under()->getDemangledName() + "[...]";
}

const Type * DynamicArrayType::replacePlaceholders(const Type * t) const {
    return DynamicArrayType::get(under()->replacePlaceholders(t));
}

StructType::StructType(std::string & name, Struct * __struct,
                       TemplateInstantiation * _inst)
    : Type(STRUCT, name), isAbstract(__struct->getFlag(Struct::IS_ABSTRACT)),
      isComplete(false), _struct(__struct), inst(_inst), extends(nullptr),
      idestroy_link(nullptr) {}

const Type * StructType::get(std::string name) {
    return getOrAddType<StructType>(name, name);
}

const Type * StructType::get(std::string name, Struct * _struct,
                             TemplateInstantiation * inst) {
    return getOrAddType<StructType>(name, name, _struct, inst);
}

static void insertProcSet(StructType * This, Procedure * proc) {
    // if (This->memberProcs.find(proc->getName()) == This->memberProcs.end()) {
    std::string lookup =
        This->_struct->getMangledName() + "." + proc->getName();
    Maybe<Symbol *> m_sym =
        This->_struct->getScope()->getSymbol(This->_struct->getScope(), lookup);
    Symbol * sym = nullptr;
    m_sym.assignTo(sym);
    BJOU_DEBUG_ASSERT(sym && sym->isProcSet());

    This->memberProcs[proc->getName()] = (ProcSet *)sym->node();
    // }
}

void StructType::complete() {
    // With aliases, there may be multiple entries in the table that point to
    // the same struct type. This makes sure that we don't screw up an already
    // completed type.
    if (isComplete)
        return;

    int i = 0;
    if (_struct->getExtends()) {
        extends = (Type *)_struct->getExtends()->getType(); // @leak?
        StructType * extends_s = (StructType *)extends;
        memberIndices = extends_s->memberIndices;
        memberTypes = extends_s->memberTypes;
        memberProcs = extends_s->memberProcs;
        i += memberTypes.size();
    }
    for (ASTNode * _mem : _struct->getMemberVarDecls()) {
        VariableDeclaration * mem = (VariableDeclaration *)_mem;
        const std::string & name = mem->getName();

        memberIndices[name] = i;
        const Type * mem_t = mem->getType();
        BJOU_DEBUG_ASSERT(mem_t);
        memberTypes.push_back(mem_t);
        i += 1;
    }

    for (ASTNode * _constant : _struct->getConstantDecls()) {
        Constant * constant = (Constant *)_constant;
        constantMap[constant->getName()] = constant;
    }

    for (ASTNode * _proc : _struct->getMemberProcs()) {
        Procedure * proc = (Procedure *)_proc;

        insertProcSet(this, proc);
    }

    for (ASTNode * _template_proc : _struct->getMemberTemplateProcs()) {
        TemplateProc * template_proc = (TemplateProc *)_template_proc;
        Procedure * proc = (Procedure *)template_proc->getTemplate();

        insertProcSet(this, proc);
    }

    unsigned int interface_idx = 0;

    for (ASTNode * _impl : _struct->getAllInterfaceImplsSorted()) {
        InterfaceImplementation * impl = (InterfaceImplementation *)_impl;
        Identifier * implIdent = (Identifier *)impl->getIdentifier();
        interfaces.insert(mangledIdentifier(implIdent));
        int i = 0;
        for (auto & procs : impl->getProcs()) {
            for (ASTNode * _proc : procs.second) {
                Procedure * proc = (Procedure *)_proc;

                interfaceIndexMap[proc] = interface_idx;
                interface_idx += 1;
                insertProcSet(this, proc);

                i += 1;
            }
        }
    }
    isComplete = true;
}

bool StructType::implementsInterfaces() const {
    BJOU_DEBUG_ASSERT(isComplete);

    return interfaces.size() != 0;
}

Declarator * StructType::getGenericDeclarator() const {
    return basicDeclarator(this);
}

std::string StructType::getDemangledName() const {
    return demangledString(key);
}

EnumType::EnumType(std::string & name, Enum * __enum)
    : Type(ENUM, name),
      _enum(__enum) {

    if (_enum) {
        identifiers.insert(
                           identifiers.begin(),
                           _enum->identifiers.begin(),
                           _enum->identifiers.end());    
    }
}

const Type * EnumType::get(std::string name) {
    return getOrAddType<EnumType>(name, name);
}

const Type * EnumType::get(std::string name, Enum * __enum) {
    return getOrAddType<EnumType>(name, name, __enum);
}

Declarator * EnumType::getGenericDeclarator() const {
    return basicDeclarator(this);
}

std::string EnumType::getDemangledName() const {
    return demangledString(key);
}

IntegerLiteral * EnumType::getValueLiteral(std::string& identifier, Context & context, Scope * scope) const {
    int i = 0;
    for (const std::string& id : identifiers) {
        if (id == identifier)
            break;
        i += 1;
    }

    if (i == identifiers.size())
        return nullptr;

    IntegerLiteral * lit = new IntegerLiteral;
    lit->setContents(std::to_string(i) + "u64");
    lit->setContext(context);
    lit->setScope(scope);

    return lit;
}

TupleType::TupleType(const std::vector<const Type *> & _types)
    : Type(TUPLE, tkey(_types)), types(_types) {
    BJOU_DEBUG_ASSERT(!types.empty());
}

const Type * TupleType::get(const std::vector<const Type *> & types) {
    return getOrAddType<TupleType>(tkey(types), types);
}

const std::vector<const Type *> & TupleType::getTypes() const { return types; }

Declarator * TupleType::getGenericDeclarator() const {
    TupleDeclarator * decl = new TupleDeclarator();
    for (const Type * st : types)
        decl->addSubDeclarator(st->getGenericDeclarator());
    decl->createdFromType = true;
    return decl;
}

std::string TupleType::getDemangledName() const {
    std::string demangled = "(";
    for (const Type * const & st : types) {
        demangled += st->getDemangledName();
        if (&st != &types.back())
            demangled += ", ";
    }
    demangled += ")";

    return demangled;
}

const Type * TupleType::replacePlaceholders(const Type * t) const {
    std::vector<const Type *> newTypes;
    for (auto st : types)
        newTypes.push_back(st->replacePlaceholders(t));
    return TupleType::get(newTypes);
}

ProcedureType::ProcedureType(const std::vector<const Type *> & _paramTypes,
                             const Type * _retType, bool _isVararg)
    : Type(PROCEDURE, prkey(_paramTypes, _retType, _isVararg)),
      paramTypes(_paramTypes), isVararg(_isVararg), retType(_retType) {}

const Type * ProcedureType::get(const std::vector<const Type *> & paramTypes,
                                const Type * retType, bool isVararg) {
    return getOrAddType<ProcedureType>(prkey(paramTypes, retType, isVararg),
                                       paramTypes, retType, isVararg);
}

const std::vector<const Type *> & ProcedureType::getParamTypes() const {
    return paramTypes;
}

const Type * ProcedureType::getRetType() const { return retType; }

Declarator * ProcedureType::getGenericDeclarator() const {
    ProcedureDeclarator * decl = new ProcedureDeclarator();
    for (const Type * pt : paramTypes)
        decl->addParamDeclarator(pt->getGenericDeclarator());
    decl->setRetDeclarator(retType->getGenericDeclarator());
    decl->createdFromType = true;
    return decl;
}

std::string ProcedureType::getDemangledName() const {
    std::string demangled = "<(";
    for (const Type * const & paramT : paramTypes) {
        demangled += paramT->getDemangledName();
        if (&paramT != &paramTypes.back() || isVararg)
            demangled += ", ";
    }
    if (isVararg)
        demangled += "...";
    demangled += ")";
    if (retType != VoidType::get())
        demangled += " : " + retType->getDemangledName();
    demangled += ">";

    return demangled;
}

const Type * ProcedureType::replacePlaceholders(const Type * t) const {
    std::vector<const Type *> newParamTypes;
    for (auto pt : paramTypes)
        newParamTypes.push_back(pt->replacePlaceholders(t));
    return ProcedureType::get(newParamTypes, retType->replacePlaceholders(t),
                              isVararg);
}

bool equal(const Type * t1, const Type * t2) {
    if (t1->kind != t2->kind)
        return false;

    switch (t1->kind) {
    case Type::VOID:
        return true;
    case Type::BOOL:
        return t1 == t2;
    case Type::INT: {
        const IntType * i1 = (const IntType *)t1;
        const IntType * i2 = (const IntType *)t2;

        return (i1->sign == i2->sign) && (i1->width == i2->width);
    }
    case Type::ENUM:
        return t1 == t2;
    case Type::FLOAT: {
        const FloatType * f1 = (const FloatType *)t1;
        const FloatType * f2 = (const FloatType *)t2;

        return f1->width == f2->width;
    }
    case Type::CHAR:
        return t1 == t2;
    case Type::POINTER:
        if (t2->isPointer())
            return equal(t1->under(), t2->under());
        break;
    case Type::REF: {
        if (t2->isRef())
            return equal(t1->unRef(), t2->unRef());
        return false;
        break;
    }
    case Type::ARRAY: {
        if (t2->isArray()) {
            ArrayType * a1 = (ArrayType *)t1;
            ArrayType * a2 = (ArrayType *)t2;
            if (!equal(a1->under(), a2->under()))
                return false;
            return a1->width == a2->width;
        }
        break;
    }
    case Type::SLICE: {
        if (t2->isSlice())
            if (equal(t1->under(), t2->under()))
                return true;
        break;
    }
    case Type::DYNAMIC_ARRAY: {
        if (t2->isDynamicArray())
            if (equal(t1->under(), t2->under()))
                return true;
        break;
    }
    case Type::STRUCT:
        return t1 == t2;
    case Type::TUPLE: {
        const TupleType * tp1 = (const TupleType *)t1;
        const TupleType * tp2 = (const TupleType *)t2;

        if (tp1->getTypes().size() != tp2->getTypes().size())
            return false;

        for (int i = 0; i < (int)tp1->getTypes().size(); i += 1)
            if (!equal(tp1->getTypes()[i], tp2->getTypes()[i]))
                return false;
        return true;
    }
    case Type::PROCEDURE: {
        const ProcedureType * p1 = (const ProcedureType *)t1;
        const ProcedureType * p2 = (const ProcedureType *)t2;

        if (p1->getParamTypes().size() != p2->getParamTypes().size())
            return false;

        for (int i = 0; i < (int)p1->getParamTypes().size(); i += 1)
            if (!equal(p1->getParamTypes()[i], p2->getParamTypes()[i]))
                return false;

        return equal(p1->getRetType(), p2->getRetType());
    }
    }

    return false;
}

const Type * convThroughPointerOrRefExtension(const Type *t1, const Type *t2) {
    const Type * r1 = nullptr;
    const Type * r2 = nullptr;

    enum { PTR, REF };

    int kind1 = t1->isPointer() ? PTR : REF; 
    int kind2 = t2->isPointer() ? PTR : REF; 

    if (kind1 == PTR)    r1 = t1->under();
    else                 r1 = t1->unRef();
    if (kind2 == PTR)    r2 = t2->under();
    else                 r2 = t2->unRef();

    if (r1->isStruct() && r2->isStruct()) {
        const StructType * s1 = (const StructType *)r1;
        const StructType * s2 = (const StructType *)r2;
        const Type * extends = nullptr;
        while (s2->extends) {
            s2 = (const StructType *)s2->extends;
            if (equal(s1, s2)) {
                extends = s2;
                break;
            }
        }
        if (extends)
            return t1;
    }

    return nullptr;
}

// returns a conversion type if t1 and t2 can convert to a common type
// order of t1 and t2 matters in some cases:
//      conv(Derived*, Base*) = nullptr
//      conv(Base*, Derived*) = Base*
// think of conv(t1*, t2*) asking whether the following expression can
// implicitly convert:
//      b : t1*
//      b = new t2.create()
const Type * conv(const Type * t1, const Type * t2) {
    if (equal(t1, t2))
        return t1;

    if (t2->isEnum())
        t2 = IntType::get(Type::Sign::UNSIGNED, 64);

    if (t2->isRef()) {
        const Type * r = conv(t1, t2->unRef());
        if (r)
            return r;
    }

    // handle int and float conversions
    // one of l and r is float, choose float type
    if (t1->isFloat() && (t2->isInt() || t2->isChar()))
        return t1;
    if ((t1->isInt() || t1->isChar()) && t2->isFloat())
        return t2;
    // if both float, choose larger
    if (t1->isFloat() && t2->isFloat()) {
        if (((FloatType *)t1)->width >= ((FloatType *)t2)->width)
            return t1;
        else
            return t2;
    }

    if (t1->isInt() && t2->isBool())
        return t1;
    if (t1->isBool() && (t2->isInt() || t2->isChar()))
        return t1;

    // both are ints
    if (t1->isInt() && t2->isInt()) {
        // unless both are unsigned, we go signed
        Type::Sign sign = Type::Sign::UNSIGNED;
        if (((IntType *)t1)->sign == Type::Sign::SIGNED ||
            ((IntType *)t2)->sign == Type::Sign::SIGNED)
            sign = Type::Sign::SIGNED;
        // choose largest width
        int width = ((IntType *)t1)->width >= ((IntType *)t2)->width
                        ? ((IntType *)t1)->width
                        : ((IntType *)t2)->width;

        return IntType::get(sign, width);
    }

    if (t1->isFloat() && t2->isChar())
        return t1;
    if (t1->isChar() && t2->isInt())
        return t1;
    if (t1->isInt() && t2->isChar())
        return t1;

    switch (t1->kind) {
    case Type::VOID:
        return nullptr;
    case Type::REF: {
        const Type * un = conv(t1->unRef(), t2->unRef());
        if (un)
            return t1;

        const Type *ref_ext = convThroughPointerOrRefExtension(t1, t2);
        if (ref_ext)
            return t1;

        break;
    }
    case Type::POINTER: {
        const Type * p1 = t1->under();
        const Type * p2 = t2->under();

        if (t2->isArray() && (p1->isVoid() || equal(p1, p2)))
            return t1;

        if (p1->isVoid() && t2->isPointer())
            return t1;

        if (p1->isStruct() && t2->isPointer() && p2->isStruct()) {
            const Type *ptr_ext = convThroughPointerOrRefExtension(t1, t2);
            if (ptr_ext)
                return t1;
        }
        break;
    }
    case Type::ARRAY: {
        if (t2->isPointer() && equal(t1->under(), t2->under()))
            return t2;
        break;
    }
    case Type::SLICE: {
        if (t2->isSlice())
            if (equal(t1->under(), t2->under()))
                return t1;
        break;
    }
    case Type::DYNAMIC_ARRAY: {
        if (t2->isDynamicArray())
            if (equal(t1->under(), t2->under()))
                return t1;
        break;
    }
    }

    return nullptr;
}

bool argMatch(const Type * t1, const Type * t2, bool exact) {
    if (!t1->isProcedure() || !t2->isProcedure())
        return false;

    const ProcedureType * p1 = (const ProcedureType *)t1;
    const ProcedureType * p2 = (const ProcedureType *)t2;

    size_t nparams1 = p1->paramTypes.size();
    size_t nparams2 = p2->paramTypes.size();

    size_t min_nparams = nparams1 <= nparams2 ? nparams1 : nparams2;

    if (nparams1 < nparams2 && !p1->isVararg)
        return false;
    if (nparams2 < nparams1 && !p2->isVararg)
        return false;

    if (exact) {
        for (unsigned int i = 0; i < min_nparams; i += 1)
            if (!equal(p1->paramTypes[i], p2->paramTypes[i]))
                return false;
    } else {
        for (unsigned int i = 0; i < min_nparams; i += 1) {
            /* conv() doesn't exactly fit what we need in this situation.
             * Need to check that unref'd types are equal rather than just
             * convertible when passed as proc args. */
            if (p1->paramTypes[i]->isRef()) {
                if (!equal(p1->paramTypes[i]->unRef(), p2->paramTypes[i]->unRef())
                &&  !convThroughPointerOrRefExtension(p1->paramTypes[i], p2->paramTypes[i]))
                    return false;
            } else {
                if (!conv(p1->paramTypes[i], p2->paramTypes[i]))
                    return false;
            }
        }
    }

    return true;
}

const Type * getTypeFromTable(const std::string & key) {
    auto val = compilation->frontEnd.typeTable.find(key);

    if (val != compilation->frontEnd.typeTable.end())
        return val->second;
    return nullptr;
}

void addTypeToTable(const Type * t, const std::string & key) {
    compilation->frontEnd.typeTable[key] = t;
}

// @fun
std::vector<const Type *>
typesSortedByDepencencies(std::vector<const Type *> nonPrimatives) {
    size_t size = nonPrimatives.size();
    std::vector<const Type *> sorted, keep;
    std::set<std::string> availableByKey;
    std::vector<int> indices;
    int idx, nadded;
    sorted.reserve(size);
    keep.reserve(size);
    indices.reserve(size);

    while (sorted.size() < size) {
        idx = 0;
        nadded = 0;

        indices.clear();

        ASTNode * badTerm = nullptr;
        bool extends_err = false;

        // each type left to place
        for (const Type * t : nonPrimatives) {
            bool good = true;
            std::vector<ASTNode *> terms;
            ASTNode * extends_term = nullptr;
            // break it down to the declarators and expressions that would
            // require knowledge of a certain type beyond its name
            if (t->isStruct()) {
                Struct * td = ((const StructType *)t)->_struct;
                if (td->getExtends()) {
                    terms.push_back(td->getExtends());
                    extends_term = td->getExtends();
                }
                for (ASTNode * _mem : td->getMemberVarDecls()) {
                    VariableDeclaration * mem = (VariableDeclaration *)_mem;
                    mem->getTypeDeclarator()->analyze();
                    Declarator * decl = (Declarator *)mem->getTypeDeclarator();

                    if (((Declarator *)decl->getBase())
                            ->getType()
                            ->isStruct()) {
                        const StructType * s_t =
                            (StructType *)((Declarator *)decl->getBase())
                                ->getType();
                        if (s_t->_struct->getFlag(Struct::IS_TEMPLATE_DERIVED))
                            availableByKey.insert(s_t->key);
                    }

                    // declarators with a pointer ANYWHERE in them imply that we
                    // don't need to know the size of the base type
                    // PointerDeclarator handles this on construction and sets a
                    // flag in their respective base Declarator
                    if (decl && decl->getBase()->getFlag(
                                    Declarator::IMPLIES_COMPLETE)) {

                        terms.push_back(decl);
                    }

                    // look out for expressions also
                    // ex. of a something we have to account for:
                    // type A {
                    //      a := { B: 12345 }
                    // }
                    // type B {
                    //      i : int
                    // }

                    if (mem->getInitialization()) {
                        std::vector<ASTNode *> init_sub_nodes;
                        mem->getInitialization()->unwrap(init_sub_nodes);
                        for (ASTNode * term : init_sub_nodes)
                            if (term->nodeKind >= ASTNode::DECLARATOR &&
                                term->nodeKind < ASTNode::_END_DECLARATORS)
                                if (((Declarator *)term)
                                        ->getBase()
                                        ->getFlag(Declarator::IMPLIES_COMPLETE))
                                    terms.push_back(term);
                    }
                }
            } /* else if (t->isAlias()) {
                // @todo
                // what should happen here is that we should have access to the
                // underlying declarator and unwrap that
            }
            */
            for (ASTNode * term : terms) {
                const Type * t = term->getType();
                BJOU_DEBUG_ASSERT(t);
                if (t->getBase()->isStruct()) { // || t->getBase()->isAlias()) {
                    // has the type that this term needs already been placed in
                    // 'sorted'?
                    if (std::find(availableByKey.begin(), availableByKey.end(),
                                  t->getBase()->key) == availableByKey.end()) {
                        good = false;
                        if (!badTerm)
                            badTerm = term; // set for error
                        if (badTerm == extends_term)
                            extends_err = true;
                        break;
                    }
                }
            }
            if (good) {
                indices.push_back(idx);
                nadded += 1;
            }
            idx += 1;
        }

        keep.clear();
        size_t nNonPrimatives = nonPrimatives.size();
        for (unsigned int i = 0; i < nNonPrimatives; i += 1) {
            if (std::find(indices.begin(), indices.end(), i) != indices.end()) {
                sorted.push_back(nonPrimatives[i]);
                availableByKey.insert(nonPrimatives[i]->key);
            } else
                keep.push_back(nonPrimatives[i]);
        }

        nonPrimatives = keep;

        // If we find a circular dependency.
        // i.e. We have gone through all of the types in 'nonPrimatives' without
        // placing a single one in 'sorted' so something couldn't be resolved.
        // The problem isn't that a type wasn't "complete" -- the whole point of
        // this algorithm is to make incomplete types impossible. However, we
        // can't solve circular dependencies.
        if (nadded == 0) {
            Context t_context, ref_context;
            if (nonPrimatives[0]->isStruct()) {
                t_context =
                    ((StructType *)nonPrimatives[0])->_struct->getNameContext();
            }
            /*
            else if (nonPrimatives[0]->isAlias()) {
                // @todo add suport for aliases
            }
            */
            std::string demangledA = demangledString(nonPrimatives[0]->key);
            std::string demangledB =
                demangledString(badTerm->getType()->getBase()->key); // @leak
            ref_context = badTerm->getContext();
            errorl(t_context,
                   "Definition of type '" + demangledA +
                       "' creates a circular dependency with type '" +
                       demangledB + "'.",
                   false);
            if (extends_err)
                errorl(ref_context, "'" + demangledB + "' referenced here.",
                       true, "Note: types may not extend themselves.");
            else
                errorl(ref_context, "'" + demangledB + "' referenced here.",
                       true,
                       "Did you mean to declare a reference or pointer to '" +
                           demangledB + "'?");
        }
    }

    return sorted;
}

void compilationAddPrimativeTypes() {
    const Type * primatives[] = {VoidType::get(),
                                 BoolType::get(),
                                 IntType::get(Type::UNSIGNED, 8),
                                 IntType::get(Type::UNSIGNED, 16),
                                 IntType::get(Type::UNSIGNED, 32),
                                 IntType::get(Type::UNSIGNED, 64),
                                 IntType::get(Type::SIGNED, 8),
                                 IntType::get(Type::SIGNED, 16),
                                 IntType::get(Type::SIGNED, 32),
                                 IntType::get(Type::SIGNED, 64),
                                 FloatType::get(32),
                                 FloatType::get(64),
                                 FloatType::get(128),
                                 CharType::get()};

    for (const Type * p : primatives) {
        compilation->frontEnd.primativeTypeTable[p->key] = p;
    }

    compilation->frontEnd.primativeTypeTable["int"] =
        IntType::get(Type::SIGNED, 32);
    compilation->frontEnd.primativeTypeTable["float"] = FloatType::get(32);
}

unsigned int simpleSizer(const Type * t) {
    unsigned int size = 0;

    if (t->isVoid()) {
        size = 0;
    } else if (t->isBool()) {
        size = 1;
    } else if (t->isInt()) {
        size = ((IntType *)t)->width / 8;
    } else if (t->isFloat()) {
        size = ((FloatType *)t)->width / 8;
    } else if (t->isChar()) {
        size = 1;
    } else if (t->isPointer() || t->isRef() || t->isProcedure()) {
        size = sizeof(void *);
    } else if (t->isArray()) {
        ArrayType * a_t = (ArrayType *)t;
        size = a_t->width * simpleSizer(a_t->elem_t);
    } else if (t->isSlice()) {
        SliceType * s_t = (SliceType *)t;
        size = simpleSizer(s_t->getRealType());
    } else if (t->isDynamicArray()) {
        DynamicArrayType * d_t = (DynamicArrayType *)t;
        size = simpleSizer(d_t->getRealType());
    } else if (t->isStruct()) {
        StructType * s_t = (StructType *)t;
        for (const Type * m_t : s_t->memberTypes)
            size += simpleSizer(m_t);
        if (s_t->implementsInterfaces())
            size += sizeof(void *);
    } else if (t->isEnum()) {
        size = 8;
    } else if (t->isTuple()) {
        TupleType * t_t = (TupleType *)t;
        for (const Type * s_t : t_t->getTypes())
            size += simpleSizer(s_t);
    } else {
        BJOU_DEBUG_ASSERT(false && "type not handled in simpleSizer()");
    }

    return size;
}

int countConversions(ProcedureType * compare_type,
                     ProcedureType * candidate_type) {
    int nconv = 0;
    for (int i = 0; i < (int)candidate_type->paramTypes.size(); i += 1) {
        const Type * t1 = compare_type->paramTypes[i];
        const Type * t2 = candidate_type->paramTypes[i];
        if (!conv(t1, t2))
            return -1;
        if (!equal(t1, t2)) {
            nconv += 2;
            // account for reference conversions too
            if (t1->isRef()) {
                if (!equal(t1->unRef(), t2->unRef()))
                    nconv += 1;
            }

            if (t1->isPointer() || t1->isRef()) {
                const Type * u1 = t1->under();
                const Type * u2 = nullptr;
                if (t2->isPointer() || t2->isRef())
                    u2 = t2->under();
                else
                    u2 = t2;

                if (u1->isStruct() && u2->isStruct()) {
                    int n_extends = 0;
                    const StructType * s1 = (const StructType *)u1;
                    const StructType * s2 = (const StructType *)u2;
                    const Type * extends = nullptr;

                    if (!equal(s1, s2)) {
                        while (s2->extends) {
                            s2 = (const StructType *)s2->extends;
                            if (equal(s1, s2)) {
                                extends = s2;
                                break;
                            }
                            n_extends += 1;
                        }

                        nconv += n_extends;
                    }
                }
            }
        }
    }
    return nconv;
}

} // namespace bjou
