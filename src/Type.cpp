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
Type::Type() : kind(BASE) {}
Type::Type(Kind _kind) : kind(_kind) {}
Type::Type(std::string _name) : kind(BASE), code(_name) {}
Type::Type(Kind _kind, std::string _name, Sign _sign, int _size)
    : kind(_kind), sign(_sign), size(_size), code(_name) {}

bool Type::isValid() const { return kind != INVALID; }
bool Type::isPrimative() const { return kind == PRIMATIVE; }
bool Type::isFP() const {
    return code == "float" || code == "double" || code == "f32" ||
           code == "f64";
}
bool Type::isStruct() const { return kind == STRUCT; }
bool Type::isEnum() const { return kind == ENUM; }
bool Type::isAlias() const { return kind == ALIAS; }
bool Type::isArray() const { return kind == ARRAY; }
bool Type::isPointer() const { return kind == POINTER; }
bool Type::isMaybe() const { return kind == MAYBE; }
bool Type::isTuple() const { return kind == TUPLE; }
bool Type::isProcedure() const { return kind == PROCEDURE; }
bool Type::isTemplateStruct() const { return kind == TEMPLATE_STRUCT; }
bool Type::isTemplateAlias() const { return kind == TEMPLATE_ALIAS; }

bool Type::enumerableEquivalent() const {
    const char * enumerables[] = {"bool", "u8",   "i8",    "u16",  "i16",
                                  "u32",  "i32",  "u64",   "i64",  "int",
                                  "uint", "long", "ulong", "char", "short"};

    return (s_in_a(getOriginal()->code.c_str(), enumerables)) || isEnum();
}

// Type interface
const Type * Type::getBase() const { return this; }

const Type * Type::getOriginal() const { return this; }

bool Type::equivalent(const Type * other, bool exactMatch) const {
    if (isPrimative() && other->isPrimative()) {
        if (((size == -1) ^ (other->size == -1)))
            return false;
        if (exactMatch)
            return (sign == other->sign) && (size == other->size);
        return primativeConversionResult(this, other);

        /*
        if (exactMatch)
            return (sign == other->sign) && (size == other->size);
        else {
            // void
            if (((size == -1) ^ (other->size == -1)))
                return false;
            // others
            return true;
        }
         */
    } else {
        if (exactMatch)
            return getOriginal()->code == other->getOriginal()->code;
        else
            return (getOriginal()->code == other->getOriginal()->code) ||
                   (enumerableEquivalent() && other->enumerableEquivalent());
    }
    return false;
}

// @leak
Declarator * Type::getGenericDeclarator() const {
    Declarator * declarator = new Declarator();
    declarator->context.filename = "<declarator created internally>";
    declarator->setIdentifier(mangledStringtoIdentifier(code));
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

std::string Type::getDemangledName() const { return demangledString(code); }

bool Type::opApplies(std::string & op) const {
    const char * applicable[] = {"!", "not", "&",  "*",  "/",  "%",  "+",
                                 "-", "<",   "<=", ">",  ">=", "==", "!=",
                                 "=", "*=",  "/=", "%=", "+=", "-="};
    const char * boolean[] = {"&&", "and", "||", "or"};

    if (equivalent(compilation->frontEnd.typeTable["void"]))
        return false;
    if (equivalent(compilation->frontEnd.typeTable["bool"]))
        return s_in_a(op.c_str(), applicable) || s_in_a(op.c_str(), boolean);
    return s_in_a(op.c_str(), applicable);
}

bool Type::isValidOperand(const Type * operand, std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {"*",  "/",  "%", "+",  "-",  "<",  "<=", ">", ">=",
                           "==", "!=", "=", "*=", "/=", "%=", "+=", "-="};
    const char * boolean[] = {"&&", "and", "||", "or"};
    if (s_in_a(op.c_str(), same))
        return equivalent(operand) ||
               (enumerableEquivalent() && operand->enumerableEquivalent());
    if (s_in_a(op.c_str(), boolean))
        return operand->equivalent(compilation->frontEnd.typeTable["bool"]);
    return false;
}

const Type * Type::binResultType(const Type * operand, std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {"*",  "/",  "%",  "+",  "-", "=",
                           "*=", "/=", "%=", "+=", "-="};
    const char * boolean[] = {"&&", "and", "||", "or", "<",
                              "<=", ">",   ">=", "==", "!="};

    if (s_in_a(op.c_str(), same))
        return this;
    if (s_in_a(op.c_str(), boolean))
        return compilation->frontEnd.typeTable["bool"];
    return nullptr;
}

const Type * Type::unResultType(std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && unary(op.c_str()));
    const char * pointer[] = {"&"};
    const char * boolean[] = {"!", "not"};

    if (s_in_a(op.c_str(), pointer))
        return pointerOf();
    if (s_in_a(op.c_str(), boolean))
        return compilation->frontEnd.typeTable["bool"];
    return nullptr;
}

