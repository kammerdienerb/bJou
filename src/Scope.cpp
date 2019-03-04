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

#include <sstream>
#include <iterator>

#define SYBMOL_TABLE_INITIAL_BUCKETS (7)

namespace bjou {

Scope::Scope(std::string _description, Scope * _parent, bool _is_module_scope, std::string mod)
    : description(_description), parent(_parent), is_module_scope(_is_module_scope) {

    if (parent) {
        usings.insert(usings.end(), parent->usings.begin(), parent->usings.end());
    }

    if (is_module_scope) {
        usings.push_back(mod);
        module_name = mod;
    }
}

Scope::~Scope() {
    for (Scope * scope : scopes)
        delete scope;
    for (auto & symbol : symbols)
        delete symbol.second;
}

static unsigned damerau_levenshtein_distance(const std::string& s1, const std::string& s2) {
    unsigned l1, l2, i, j, l_cost;
    std::vector<std::vector<unsigned> > d;

    l1 = s1.size();
    l2 = s2.size();

    d.resize(l1+1);
    for (auto& _d : d)    _d.resize(l2+1);
   
    for (i = 0; i <= l1; i++) { d[i][0] = i; }
    for (j = 0; j <= l2; j++) { d[0][j] = j; }

    for (i = 1; i <= l1; i++) {
        for (j = 1; j <= l2; j++) {
            if (s1[i-1] == s2[j-1]) {
                l_cost = 0;
            } else {
                l_cost = 1;
            }

            d[i][j] = std::min(
                d[i-1][j] + 1,                  // delete
                std::min(
                    d[i][j-1] + 1,              // insert
                    d[i-1][j-1] + l_cost)       // substitution
            );

            if ((i > 1)
            &&  (j > 1)
            &&  (s1[i-1] == s2[j-2])
            &&  (s1[i-2] == s2[j-1])) {

                d[i][j] = std::min(
                            d[i][j],
                            d[i-2][j-2] + l_cost   // transposition
                          );
            }
        }
    }
    return d[l1][l2];
}

static void collectSimilarSymbols(Scope * scope, std::string symbol, std::vector<std::string> &out) {
    unsigned thresh = 2;

    std::replace(symbol.begin(), symbol.end(), ':', ' ');
    std::replace(symbol.begin(), symbol.end(), '.', ' ');
    std::istringstream iss(symbol);
    std::vector<std::string> split_symbol(std::istream_iterator<std::string>{iss},
                                          std::istream_iterator<std::string>());

    std::vector<std::vector<std::string> > outs;
    outs.resize(thresh+1);

    while (scope) {
        for (auto& pair : scope->symbols) {
            std::string s2_save = pair.first;
            std::string s2 = s2_save;
            std::replace(s2.begin(), s2.end(), ':', ' ');
            std::replace(s2.begin(), s2.end(), '.', ' ');
            std::istringstream iss2(s2);
            std::vector<std::string> split_s2(std::istream_iterator<std::string>{iss2},
                                              std::istream_iterator<std::string>());

            if (split_symbol.size() != split_s2.size())
                continue;

            unsigned dist = 0;
            for (int i = 0; i < split_symbol.size(); i += 1) {
                dist += damerau_levenshtein_distance(split_symbol[i], split_s2[i]);
            }

            if (dist <= thresh) {
                outs[dist].push_back(s2_save);
            }
        }

        scope = scope->parent;
    }

    for (auto& _out : outs) {
        for (auto& s : _out) {
            out.push_back(s);
        }
    }
}

static void checkUninitialized(Scope * startingScope, Symbol * sym,
                               Context & context) {
    if (sym->isVar()) {
        // global vars are zero-initialized, so they
        // can be left uninitialized
        if (!sym->node()->getScope()->parent)
            return;

        if (sym->referenced) {
            Scope * s = startingScope;
            while (s) {
                if (sym->initializedInScopes.find(s) !=
                    sym->initializedInScopes.end())
                    return;
                s = s->parent;
            }
            errorl(context,
                   "'" + sym->unmangled +
                       "' might be uninitialized when referenced here.");
        }
    }
}

static Maybe<Symbol *> getModuleSymbol(Scope * startingScope, const std::string& m, const std::string& s,
                                    Context * context) {
    Scope * global_scope = compilation->frontEnd.globalScope;

    for (Scope * scope : global_scope->scopes) {
        if (scope->is_module_scope && scope->module_name == m) {
            return scope->getSymbolSingleScope(startingScope, s, context, /* checkUninit =*/ false, /* countAsReference =*/ true);
        }
    }

    return Maybe<Symbol *>();
}

static void symbol_error(Scope * startingScope, const std::string& u, Context * context) {
    std::vector<std::string> similar;
    collectSimilarSymbols(startingScope, u, similar);
    if (similar.empty()) {
        errorl(*context,
                "Use of undeclared identifier '" + u + "'.");
    }
    std::string did_you_mean = "Did you mean ";
    const char * lazy_comma = "";
    for (auto& s : similar) {
        did_you_mean += lazy_comma;
        if (&s == &similar.back() && similar.size() > 1) {
            did_you_mean += "or ";
        }
        did_you_mean += "'" + s + "'";
        lazy_comma = ", ";
    }
    did_you_mean += "?";
    BJOU_DEBUG_ASSERT(context);
    errorl(*context,
            "Use of undeclared identifier '" + u + "'.", true, did_you_mean);
}

Maybe<Symbol *> Scope::getSymbolSingleScope(Scope * startingScope,
                                            const std::string & qualifiedIdentifier,
                                            Context * context, 
                                            bool checkUninit,
                                            bool countAsReference) {

    if (symbols.count(qualifiedIdentifier) > 0) {
        if (checkUninit)
            checkUninitialized(startingScope, symbols[qualifiedIdentifier],
                               *context);
        if (countAsReference)
            symbols[qualifiedIdentifier]->referenced = true;
        return Maybe<Symbol *>(symbols[qualifiedIdentifier]);
    }

    return Maybe<Symbol *>();
}

Maybe<Symbol *> Scope::getSymbol(Scope * startingScope,
                                 const std::string & qualifiedIdentifier,
                                 Context * context, bool traverse, bool fail,
                                 bool checkUninit, bool countAsReference, std::string mod) {

    bool has_mod = !mod.empty();
    if (this == startingScope && !has_mod) {
        if (auto m_mod = get_mod_from_string(qualifiedIdentifier)) {
            m_mod.assignTo(mod);
            has_mod = true;
        }
    }

    if (has_mod) {
        if (auto m_sym = getModuleSymbol(startingScope, mod, qualifiedIdentifier, context)) {
            return m_sym;
        } else {
            goto out;
        }
    }

    if (auto m_sym = getSymbolSingleScope(startingScope, qualifiedIdentifier, context, checkUninit, countAsReference)) {
        return m_sym;
    }

    if (traverse && parent) {
        auto m_sym = parent->getSymbol(startingScope, qualifiedIdentifier, context,
                                 traverse, fail, checkUninit, countAsReference, mod);

        if (m_sym)    { return m_sym; }
    }

    if (!has_mod) {
        for (auto it = usings.rbegin(); it != usings.rend(); it++) {
            auto m_sym = getModuleSymbol(startingScope, *it, *it + "::" + qualifiedIdentifier, context);

            if (m_sym)    { return m_sym; }
        }
    }

out:
    if (fail && this == startingScope)    { symbol_error(startingScope, qualifiedIdentifier, context); }

    return Maybe<Symbol *>();
}
    
Maybe<Symbol *> Scope::getSymbol(Scope * startingScope, ASTNode * _identifier,
                                 Context * context, bool traverse, bool fail,
                                 bool checkUninit, bool countAsReference) {
    
    std::string m = "";
    std::string s = ((Identifier*)_identifier)->symAll();
    return getSymbol(startingScope, s, context, traverse, fail, checkUninit, countAsReference, m);
}

void Scope::addSymbol(Symbol * symbol, Context * context) {
    Maybe<Symbol *> m_existing = getSymbol(this, symbol->unmangled, nullptr, true, false);
    Symbol * existing = nullptr;
    if (m_existing.assignTo(existing) &&
        !symbol->node()->getFlag(ASTNode::SYMBOL_OVERWRITE)) {
        if (existing->node()->nodeKind == ASTNode::PROC_SET) {
            ProcSet * procSet = (ProcSet *)existing->node();
            for (auto & psym : procSet->procs) {
                if (psym.second->node()->getScope() == this) {
                    errorl(*context,
                           "Redefinition of '" + symbol->unmangled +
                               "' as a different kind of symbol.",
                           false);
                    Context econtext = psym.second->node()->getNameContext();
                    errorl(econtext, "'" + psym.second->unmangled +
                                         "' defined here.");
                }
            }
        } else if (existing->node()->getScope()->parent ||
                   existing->node()->getScope() == symbol->node()->getScope()) {
            /* the existing symbol isn't a global or they are both global */
            errorl(*context,
                   "Redefinition of '" + symbol->unmangled + "'.",
                   false);
            Context econtext = existing->node()->getNameContext();
            errorl(econtext,
                   "'" + existing->unmangled + "' also defined here.");
        }
    }
    if (symbol->isVar()
    &&  symbol->node()->getFlag(VariableDeclaration::IS_PROC_PARAM))
        symbol->initializedInScopes.insert(this);
    symbols[symbol->unmangled] = symbol;
}

void Scope::addProcSymbol(Symbol * symbol, bool is_extern, bool no_mangle, Context * context) {
    Maybe<Symbol *> m_existing = getSymbol(this, symbol->proc_name, nullptr, true, false);
    Symbol * existing = nullptr;
    
    std::string pn = string_sans_mod(symbol->proc_name);

    Scope * gs = compilation->frontEnd.globalScope;

    if (m_existing.assignTo(existing)) {
        if (existing->node()->nodeKind != ASTNode::PROC_SET) {
            errorl(*context,
                   "Redefinition of '" + symbol->unmangled +
                       "' as a different kind of symbol.",
                   false);
            errorl(existing->node()->getNameContext(),
                   "'" + symbol->unmangled + "' defined here.");
        }

        ProcSet * set = (ProcSet*)existing->node();

        if (is_extern || no_mangle) {
            for (auto & sym : set->procs) {
                if (sym.second->isTemplateProc())
                    continue;

                Procedure * existing_proc = (Procedure *)sym.second->node();

                BJOU_DEBUG_ASSERT(symbol->node()->nodeKind == ASTNode::PROCEDURE);
                Procedure * proc = (Procedure *)symbol->node();

                if (existing_proc->getFlag(Procedure::IS_EXTERN)
                ||  existing_proc->getFlag(Procedure::NO_MANGLE)) {
                    const Type * t = proc->getType();
                    const Type * existing_t = existing_proc->getType();

                    if (!equal(existing_t, t)) {
                        std::string err_str = existing_proc->getFlag(Procedure::NO_MANGLE)
                                                ? "no_mangle"
                                                : "extern";
                        errorl(
                            proc->getNameContext(),
                            "Conflicting declarations of " + err_str + " procedure '" +
                                proc->getName() + "'.",
                            false,
                            "declared with type '" + t->getDemangledName() +
                                "'");
                        errorl(existing_proc->getNameContext(),
                               "'" + existing_proc->getName() +
                                   "' previously declared here.",
                               true,
                               "declared with type '" +
                                   existing_t->getDemangledName() + "'");
                    }

                    if (!proc->getFlag(ASTNode::CT))
                        existing_proc->setFlag(ASTNode::CT, false);
                }
            }
        }
        
        if (set->procs.count(symbol->unmangled) > 0
        &&  !is_extern) {
            errorl(*context,
                   "Redefinition of '" + symbol->unmangled + "'.",
                   false);
            errorl(set->procs[symbol->unmangled]->node()->getNameContext(),
                   "'" + symbol->unmangled +
                       "' also defined here.");
        }

        set->procs[symbol->unmangled] = symbol;
    } else {
        if (this != gs) {
            BJOU_DEBUG_ASSERT(symbols.count(symbol->proc_name) == 0);
            ProcSet * set = new ProcSet(pn);
            symbols[symbol->proc_name] = new _Symbol<ProcSet>(symbol->proc_name, set);
            set->setScope(this);
            set->procs[symbol->unmangled] = symbol;
        }
    }

    ProcSet * set = nullptr;
    if (gs->symbols.count(pn) == 0) {
        set = new ProcSet(pn);
        gs->symbols[pn] = new _Symbol<ProcSet>(pn, set);
        set->setScope(gs);
    } else {
        BJOU_DEBUG_ASSERT(gs->symbols[pn]->node()->nodeKind == ASTNode::PROC_SET);
        set = (ProcSet*)gs->symbols[pn]->node();
    }
    set->procs[symbol->unmangled] = symbol;
}

void Scope::addSymbol(_Symbol<Procedure> * symbol, Context * context) {
    Procedure * proc = (Procedure *)symbol->node();
    bool is_extern = proc->getFlag(Procedure::IS_EXTERN);
    bool no_mangle = proc->getFlag(Procedure::NO_MANGLE);
    if (is_extern) {
        compilation->frontEnd.globalScope->addProcSymbol(symbol, is_extern, no_mangle, context);
    } else {
        addProcSymbol(symbol, is_extern, no_mangle, context);
    }
}

void Scope::addSymbol(_Symbol<TemplateProc> * symbol, Context * context) {
    addProcSymbol(symbol, false, false, context);
}

void Scope::printSymbols(int indent) const {
    static const char * side = "\xE2\x95\x91";
    printf("%s %-76s %s\n", side,
           (std::string(indent * 2, ' ') + "*** " + description + ":").c_str(),
           side);
    for (auto & sym : symbols) {
        if (sym.second->node()->nodeKind == ASTNode::PROC_SET) {
            for (auto& p_sym : ((ProcSet*)sym.second->node())->procs) {
                p_sym.second->tablePrint(indent);
            }
        } else {
            sym.second->tablePrint(indent);
        }
    }
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
