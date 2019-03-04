//  Symbol.hpp
//  bjou
//
//  January, 2019

#ifndef Symbol_hpp
#define Symbol_hpp

#include "ASTNode.hpp"
#include "Context.hpp"
#include "Maybe.hpp"
#include "Scope.hpp"
#include "bJouDemangle.h"
#include "std_string_hasher.hpp"
#include "hybrid_map.hpp"

#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <stdint.h>

namespace bjou {
struct Symbol;

// ProcSet inherits from ASTNode only so that we can have symbols to it.
// ProcSets are NOT part of the AST
struct ProcSet : ASTNode {
    ProcSet();
    ProcSet(std::string _name);

    /* std::unordered_map<std::string, Symbol *> procs; */
    hybrid_map<std::string, Symbol*, std_string_hasher> procs;
    std::string name;

    // Node interface
    void analyze(bool force = false);
    ASTNode * clone();
    void addSymbols(std::string& _mod, Scope * scope);
    ~ProcSet();
    //

    Procedure * get(Scope * scope, ASTNode * args = nullptr, ASTNode * inst = nullptr,
                    Context * context = nullptr, bool fail = true);
    Procedure * getTemplate(std::vector<const Type *> & arg_types,
                            ASTNode * args, ASTNode * inst, Context * context,
                            bool fail);
    std::vector<Symbol *> getCandidates(Scope * scope, ProcedureType * compare_type,
                                        ASTNode * args, ASTNode * inst,
                                        Context * context, bool fail, bool set_is_in_module);
    bool resolve(std::vector<Symbol *> & candidates,
                 std::vector<Symbol *> & resolved, ProcedureType * compare_type,
                 ASTNode * args, ASTNode * inst, Context * context, bool fail);
    void showCandidatesError(std::vector<Symbol *> & candidates,
                             Context * context);
};

std::string mangledIdentifier(Identifier * ident);
std::string demangledIdentifier(Identifier * ident);
Identifier * stringToIdentifier(std::string mangled);
Identifier * stringToIdentifier(std::string m, std::string t, std::string n);
Maybe<std::string> get_mod_from_string(const std::string& s);
std::string string_sans_mod(const std::string& s);

struct Symbol {
    Symbol(ASTNode * __node);
    virtual ~Symbol();

    std::string
        unmangled,
        proc_name,
        template_pos_string,
        real_mangled;
    ASTNode
        *_node;
    std::set<Scope *>
        initializedInScopes;
    bool
        referenced;

    virtual bool isProc()         const = 0;
    virtual bool isTemplateProc() const = 0;
    virtual bool isProcSet()      const = 0;
    virtual bool isConstant()     const = 0;
    virtual bool isVar()          const = 0;
    virtual bool isType()         const = 0;
    virtual bool isTemplateType() const = 0;
    virtual bool isAlias()        const = 0;