const Type * Type::arrayOf() const { return new ArrayType(this); }
const Type * Type::pointerOf() const { return new PointerType(this); }
const Type * Type::maybeOf() const { return new MaybeType(this); }
const Type * Type::replacePlaceholders(const Type * t) const {
    return new Type(*this);
}

Type::~Type() {}
//

InvalidType::InvalidType() : Type(INVALID) {}

PlaceholderType::PlaceholderType() : Type(PLACEHOLDER) { code = "_"; }

const Type * PlaceholderType::replacePlaceholders(const Type * t) const {
    return t;
}

// Type interface
bool PlaceholderType::equivalent(const Type * other, bool exactMatch) const {
    // return other->kind == Type::PLACEHOLDER;
    return true;
}

Declarator * PlaceholderType::getGenericDeclarator() const {
    Declarator * declarator = new PlaceholderDeclarator();
    declarator->context.filename = "<declarator created internally>";
    declarator->createdFromType = true;
    return declarator;
}
//

StructType::StructType() : Type(STRUCT) {}

StructType::StructType(std::string & name, Struct * __struct,
                       TemplateInstantiation * _inst)
    : Type(STRUCT, name), isAbstract(__struct->getFlag(Struct::IS_ABSTRACT)),
      _struct(__struct), inst(_inst), extends(nullptr), idestroy_link(nullptr) {}

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
}

bool StructType::opApplies(std::string & op) const {
    const char * applicable[] = {"==", "!=", "=", "&"};
    return s_in_a(op.c_str(), applicable);
}

bool StructType::isValidOperand(const Type * operand, std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {"==", "!=", "="};
    if (s_in_a(op.c_str(), same))
        return equivalent(operand);
    return false;
}

const Type * StructType::binResultType(const Type * operand,
                                       std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {"="};
    const char * boolean[] = {"==", "!="};

    if (s_in_a(op.c_str(), same))
        return this;
    else if (s_in_a(op.c_str(), boolean))
        return compilation->frontEnd.typeTable["bool"];
    else
        return nullptr;
}

const Type * StructType::unResultType(std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && unary(op.c_str()));
    const char * pointer[] = {"&"};

    if (s_in_a(op.c_str(), pointer))
        return pointerOf();
    return nullptr;
}

const Type * StructType::replacePlaceholders(const Type * t) const {
    return new StructType(*this);
}

EnumType::EnumType() : Type(ENUM) {}

EnumType::EnumType(std::string & name, ASTNode * __enum) : Type(ENUM, name) {
    Enum * _enum = (Enum *)__enum;
    std::vector<std::string> & idents = _enum->getIdentifiers();
    for (unsigned int i = 0; i < idents.size(); i += 1)
        valMap[idents[i]] = i;
}

AliasType::AliasType() : Type(ALIAS) {}

AliasType::AliasType(std::string & name, Alias * _alias)
    : Type(ALIAS, name), alias(_alias) {
    code = name;
}

const Type * AliasType::getOriginal() const { return alias_of; }

bool AliasType::equivalent(const Type * other, bool exactMatch) const {
    return alias_of->equivalent(other, exactMatch);
}

bool AliasType::opApplies(std::string & op) const {
    // @future check overloads here instead of alias_of?
    return alias_of->opApplies(op);
}

bool AliasType::isValidOperand(const Type * operand, std::string & op) const {
    return alias_of->isValidOperand(operand, op);
}

const Type * AliasType::binResultType(const Type * operand,
                                      std::string & op) const {
    return alias_of->binResultType(operand, op);
}

const Type * AliasType::unResultType(std::string & op) const {
    return alias_of->unResultType(op);
}

const Type * AliasType::replacePlaceholders(const Type * t) const {
    AliasType * result = new AliasType(*this);
    result->alias_of = alias_of->replacePlaceholders(t);
    return result;
}

