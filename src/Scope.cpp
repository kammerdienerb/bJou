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

namespace bjou {

Scope::Scope(std::string _description, Scope * _parent)
    : description(_description), parent(_parent) {}

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
                out.push_back(s2_save);
            }
        }

        scope = scope->parent;
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

Maybe<Symbol *> Scope::getSymbol(Scope * startingScope, ASTNode * _identifier,
                                 Context * context, bool traverse, bool fail,
                                 bool checkUninit, bool countAsReference) {
    Identifier * identifier = (Identifier *)_identifier;

    std::string u = identifier->symAll();

    if (symbols.count(u) > 0) {
        if (checkUninit)
            checkUninitialized(startingScope, symbols[u], *context);
        if (countAsReference)
            symbols[u]->referenced = true;
        return Maybe<Symbol *>(symbols[u]);
    }
    if (traverse && parent)
        return parent->getSymbol(startingScope, _identifier, context,
                                 traverse, fail, checkUninit,
                                 countAsReference);
    Scope * scope = compilation->frontEnd.globalScope;
    if (scope->symbols.count(u)) {
        if (checkUninit)
            checkUninitialized(startingScope, scope->symbols[u], *context);
        if (countAsReference)
            scope->symbols[u]->referenced = true;
        return Maybe<Symbol *>(scope->symbols[u]);
    }
    
    if (fail) {
        std::string reportName = identifier->symAll();
        std::vector<std::string> similar;
        collectSimilarSymbols(startingScope, u, similar);
        if (similar.empty()) {
            errorl(*context,
                   "Use of undeclared identifier '" + reportName + "'.");
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
                   "Use of undeclared identifier '" + reportName + "'.", true, did_you_mean);
    }

    return Maybe<Symbol *>();
}

Maybe<Symbol *> Scope::getSymbol(Scope * startingScope,
                                 std::string & qualifiedIdentifier,
                                 Context * context, bool traverse, bool fail,
                                 bool checkUninit, bool countAsReference) {
    if (symbols.count(qualifiedIdentifier) > 0) {
        if (checkUninit)
            checkUninitialized(startingScope, symbols[qualifiedIdentifier],
                               *context);
        if (countAsReference)
            symbols[qualifiedIdentifier]->referenced = true;
        return Maybe<Symbol *>(symbols[qualifiedIdentifier]);
    }
    if (traverse && parent)
        return parent->getSymbol(startingScope, qualifiedIdentifier, context,
                                 traverse, fail, checkUninit, countAsReference);
    if (fail) {
        if (context) {
            std::vector<std::string> similar;
            collectSimilarSymbols(startingScope, qualifiedIdentifier, similar);
            if (similar.empty()) {
                errorl(*context,
                       "Use of undeclared identifier '" + qualifiedIdentifier + "'.");
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

            errorl(*context, "Use of undeclared identifier '" +
                                 qualifiedIdentifier + "'.", true, did_you_mean);
        } else {
            internalError("getSymbol('" + qualifiedIdentifier +
                          "') failed without a context.");
        }
    }

    return Maybe<Symbol *>();
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

void Scope::addProcSymbol(Symbol * symbol, bool is_extern, Context * context) {
    Maybe<Symbol *> m_existing = getSymbol(this, symbol->proc_name, nullptr, false, false);
    Symbol * existing = nullptr;

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

        if (is_extern) {
            for (auto & sym : set->procs) {
                if (sym.second->isTemplateProc())
                    continue;

                Procedure * existing_proc = (Procedure *)sym.second->node();

                BJOU_DEBUG_ASSERT(symbol->node()->nodeKind == ASTNode::PROCEDURE);
                Procedure * proc = (Procedure *)symbol->node();

                if (existing_proc->getFlag(Procedure::IS_EXTERN)) {
                    const Type * t = proc->getType();
                    const Type * existing_t = existing_proc->getType();

                    if (!equal(existing_t, t)) {
                        errorl(
                            proc->getNameContext(),
                            "Conflicting declarations of extern procedure '" +
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
        if (symbols.count(symbol->proc_name) == 0) {
            symbols[symbol->proc_name] = new _Symbol<ProcSet>(symbol->proc_name, new ProcSet(symbol->proc_name));
        }
        if (symbols[symbol->proc_name]->node()->nodeKind == ASTNode::PROC_SET) {
            ProcSet * set = (ProcSet *)symbols[symbol->proc_name]->node();
            set->procs[symbol->unmangled] = symbol;
        }
    }
}

void Scope::addSymbol(_Symbol<Procedure> * symbol, Context * context) {
    Procedure * proc = (Procedure *)symbol->node();
    addProcSymbol(symbol, proc->getFlag(Procedure::IS_EXTERN), context);
}

void Scope::addSymbol(_Symbol<TemplateProc> * symbol, Context * context) {
    addProcSymbol(symbol, false, context);
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