    ASTNode * node()              const;
    void tablePrint(int indent);
};

template <typename T> struct _Symbol : Symbol {
    _Symbol(std::string name, std::string mod, std::string type, ASTNode * __node, ASTNode * _inst, ASTNode * _def)
        : Symbol(__node) {

        BJOU_DEBUG_ASSERT(!name.empty() && "can't create symbol without a name");
        BJOU_DEBUG_ASSERT(!_def);

        { /* make the unmangled symbol */
            const char * lazy_comma = "";
            if (!mod.empty()) {
                unmangled += mod + "::";
            }
            if (!type.empty()) {
                unmangled += type + ".";
            }
            unmangled += name;
            if (_inst) {
                TemplateInstantiation * inst = (TemplateInstantiation*)_inst;
                BJOU_DEBUG_ASSERT(!_def);
                BJOU_DEBUG_ASSERT(inst->getElements().size() > 0);
                unmangled += "$(";
                for (ASTNode * elem : inst->getElements()) {
                    unmangled += lazy_comma + elem->getType()->getDemangledName();
                    lazy_comma = ", ";
                }
                unmangled += ")";
            }
        }

        { /* make the real_mangled symbol */
            uint64_t real_hash = bJouDemangle_hash(unmangled.c_str()); 
            char buff[128];
            bJouDemangle_u642b32(real_hash, buff);
            std::string b32(buff);
            real_mangled = name + "_" + b32;
        }
    }
    
    _Symbol(std::string name, std::string mod, std::string type, ASTNode * __node)
        : _Symbol<T>(name, mod, type, __node, nullptr, nullptr) { }
    _Symbol(std::string _proc_name, ProcSet * set)
        : _Symbol<T>(_proc_name, "", "", set, nullptr, nullptr) {

        proc_name = proc_name;
    }

    inline bool isProc() const { return false; }
    inline bool isTemplateProc() const { return false; }
    inline bool isProcSet() const { return false; }
    inline bool isConstant() const { return false; }
    inline bool isVar() const { return false; }
    inline bool isType() const { return false; }
    inline bool isTemplateType() const { return false; }
    inline bool isAlias() const { return false; }
};

template <> inline _Symbol<Procedure>::_Symbol(std::string name, std::string mod, std::string type, ASTNode * __node, ASTNode * _inst, ASTNode * _def)
        : Symbol(__node) {

    BJOU_DEBUG_ASSERT(!_def);

    Procedure * proc = (Procedure*)__node;

    { /* make the unmangled symbol */
        const char * lazy_comma = "";
        if (!mod.empty()) {
            unmangled += mod + "::";
        }
        if (!type.empty()) {
            unmangled += type + ".";
        }
        unmangled += name;

        if (proc->getFlag(Procedure::IS_EXTERN)
        ||  proc->getFlag(Procedure::NO_MANGLE)) {
            BJOU_DEBUG_ASSERT(type.empty());
            proc_name = name;
        } else {
            proc_name = unmangled;
        }
        
        if (_inst) {
            TemplateInstantiation * inst = (TemplateInstantiation*)_inst;
            BJOU_DEBUG_ASSERT(inst->getElements().size() > 0);
            unmangled += "$(";
            for (ASTNode * elem : inst->getElements()) {
                unmangled += lazy_comma + elem->getType()->getDemangledName();
                lazy_comma = ", ";
            }
            unmangled += ")";
        }

        lazy_comma = "";
        unmangled += "(";
        for (ASTNode * param : proc->getParamVarDeclarations()) {
            if (param->nodeKind == ASTNode::THIS) {
                unmangled += lazy_comma;
                unmangled += "this";
            } else {
                VariableDeclaration * var = (VariableDeclaration*)param;
                Declarator * decl = (Declarator*)var->getTypeDeclarator();
                unmangled += lazy_comma + decl->asString();
            }

            lazy_comma = ", ";
        }
        if (proc->getFlag(Procedure::IS_VARARG))
            unmangled += std::string(lazy_comma) + "...";
        unmangled += ")";
    }

    { /* make the real_mangled symbol */
        uint64_t real_hash = bJouDemangle_hash(unmangled.c_str()); 
        char buff[128];
        bJouDemangle_u642b32(real_hash, buff);
        std::string b32(buff);
        real_mangled = name + "_" + b32;
    }
}

template <> inline _Symbol<TemplateProc>::_Symbol(std::string name, std::string mod, std::string type, ASTNode * __node, ASTNode * _inst, ASTNode * _def)
        : Symbol(__node) {

    BJOU_DEBUG_ASSERT(_def && !_inst);

    TemplateProc * tproc = (TemplateProc*)__node;
    Procedure * proc = (Procedure*)tproc->_template;

    { /* make the unmangled symbol */
        const char * lazy_comma = "";
        if (!mod.empty()) {
            unmangled += mod + "::";
        }
        if (!type.empty()) {
            unmangled += type + ".";
        }

        unmangled += name;
        proc_name = unmangled;

        unmangled = "template " + unmangled;

        TemplateDefineList * def = (TemplateDefineList*)_def;
        BJOU_DEBUG_ASSERT(def->getElements().size() > 0);
        unmangled += "$(";
        for (ASTNode * _elem : def->getElements()) {
            TemplateDefineElement * elem = (TemplateDefineElement*)_elem; 
            unmangled += lazy_comma + elem->getName();
            lazy_comma = ", ";
        }
        unmangled += ")";

        lazy_comma = "";
        unmangled += "(";
        for (ASTNode * param : proc->getParamVarDeclarations()) {
            if (param->nodeKind == ASTNode::THIS) {
                unmangled += lazy_comma;
                unmangled += "this";
            } else {
                VariableDeclaration * var = (VariableDeclaration*)param;
                Declarator * decl = (Declarator*)var->getTypeDeclarator();
                unmangled += lazy_comma + decl->asString();
            }
            lazy_comma = ", ";
        }
        unmangled += ")";
    }

    { /* make the template_pos_string */
        const char * lazy_comma = "";
        if (!mod.empty()) {
            template_pos_string += mod + "::";
        }
        if (!type.empty()) {
            template_pos_string += type + ".";
        }
        template_pos_string += name;

        TemplateDefineList * def = (TemplateDefineList*)_def;

        template_pos_string += std::to_string(def->getElements().size());

        std::vector<std::string> elem_names;
        for (ASTNode * _elem : def->getElements()) {
            TemplateDefineElement * elem = (TemplateDefineElement*)_elem;
            elem_names.push_back(elem->getName());
        }
        
        lazy_comma = "";
        template_pos_string += "(";
        for (ASTNode * param : proc->getParamVarDeclarations()) {
            if (param->nodeKind == ASTNode::THIS) {
                template_pos_string += lazy_comma;
                template_pos_string += "this";
            } else {
                VariableDeclaration * var = (VariableDeclaration*)param;
                Declarator * decl = (Declarator*)var->getTypeDeclarator()->clone();
                std::vector<ASTNode*> terms;
                decl->unwrap(terms);
                for (ASTNode * term : terms) {
                    if (term->nodeKind == ASTNode::IDENTIFIER) {
                        Identifier * iden = (Identifier*)term;
                        for (int i = 0; i < elem_names.size(); i += 1) {
                            if (iden->symAll() == elem_names[i]) {
                                iden->setSymName(std::to_string(i));
                            }
                        }
                    }
                }
                template_pos_string += lazy_comma + decl->asString();
                delete decl;
            }
            lazy_comma = ", ";
        }
        template_pos_string += ")";
    }

    { /* make the real_mangled symbol */
        uint64_t real_hash = bJouDemangle_hash(unmangled.c_str()); 
        char buff[128];
        bJouDemangle_u642b32(real_hash, buff);
        std::string b32(buff);
        real_mangled = name + "_" + b32;
    }
}

struct TmpMangler : _Symbol<void> {
    TmpMangler(std::string name, std::string mod, std::string type)
        : _Symbol<void>(name, mod, type, nullptr, nullptr, nullptr) { }
    TmpMangler(std::string name, std::string mod, std::string type, ASTNode * _inst, ASTNode * _def)
        : _Symbol<void>(name, mod, type, nullptr, _inst, _def) { }
};

/* kind getters */
template <> inline bool _Symbol<Procedure>::isProc() const { return true; }
template <> inline bool _Symbol<TemplateProc>::isTemplateProc() const {
    return true;
}
template <> inline bool _Symbol<ProcSet>::isProcSet() const { return true; }
template <> inline bool _Symbol<Constant>::isConstant() const { return true; }
template <> inline bool _Symbol<VariableDeclaration>::isVar() const {
    return true;
}
template <> inline bool _Symbol<Struct>::isType() const { return true; }
template <> inline bool _Symbol<Alias>::isType() const { return true; }
template <> inline bool _Symbol<Enum>::isType() const { return true; }
template <> inline bool _Symbol<TemplateStruct>::isTemplateType() const {
    return true;
}
template <> inline bool _Symbol<TemplateAlias>::isTemplateType() const {
    return true;
}
template <> inline bool _Symbol<Alias>::isAlias() const { 
    return true;
}
/* end kind getters */
} // namespace bjou

#endif /* Symbol_hpp */