void AliasType::complete() { alias_of = alias->getDeclarator()->getType(); }

ArrayType::ArrayType() : Type(ARRAY) {}

ArrayType::ArrayType(const Type * _array_of)
    : Type(ARRAY), array_of(_array_of), expression(nullptr), size(-1) {
    code = array_of->code + "[]";
}

ArrayType::ArrayType(const Type * _array_of, Expression * _expression)
    : Type(ARRAY), array_of(_array_of), expression(_expression) {
    code = array_of->code + "[]";
    if (expression->isConstant())
        size = (int)expression->eval().as_i64;
    else
        size = -1;
}

bool ArrayType::equivalent(const Type * other, bool exactMatch) const {
    return (Type::equivalent(other, exactMatch)) ||
           ((other->isPointer() &&
             array_of->equivalent(((PointerType *)other)->pointer_of,
                                  exactMatch)));
}

const Type * ArrayType::getBase() const { return array_of->getBase(); }

Declarator * ArrayType::getGenericDeclarator() const {
    ArrayDeclarator * decl =
        new ArrayDeclarator(array_of->getGenericDeclarator(), expression);
    decl->size = size;
    decl->createdFromType = true;
    return decl;
}

std::string ArrayType::getDemangledName() const {
    return array_of->getDemangledName() + "[]";
}

bool ArrayType::opApplies(std::string & op) const {
    const char * applicable[] = {"&", "@", "+", "-", "==", "!=", "=", "[]"};

    return s_in_a(op.c_str(), applicable);
}

bool ArrayType::isValidOperand(const Type * operand, std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {"+", "-", "==", "!=", "="};
    const char * access[] = {"[]"};

    if (s_in_a(op.c_str(), same))
        return equivalent(operand);
    if (s_in_a(op.c_str(), access))
        return operand->enumerableEquivalent();
    return false;
}

const Type * ArrayType::binResultType(const Type * operand,
                                      std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {"+", "-", "="};
    const char * boolean[] = {"==", "!="};
    const char * access[] = {"[]"};

    if (s_in_a(op.c_str(), same))
        return this;
    if (s_in_a(op.c_str(), boolean))
        return compilation->frontEnd.typeTable["bool"];
    if (s_in_a(op.c_str(), access))
        return array_of;
    return nullptr;
}

const Type * ArrayType::unResultType(std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && unary(op.c_str()));
    const char * pointer[] = {"&"};

    if (s_in_a(op.c_str(), pointer))
        return pointerOf();
    return nullptr;
}

const Type * ArrayType::replacePlaceholders(const Type * t) const {
    ArrayType * result = new ArrayType(*this);
    result->array_of = array_of->replacePlaceholders(t);
    return result;
}

DynamicArrayType::DynamicArrayType() : Type(DYNAMIC_ARRAY) {}

DynamicArrayType::DynamicArrayType(const Type * _array_of)
    : Type(DYNAMIC_ARRAY), array_of(_array_of) {
    code = array_of->code + "[...]";
}

bool DynamicArrayType::equivalent(const Type * other, bool exactMatch) const {
    return (Type::equivalent(other, exactMatch)) ||
           ((other->isPointer() &&
             array_of->equivalent(((PointerType *)other)->pointer_of,
                                  exactMatch)));
}

const Type * DynamicArrayType::getBase() const { return array_of->getBase(); }

Declarator * DynamicArrayType::getGenericDeclarator() const {
    DynamicArrayDeclarator * decl =
        new DynamicArrayDeclarator(array_of->getGenericDeclarator());
    decl->createdFromType = true;
    return decl;
}

std::string DynamicArrayType::getDemangledName() const {
    return array_of->getDemangledName() + "[]";
}

bool DynamicArrayType::opApplies(std::string & op) const {
    const char * applicable[] = {"&", "@", "+", "-", "==", "!=", "=", "[]"};

    return s_in_a(op.c_str(), applicable);
}

bool DynamicArrayType::isValidOperand(const Type * operand,
                                      std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {"+", "-", "==", "!=", "="};
    const char * access[] = {"[]"};

    if (s_in_a(op.c_str(), same))
        return equivalent(operand);
    if (s_in_a(op.c_str(), access))
        return operand->enumerableEquivalent();
    return false;
}

