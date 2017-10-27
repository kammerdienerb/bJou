//
//  Scope.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "Scope.hpp"
#include "CLI.hpp"
#include "Compile.hpp"
#include "FrontEnd.hpp"
#include "Global.hpp"
#include "Misc.hpp"
#include "Symbol.hpp"

namespace bjou {
Scope::Scope(std::string _description, Scope * _parent, bool _nspace)
    : nspace(_nspace), description(_description), parent(_parent) {}

std::string Scope::mangledPrefix() const { return ""; }

Scope::~Scope() {
    for (Scope * scope : scopes)
        delete scope;
    for (auto & symbol : symbols)
        delete symbol.second;
}

NamespaceScope::NamespaceScope(std::string _name, Scope * _parent)
    : Scope("namespace " + _name, _parent, true), name(_name) {}

std::string NamespaceScope::mangledPrefix() const {
    std::string mangled;
    if (parent && parent->nspace)
        mangled = parent->mangledPrefix();
    mangled += "N" + std::to_string(name.size()) + name;
    return mangled;
}

NamespaceScope::~NamespaceScope() {}

static void checkUninitialized(Scope * startingScope, Symbol * sym,
                               Context & context) {
    if (sym->isVar()) {
        if (sym->referenced) {
            Scope * s = startingScope;
            while (s) {
                if (sym->initializedInScopes.find(s) !=
                    sym->initializedInScopes.end())
                    return;
                s = s->parent;
            }
            errorl(context,
                   "'" + sym->demangledString() +
                       "' might be uninitialized when referenced here.");
        }
    }
}

Maybe<Symbol *> Scope::getSymbol(Scope * startingScope, ASTNode * _identifier,
                                 Context * context, bool traverse, bool fail,
                                 bool checkUninit) {
    Identifier * identifier = (Identifier *)_identifier;

    // if (!identifier->qualified.empty())
    // return getSymbol(identifier->qualified, context, traverse, fail);

    std::string & u = identifier->getUnqualified();

    if (identifier->namespaces.empty()) {
        if (symbols.count(u) > 0) {
            if (checkUninit)
                checkUninitialized(startingScope, symbols[u], *context);
            symbols[u]->referenced = true;
            return Maybe<Symbol *>(symbols[u]);
        }
        if (traverse && parent)
            return parent->getSymbol(startingScope, _identifier, context,
                                     traverse, fail, checkUninit);
    }
    Scope * scope = compilation->frontEnd.globalScope;
    for (std::string & nspaceName : identifier->getNamespaces()) {
        if (scope->namespaces.count(nspaceName))
            scope = scope->namespaces[nspaceName];
        else
            goto end;
    }
    if (scope->symbols.count(u)) {
        if (checkUninit)
            checkUninitialized(startingScope, scope->symbols[u], *context);
        scope->symbols[u]->referenced = true;
        return Maybe<Symbol *>(scope->symbols[u]);
    }
end:
    if (fail) {
        std::string reportName = mangledIdentifier(identifier);
        if (context)
            errorl(*context,
                   "Use of undeclared identifier '" + reportName + "'.");
        else
            internalError("getSymbol('" + reportName +
                          "') failed without a context.");
    }

    return Maybe<Symbol *>();
}

Maybe<Symbol *> Scope::getSymbol(Scope * startingScope,
                                 std::string & qualifiedIdentifier,
                                 Context * context, bool traverse, bool fail,
                                 bool checkUninit) {
    if (symbols.count(qualifiedIdentifier) > 0) {
        if (checkUninit)
            checkUninitialized(startingScope, symbols[qualifiedIdentifier],
                               *context);
        symbols[qualifiedIdentifier]->referenced = true;
        return Maybe<Symbol *>(symbols[qualifiedIdentifier]);
    }
    if (traverse && parent)
        return parent->getSymbol(startingScope, qualifiedIdentifier, context,
                                 traverse, fail, checkUninit);
    if (fail) {
        if (context)
            errorl(*context, "Use of undeclared identifier '" +
                                 qualifiedIdentifier + "'.");
        else
            internalError("getSymbol('" + qualifiedIdentifier +
                          "') failed without a context.");
    }

    return Maybe<Symbol *>();
}

void Scope::addSymbol(Symbol * symbol, Context * context) {
    if ((!parent || nspace) && symbol->name[symbol->name.size() - 1] == '\'')
        errorl(symbol->node()->getNameContext(),
               "Global symbols using the prime notation are disallowed to "
               "maintain cross-language compatibility.",
               true, "fix: remove trailing '");

    Symbol * mangled = symbol->mangled(this);
    Maybe<Symbol *> m_existing;
    if (nspace)
        m_existing = getSymbol(this, symbol->name, nullptr, false, false);
    else
        m_existing = getSymbol(this, symbol->name, nullptr, true, false);
    Symbol * existing = nullptr;
    if (m_existing.assignTo(existing) &&
        !symbol->node()->getFlag(ASTNode::SYMBOL_OVERWRITE)) {
        if (existing->node()->nodeKind == ASTNode::PROC_SET) {
            ProcSet * procSet = (ProcSet *)existing->node();
            for (auto & psym : procSet->procs) {
                if (psym.second->node()->getScope() == this) {
                    errorl(*context,
                           "Redefinition of '" + mangled->demangledString() +
                               "' as a different kind of symbol.",
                           false);
                    Context econtext = psym.second->node()->getNameContext();
                    errorl(econtext, "'" + psym.second->demangledString() +
                                         "' defined here.");
                }
            }
        } else {
            errorl(*context,
                   "Redefinition of '" + mangled->demangledString() + "'.",
                   false);
            Context econtext = existing->node()->getNameContext();
            errorl(econtext,
                   "'" + existing->demangledString() + "' also defined here.");
        }
    }
    if (symbol->isVar() &&
        symbol->node()->getFlag(VariableDeclaration::IS_PROC_PARAM))
        mangled->initializedInScopes.insert(this);
    symbols[symbol->name] = mangled;
}

void Scope::addSymbol(_Symbol<Procedure> * symbol, Context * context) {
    if ((!parent || nspace) && symbol->name[symbol->name.size() - 1] == '\'')
        errorl(symbol->node()->getNameContext(),
               "Global symbols using the prime notation are disallowed to "
               "maintain cross-language compatibility.",
               true, "fix: remove trailing '");

    Procedure * proc = (Procedure *)symbol->node();
    Symbol * mangled = symbol->mangled(this);

    std::string name;
    if (proc->getFlag(Procedure::IS_TYPE_MEMBER)) {
        Struct * parent = proc->getParentStruct();

        BJOU_DEBUG_ASSERT(parent);

        name = parent->getMangledName() + ".";
    }
    name += symbol->name;

    Maybe<Symbol *> m_existing = getSymbol(this, name, nullptr, false, false);
    Symbol * existing = nullptr;
    if (m_existing.assignTo(existing) &&
        existing->node()->nodeKind != ASTNode::PROC_SET) {
        if (existing->node()->getFlag(ASTNode::SYMBOL_OVERWRITE)) {
            existing->node()->setFlag(ASTNode::IGNORE_GEN, true);
        } else {
            errorl(*context,
                   "Redefinition of '" + mangled->demangledString() +
                       "' as a different kind of symbol.",
                   false);
            errorl(existing->node()->getNameContext(),
                   "'" + existing->demangledString() + "' defined here.");
        }
    }

    Maybe<Symbol *> m_mangled_existing =
        getSymbol(this, mangled->name, nullptr, false, false);
    Symbol * mangled_existing = nullptr;
    if (m_mangled_existing.assignTo(mangled_existing)) {
        if (mangled_existing->node()->getFlag(ASTNode::SYMBOL_OVERWRITE)) {
            mangled_existing->node()->setFlag(ASTNode::IGNORE_GEN, true);
        } else if (mangled_existing->node()->nodeKind == ASTNode::PROC_SET &&
                   mangled->node()->getFlag(Procedure::IS_EXTERN)) {
            // @incomplete
            // Create a mechanism to catch conflicting redefinitions of extern
            // procs
        } else {
            errorl(*context,
                   "Redefinition of '" + mangled->demangledString() + "'.",
                   false);
            errorl(mangled_existing->node()->getNameContext(),
                   "'" + mangled_existing->demangledString() +
                       "' also defined here.");
        }
    }

    // only create a ProcSet if it doesn't clobber another symbol

    // global
    if (!parent || (nspace && !parent->parent)) {
        // global ProcSets are only concerned with one layer of namespace
        // deduction
        if (compilation->frontEnd.globalScope->symbols.count(name) == 0)
            compilation->frontEnd.globalScope->symbols[name] =
                new _Symbol<ProcSet>(name, new ProcSet(name));

        if (compilation->frontEnd.globalScope->symbols[name]
                ->node()
                ->nodeKind == ASTNode::PROC_SET) {
            ProcSet * set =
                (ProcSet *)compilation->frontEnd.globalScope->symbols[name]
                    ->node();
            set->procs[mangled->name] = mangled;
        }

        // also create another proc set unique to the type/interface combo
        if (proc->getFlag(Procedure::IS_INTERFACE_IMPL)) {
            std::string iname = proc->getParentStruct()->getMangledName() + ".";
            Identifier * ident =
                (Identifier *)((InterfaceImplementation *)proc->parent)
                    ->getIdentifier();
            iname += mangledIdentifier(ident) + "." + proc->getName();

            if (compilation->frontEnd.globalScope->symbols.count(iname) == 0)
                compilation->frontEnd.globalScope->symbols[iname] =
                    new _Symbol<ProcSet>(iname, new ProcSet(iname));

            if (compilation->frontEnd.globalScope->symbols[iname]
                    ->node()
                    ->nodeKind == ASTNode::PROC_SET) {
                ProcSet * set =
                    (ProcSet *)compilation->frontEnd.globalScope->symbols[iname]
                        ->node();
                set->procs[mangled->name] = mangled;
            }
        }
    }

    // local
    if (nspace) {
        if (symbols.count(name) == 0)
            symbols[name] = new _Symbol<ProcSet>(name, new ProcSet(name));
        if (symbols[name]->node()->nodeKind == ASTNode::PROC_SET) {
            ProcSet * set = (ProcSet *)symbols[name]->node();
            set->procs[mangled->name] = mangled;
        }

        // also create another proc set unique to the type/interface combo
        if (proc->getFlag(Procedure::IS_INTERFACE_IMPL)) {
            std::string iname = proc->getParentStruct()->getMangledName() + ".";
            Identifier * ident =
                (Identifier *)((InterfaceImplementation *)proc->parent)
                    ->getIdentifier();
            iname += mangledIdentifier(ident) + "." + proc->getName();

            if (symbols.count(name) == 0)
                symbols[iname] =
                    new _Symbol<ProcSet>(iname, new ProcSet(iname));
            if (symbols[iname]->node()->nodeKind == ASTNode::PROC_SET) {
                ProcSet * set = (ProcSet *)symbols[iname]->node();
                set->procs[mangled->name] = mangled;
            }
        }
    }

    // if it's extern and the mangled name is the same, we don't want to clobber
    // the set
    if (mangled->name != name)
        symbols[mangled->name] = mangled;
}

void Scope::addSymbol(_Symbol<TemplateProc> * symbol, Context * context) {
    if ((!parent || nspace) && symbol->name[symbol->name.size() - 1] == '\'')
        errorl(symbol->node()->getNameContext(),
               "Global symbols using the prime notation are disallowed to "
               "maintain cross-language compatibility.",
               true, "fix: remove trailing '");

    TemplateProc * template_proc = (TemplateProc *)symbol->node();
    // Procedure * proc = (Procedure*)template_proc->getTemplate();
    Symbol * mangled = symbol->mangled(this);

    std::string name;

    if (template_proc->getFlag(TemplateProc::IS_TYPE_MEMBER)) {
        Struct * s = (Struct *)template_proc->parent;
        name = s->getMangledName() + ".";
    }
    name += symbol->name;

    Maybe<Symbol *> m_existing = getSymbol(this, name, nullptr, false, false);
    Symbol * existing = nullptr;
    if (m_existing.assignTo(existing) &&
        existing->node()->nodeKind != ASTNode::PROC_SET) {
        if (existing->node()->getFlag(ASTNode::SYMBOL_OVERWRITE)) {
            existing->node()->setFlag(ASTNode::IGNORE_GEN, true);
        } else {
            errorl(*context,
                   "Redefinition of '" + mangled->demangledString() +
                       "' as a different kind of symbol.",
                   false);
            errorl(existing->node()->getNameContext(),
                   "'" + existing->demangledString() + "' defined here.");
        }
    }

    Maybe<Symbol *> m_mangled_existing =
        getSymbol(this, mangled->name, nullptr, false, false);
    Symbol * mangled_existing = nullptr;
    if (m_mangled_existing.assignTo(mangled_existing)) {
        if (mangled_existing->node()->getFlag(ASTNode::SYMBOL_OVERWRITE)) {
            mangled_existing->node()->setFlag(ASTNode::IGNORE_GEN, true);
        } else {
            errorl(*context,
                   "Redefinition of '" + mangled->demangledString() + "'.",
                   false);
            errorl(mangled_existing->node()->getNameContext(),
                   "'" + mangled_existing->demangledString() +
                       "' also defined here.");
        }
    }

    // only create a ProcSet if it doesn't clobber another symbol

    // global
    if (!parent || (nspace && !parent->parent)) {
        // global ProcSets are only concerned with one layer of namespace
        // deduction
        if (compilation->frontEnd.globalScope->symbols.count(name) == 0)
            compilation->frontEnd.globalScope->symbols[name] =
                new _Symbol<ProcSet>(name, new ProcSet(name));
        if (compilation->frontEnd.globalScope->symbols[name]
                ->node()
                ->nodeKind == ASTNode::PROC_SET) {
            ProcSet * set =
                (ProcSet *)compilation->frontEnd.globalScope->symbols[name]
                    ->node();
            set->procs[mangled->name] = mangled;
        }
    }

    // local
    if (nspace) {
        if (symbols.count(name) == 0)
            symbols[name] = new _Symbol<ProcSet>(name, new ProcSet(name));
        if (symbols[name]->node()->nodeKind == ASTNode::PROC_SET) {
            ProcSet * set = (ProcSet *)symbols[name]->node();
            set->procs[mangled->name] = mangled;
        }
    }

    symbols[mangled->name] = mangled;
}

void createProcSetsForPuntedInterfaceImpl(Struct * s,
                                          InterfaceImplementation * impl) {
    Identifier * ident = (Identifier *)impl->getIdentifier();
    Maybe<Symbol *> m_sym = s->getScope()->getSymbol(s->getScope(), ident);
    Symbol * sym = nullptr;
    m_sym.assignTo(sym);
    BJOU_DEBUG_ASSERT(sym);
    BJOU_DEBUG_ASSERT(sym->isInterface());

    InterfaceDef * idef = (InterfaceDef *)sym->node();

    for (auto & procs : idef->getProcs()) {
        std::string name = s->getMangledName() + "." + procs.first;
        compilation->frontEnd.globalScope->symbols[name] =
            new _Symbol<ProcSet>(name, new ProcSet(name));
    }
}

void createProcSetForInheritedInterfaceImpl(Struct * s,
                                            InterfaceImplementation * impl) {
    Identifier * ident = (Identifier *)impl->getIdentifier();
    Maybe<Symbol *> m_sym = s->getScope()->getSymbol(s->getScope(), ident);
    Symbol * sym = nullptr;
    m_sym.assignTo(sym);
    BJOU_DEBUG_ASSERT(sym);
    BJOU_DEBUG_ASSERT(sym->isInterface());

    BJOU_DEBUG_ASSERT(impl->parent &&
                      impl->parent->nodeKind == ASTNode::STRUCT);

    Struct * implementor_struct = (Struct *)impl->parent;
    InterfaceDef * idef = (InterfaceDef *)sym->node();

    for (auto & procs : idef->getProcs()) {
        std::string name = s->getMangledName() + "." + procs.first;
        ProcSet * set = new ProcSet(name);
        compilation->frontEnd.globalScope->symbols[name] =
            new _Symbol<ProcSet>(name, set);

        std::string lookup =
            implementor_struct->getMangledName() + "." + procs.first;
        Maybe<Symbol *> m_sym = implementor_struct->getScope()->getSymbol(
            implementor_struct->getScope(), lookup);
        Symbol * sym = nullptr;
        m_sym.assignTo(sym);
        BJOU_DEBUG_ASSERT(sym);
        BJOU_DEBUG_ASSERT(sym->isProcSet());

        ProcSet * existing_set = (ProcSet *)sym->node();
        set->procs = existing_set->procs;
    }
}

void Scope::printSymbols(int indent) const {
    static const char * side = "\xE2\x95\x91";
    printf("%s %-76s %s\n", side,
           (std::string(indent * 2, ' ') + "*** " + description + ":").c_str(),
           side);
    for (auto & sym : symbols)
        if (sym.second->node()->nodeKind != ASTNode::PROC_SET)
            sym.second->tablePrint(indent);
    for (Scope * scope : scopes)
        scope->printSymbols(indent + 1);
}

void printSymbolTables() {
    static const char * top = "\xE2\x95\x90";
    static const char * tl_corner = "\xE2\x95\x94";
    static const char * tr_corner = "\xE2\x95\x97";
    static const char * bl_corner = "\xE2\x95\x9A";
    static const char * br_corner = "\xE2\x95\x9D";
    static const char * side = "\xE2\x95\x91";
    static const char * l_side_connect = "\xE2\x95\x9F";
    static const char * r_side_connect = "\xE2\x95\xA2";
    static const char * thin_hor = "\xE2\x94\x80";

    printf("%s", tl_corner);
    for (int i = 0; i < 78; i += 1)
        printf("%s", top);
    printf("%s", tr_corner);
    printf("\n");
    printf("%s%-35sSymbols%36s%s\n", side, "", "", side);
    printf("%s", l_side_connect);
    for (int i = 0; i < 78; i += 1)
        printf("%s", thin_hor);
    printf("%s", r_side_connect);
    printf("\n");
    compilation->frontEnd.globalScope->printSymbols(0);
    printf("%s", bl_corner);
    for (int i = 0; i < 78; i += 1)
        printf("%s", top);
    printf("%s", br_corner);
    printf("\n");
}
} // namespace bjou