const Type * DynamicArrayType::binResultType(const Type * operand,
                                             std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {"+", "-", "="};
    const char * boolean[] = {"==", "!="};
    const char * access[] = {"[]"};

    if (s_in_a(op.c_str(), same))
        return this;
    if (s_in_a(op.c_str(), boolean))
        return compilation->frontEnd.typeTable["bool"];
    if (s_in_a(op.c_str(), access))
        return array_of;
    return nullptr;
}

const Type * DynamicArrayType::unResultType(std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && unary(op.c_str()));
    const char * pointer[] = {"&"};

    if (s_in_a(op.c_str(), pointer))
        return pointerOf();
    return nullptr;
}

const Type * DynamicArrayType::replacePlaceholders(const Type * t) const {
    DynamicArrayType * result = new DynamicArrayType(*this);
    result->array_of = array_of->replacePlaceholders(t);
    return result;
}

PointerType::PointerType() : Type(POINTER) {}

PointerType::PointerType(const Type * _pointer_of)
    : Type(POINTER), pointer_of(_pointer_of) {
    code = pointer_of->code + "*";
}

bool PointerType::equivalent(const Type * other, bool exactMatch) const {
    if (other->isPointer())
        if (pointer_of->equivalent(((PointerType *)other)->pointer_of,
                                   exactMatch))
            return true;
    if (other->isArray() &&
        ((ArrayType *)other)->array_of->equivalent(pointer_of, exactMatch))
        return true;
    if (pointer_of->isStruct() && other->isPointer()) {
        const PointerType * other_p = (const PointerType *)other;
        if (other_p->pointer_of->isStruct()) {
            const StructType * my_t = (StructType *)pointer_of;
            const StructType * other_t = (StructType *)other_p->pointer_of;
            bool extends = false;
            while (my_t->extends) {
                my_t = (const StructType *)my_t->extends;
                if (my_t->equivalent(other_t, exactMatch)) {
                    extends = true;
                    break;
                }
            }
            if (!exactMatch)
                return extends;
        }
    }
    return false;
}

const Type * PointerType::getBase() const { return pointer_of->getBase(); }

Declarator * PointerType::getGenericDeclarator() const {
    PointerDeclarator * decl =
        new PointerDeclarator(pointer_of->getGenericDeclarator());
    decl->createdFromType = true;
    return decl;
}

std::string PointerType::getDemangledName() const {
    return pointer_of->getDemangledName() + "*";
}

bool PointerType::opApplies(std::string & op) const {
    const char * applicable[] = {"&", "@",
                                 // "+", "-",
                                 "==", "!=", "=", "[]"};

    return s_in_a(op.c_str(), applicable);
}

bool PointerType::isValidOperand(const Type * operand, std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {// "+", "-",
                           "==", "!=", "="};
    const char * access[] = {"[]"};

    if (s_in_a(op.c_str(), same))
        return equivalent(operand);
    if (s_in_a(op.c_str(), access))
        return operand->enumerableEquivalent();
    return false;
}

const Type * PointerType::binResultType(const Type * operand,
                                        std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {// "+", "-",
                           "="};
    const char * boolean[] = {"==", "!="};
    const char * access[] = {"[]"};

    if (s_in_a(op.c_str(), same))
        return this;
    if (s_in_a(op.c_str(), boolean))
        return compilation->frontEnd.typeTable["bool"];
    if (s_in_a(op.c_str(), access))
        return pointer_of;
    return nullptr;
}

const Type * PointerType::unResultType(std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && unary(op.c_str()));
    const char * pointer[] = {"&"};
    const char * deref[] = {"@"};

    if (s_in_a(op.c_str(), pointer))
        return pointerOf();
    if (s_in_a(op.c_str(), deref))
        return pointer_of;
    return nullptr;
}

const Type * PointerType::replacePlaceholders(const Type * t) const {
    PointerType * result = new PointerType(*this);
    result->pointer_of = pointer_of->replacePlaceholders(t);
    return result;
}

MaybeType::MaybeType() : Type(MAYBE) {}

MaybeType::MaybeType(const Type * _maybe_of)
    : Type(MAYBE), maybe_of(_maybe_of) {
    code = maybe_of->code + "?";
}

const Type * MaybeType::getBase() const { return maybe_of->getBase(); }

Declarator * MaybeType::getGenericDeclarator() const {
    MaybeDeclarator * decl =
        new MaybeDeclarator(maybe_of->getGenericDeclarator());
    decl->createdFromType = true;
    return decl;
}

std::string MaybeType::getDemangledName() const {
    return maybe_of->getDemangledName() + "?";
}

bool MaybeType::opApplies(std::string & op) const {
    const char * applicable[] = {"&", "==", "!=", "=", "?", "??"};

    return s_in_a(op.c_str(), applicable);
}

bool MaybeType::isValidOperand(const Type * operand, std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {"==", "!=", "="};
    const char * transfer[] = {"??"};

    if (s_in_a(op.c_str(), same))
        return equivalent(operand);
    if (s_in_a(op.c_str(), transfer))
        return operand->equivalent(maybe_of);
    return false;
}

const Type * MaybeType::binResultType(const Type * operand,
                                      std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {"="};
    const char * boolean[] = {"==", "!=", "??"};

    if (s_in_a(op.c_str(), same))
        return this;
    if (s_in_a(op.c_str(), boolean))
        return compilation->frontEnd.typeTable["bool"];
    return nullptr;
}

const Type * MaybeType::unResultType(std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && unary(op.c_str()));
    const char * pointer[] = {"&"};
    const char * good[] = {"?"};

    if (s_in_a(op.c_str(), pointer))
        return pointerOf();
    if (s_in_a(op.c_str(), good))
        return compilation->frontEnd.typeTable["bool"];
    return nullptr;
}

const Type * MaybeType::replacePlaceholders(const Type * t) const {
    MaybeType * result = new MaybeType(*this);
    result->maybe_of = maybe_of->replacePlaceholders(t);
    return result;
}

TupleType::TupleType() : Type(TUPLE) {}

TupleType::TupleType(std::vector<const Type *> _subTypes)
    : Type(TUPLE), subTypes(_subTypes) {
    code = "(";
    for (const Type * st : subTypes) {
        code += st->code;
        if (st != subTypes.back())
            code += st->code + ", ";
    }
    code += ")";
}

bool TupleType::equivalent(const Type * other, bool exactMatch) const {
    if (!other->isTuple())
        return false;
    const TupleType * other_tuple = (const TupleType *)other;
    if (subTypes.size() != other_tuple->subTypes.size())
        return false;
    for (unsigned int i = 0; i < subTypes.size(); i += 1)
        if (!subTypes[i]->equivalent(other_tuple->subTypes[i], exactMatch))
            return false;
    return true;
}

Declarator * TupleType::getGenericDeclarator() const {
    TupleDeclarator * decl = new TupleDeclarator();
    for (const Type * st : subTypes)
        decl->addSubDeclarator(st->getGenericDeclarator());
    decl->createdFromType = true;
    return decl;
}

std::string TupleType::getDemangledName() const {
    std::string demangled = "(";
    for (const Type * const & st : subTypes) {
        demangled += st->getDemangledName();
        if (&st != &subTypes.back())
            demangled += ", ";
    }
    demangled += ")";

    return demangled;
}

bool TupleType::opApplies(std::string & op) const {
    const char * applicable[] = {"&", "==", "!=", "=", "[]"};

    return s_in_a(op.c_str(), applicable);
}

bool TupleType::isValidOperand(const Type * operand, std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {"==", "!=", "="};
    const char * access[] = {"[]"};

    if (s_in_a(op.c_str(), same))
        return equivalent(operand);
    if (s_in_a(op.c_str(), access))
        return operand->enumerableEquivalent();
    return false;
}

const Type * TupleType::binResultType(const Type * operand,
                                      std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {"="};
    const char * boolean[] = {"==", "!="};
    const char * access[] = {"[]"};

    if (s_in_a(op.c_str(), same))
        return this;
    if (s_in_a(op.c_str(), boolean))
        return compilation->frontEnd.typeTable["bool"];
    if (s_in_a(op.c_str(), access))
        return new InvalidType(); // @fix -- what should we do here?
    return nullptr;
}

const Type * TupleType::unResultType(std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && unary(op.c_str()));
    const char * pointer[] = {"&"};

    if (s_in_a(op.c_str(), pointer))
        return pointerOf();
    return nullptr;
}

const Type * TupleType::replacePlaceholders(const Type * t) const {
    TupleType * result = new TupleType(*this);
    for (const Type *& s_t : result->subTypes)
        s_t = s_t->replacePlaceholders(t);
    return result;
}

ProcedureType::ProcedureType() : Type(PROCEDURE) {}

ProcedureType::ProcedureType(std::vector<const Type *> _paramTypes,
                             const Type * _retType, bool _isVararg)
    : Type(PROCEDURE), paramTypes(_paramTypes), isVararg(_isVararg),
      retType(_retType) {
    code = "<(";
    for (const Type * paramT : paramTypes) {
        code += paramT->code;
        if (&paramT != &paramTypes.back())
            code += ", ";
    }
    code += ")";
    if (retType->code != compilation->frontEnd.getBuiltinVoidTypeName())
        code += " : " + retType->code;
    code += ">";
}

bool ProcedureType::argMatch(const Type * _other, bool exactMatch) {
    if (_other->kind != PROCEDURE)
        return false;

    ProcedureType * other = (ProcedureType *)_other;

    size_t my_nparams = paramTypes.size();
    size_t other_nparams = other->paramTypes.size();

    size_t min_nparams =
        my_nparams <= other_nparams ? my_nparams : other_nparams;

    if (my_nparams < other_nparams && !isVararg)
        return false;
    if (other_nparams < my_nparams && !other->isVararg)
        return false;

    for (unsigned int i = 0; i < min_nparams; i += 1)
        if (!paramTypes[i]->equivalent(other->paramTypes[i], exactMatch))
            return false;
    return true;
}

bool ProcedureType::equivalent(const Type * _other, bool exactMatch) const {
    if (!_other->isProcedure())
        return false;
    ProcedureType * other = (ProcedureType *)_other;
    if (paramTypes.size() != other->paramTypes.size())
        return false;
    for (unsigned int i = 0; i < paramTypes.size(); i += 1)
        if (!paramTypes[i]->equivalent(other->paramTypes[i], exactMatch))
            return false;
    if (!retType->equivalent(other->retType, exactMatch))
        return false;
    return true;
}

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
    if (retType->getDemangledName() !=
        compilation->frontEnd.getBuiltinVoidTypeName())
        demangled += " : " + retType->getDemangledName();
    demangled += ">";

    return demangled;
}

bool ProcedureType::opApplies(std::string & op) const {
    const char * applicable[] = {"&", "==", "!=", "=", "()"};

    return s_in_a(op.c_str(), applicable);
}

bool ProcedureType::isValidOperand(const Type * operand,
                                   std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    BJOU_DEBUG_ASSERT(
        op != "()" &&
        "Operand for call must be ArgList, which does not have a type.");
    const char * same[] = {"==", "!=", "="};

    if (s_in_a(op.c_str(), same))
        return equivalent(operand);
    return false;
}

const Type * ProcedureType::binResultType(const Type * operand,
                                          std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && binary(op.c_str()));
    const char * same[] = {"="};
    const char * boolean[] = {"==", "!="};

    if (s_in_a(op.c_str(), same))
        return this;
    if (s_in_a(op.c_str(), boolean))
        return compilation->frontEnd.typeTable["bool"];
    return nullptr;
}

const Type * ProcedureType::unResultType(std::string & op) const {
    BJOU_DEBUG_ASSERT(opApplies(op) && unary(op.c_str()));
    const char * pointer[] = {"&"};

    if (s_in_a(op.c_str(), pointer))
        return pointerOf();
    return nullptr;
}

const Type * ProcedureType::replacePlaceholders(const Type * t) const {
    ProcedureType * result = new ProcedureType(*this);
    for (const Type *& p_t : result->paramTypes)
        p_t = p_t->replacePlaceholders(t);
    result->retType = result->retType->replacePlaceholders(t);
    return result;
}

TemplateStructType::TemplateStructType() : Type(TEMPLATE_STRUCT) {}

TemplateStructType::TemplateStructType(std::string & name)
    : Type(TEMPLATE_STRUCT, name) {}

TemplateAliasType::TemplateAliasType() : Type(TEMPLATE_ALIAS) {}

TemplateAliasType::TemplateAliasType(std::string & name)
    : Type(TEMPLATE_ALIAS, name) {}

void compilationAddPrimativeTypes() {
    const std::tuple<const char *, Type::Sign, int> primatives[] = {

        std::make_tuple("u1", Type::UNSIGNED, 1),
        std::make_tuple("u8", Type::UNSIGNED, 8),
        std::make_tuple("i8", Type::SIGNED, 8),
        std::make_tuple("u16", Type::UNSIGNED, 16),
        std::make_tuple("i16", Type::SIGNED, 16),
        std::make_tuple("u32", Type::UNSIGNED, 32),
        std::make_tuple("i32", Type::SIGNED, 32),
        std::make_tuple("u64", Type::UNSIGNED, 64),
        std::make_tuple("i64", Type::SIGNED, 64),
        std::make_tuple("f32", Type::NA, 32),
        std::make_tuple("f64", Type::NA, 64),
        std::make_tuple("void", Type::NA, -1),
        std::make_tuple("bool", Type::UNSIGNED, 1),
        std::make_tuple("int", Type::SIGNED, 32),
        std::make_tuple("uint", Type::UNSIGNED, 32),
        std::make_tuple("long", Type::SIGNED, 64),
        std::make_tuple("ulong", Type::UNSIGNED, 64),
        std::make_tuple("char", Type::SIGNED, 8),
        std::make_tuple("short", Type::SIGNED, 16),
        std::make_tuple("float", Type::NA, 32),
        std::make_tuple("double", Type::NA, 64)};

    for (auto & p : primatives)
        compilation->frontEnd.primativeTypeTable[std::get<0>(p)] = new Type(
            Type::PRIMATIVE, std::get<0>(p), std::get<1>(p), std::get<2>(p));
}

const Type * primativeConversionResult(const Type * l, const Type * r) {
    // one of l and r is fp, then we choose the fp type
    if (l->isFP() && !r->isFP())
        return l;
    if (!l->isFP() && r->isFP())
        return r;
    // if both, choose the larger
    if (l->isFP() && r->isFP()) {
        if (l->size >= r->size)
            return l;
        else
            return r;
    }

    // both integer types
    Type::Sign sign = Type::Sign::UNSIGNED;
    if (l->sign == Type::Sign::SIGNED || r->sign == Type::Sign::SIGNED)
        sign = Type::Sign::SIGNED;
    int size = l->size >= r->size ? l->size : r->size;

    if (size == -1)
        return compilation->frontEnd.typeTable["void"];

    char buf[32];
    sprintf(buf, "%c%d", (sign == Type::Sign::SIGNED ? 'i' : 'u'), size);

    BJOU_DEBUG_ASSERT(compilation->frontEnd.typeTable.count(buf) > 0);

    return compilation->frontEnd.typeTable[buf];
}

void createCompleteType(Symbol * sym) {
    if (false) {
    } else if (false) {
    } else if (false) {
    }
}

// @fun
std::vector<const Type *>
typesSortedByDepencencies(std::vector<const Type *> nonPrimatives) {
    size_t size = nonPrimatives.size();
    std::vector<const Type *> sorted, keep;
    std::vector<std::string> availableByCode;
    std::vector<int> indices;
    int idx, nadded;
    sorted.reserve(size);
    keep.reserve(size);
    availableByCode.reserve(size);
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
                    Declarator * decl = (Declarator *)mem->getTypeDeclarator();
                    // declarators with a pointer ANYWHERE in them imply that we
                    // don't need to know the size of the base type
                    // PointerDeclarator handles this on construction and sets a
                    // flag in their respective base Declarator
                    if (decl &&
                        decl->getBase()->getFlag(Declarator::IMPLIES_COMPLETE))
                        terms.push_back(decl);

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
            } else if (t->isAlias()) {
                // @todo
                // what should happen here is that we should have access to the
                // underlying declarator and unwrap that
            }
            for (ASTNode * term : terms) {
                const Type * t = term->getType(); // @leak?
                BJOU_DEBUG_ASSERT(t->isValid());
                if (t->getBase()->isStruct() || t->getBase()->isAlias()) {
                    // has the type that this term needs already been placed in
                    // 'sorted'?
                    if (std::find(availableByCode.begin(),
                                  availableByCode.end(), t->getBase()->code) ==
                        availableByCode.end()) {
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
                availableByCode.push_back(nonPrimatives[i]->code);
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
            } else if (nonPrimatives[0]->isAlias()) {
                // @todo add suport for aliases
            }
            std::string demangledA = demangledString(nonPrimatives[0]->code);
            std::string demangledB =
                demangledString(badTerm->getType()->getBase()->code); // @leak
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
                       "Did you mean to reference '" + demangledB +
                           "' by pointer?");
        }
    }

    return sorted;
}

} // namespace bjou
