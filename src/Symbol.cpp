//  Symbol.cpp
//  bjou
//
//  January, 2019

#include <map>

#include "CLI.hpp"
#include "Compile.hpp"
#include "FrontEnd.hpp"
#include "Global.hpp"
#include "Misc.hpp"
#include "Symbol.hpp"
#include "Template.hpp"
#include "bJouDemangle.h"

namespace bjou {
ProcSet::ProcSet() {
    nodeKind = PROC_SET;
    setFlag(PROC_SET, true);
}
ProcSet::ProcSet(std::string _name) {
    nodeKind = PROC_SET;
    setFlag(PROC_SET, true);
    name = _name;
}
void ProcSet::analyze(bool force) { BJOU_DEBUG_ASSERT(false); }
ASTNode * ProcSet::clone() {
    BJOU_DEBUG_ASSERT(false);
    return nullptr;
}
void ProcSet::addSymbols(std::string& _mod, Scope * scope) { BJOU_DEBUG_ASSERT(false); }

static bool can_use_module_node(ASTNode * node, Scope * scope) {
    const std::string& m = node->mod;

    if (m.empty()) { return true; }

    auto search = std::find(scope->usings.begin(), scope->usings.end(), m);
    if (search != scope->usings.end()) {
        return true;
    }

    return false;
}

static ProcSet * get_global_set(ProcSet * set, Scope * scope, Context * context) {
    Symbol * sym = nullptr;
    auto m_sym = compilation->frontEnd.globalScope->getSymbolSingleScope(scope, set->name, context, false, false);

    m_sym.assignTo(sym);
    BJOU_DEBUG_ASSERT(sym);
    BJOU_DEBUG_ASSERT(sym->isProcSet());

    return (ProcSet*)sym->node();
}

static void printProcSetGetError(ProcSet * set, ASTNode * args, Context * context, Scope * scope, bool set_is_in_module) {
    std::vector<std::string> help;

    set = get_global_set(set, scope, context);    

    if (args) {
        std::string passedTypes = "Note: recieved argument types: (";
        for (ASTNode *& expr : ((ArgList *)args)->getExpressions()) {
            passedTypes += expr->getType()->getDemangledName();
            if (&expr != &((ArgList *)args)->getExpressions().back())
                passedTypes += ", ";
        }
        passedTypes += ")";

        help.push_back(passedTypes);
    }

    help.push_back("Note: options are:");

    int n_options = 0;

    for (auto & p : set->procs) {
        if (!set_is_in_module && !can_use_module_node(p.second->node(), scope)) {
            continue;
        }
        std::string option = "\t";
        option += p.second->unmangled;
        help.push_back(option);
        n_options += 1;
    }

    if (n_options == 0) {
        help.clear();
        help.push_back("No viable option is in scope");
        help.push_back("Note: '" + set->name + "' has other options in the following modules:");

        std::set<std::string> mods;
        for (auto & p : set->procs) {
            ASTNode * n = p.second->node();
            const std::string& mod = n->mod;
            if (!mod.empty()) {
                mods.insert(mod);
            }
        }

        for (auto& m : mods) {
            help.push_back("    " + m);
        }
    }

    errorl(*context, "No matching call for '" + set->name + "' found.", true, help);
}

Procedure * ProcSet::try_global_set(Scope * scope, ASTNode * args, ASTNode * inst, Context * context, bool fail) {
    ProcSet * global_set = get_global_set(this, scope, context);

    if (this == global_set) {
        return nullptr;
    }

    return global_set->get(scope, args, inst, context, fail);
}

Procedure * ProcSet::get(Scope * scope, ASTNode * args, ASTNode * inst, Context * context,
                         bool fail) {
    BJOU_DEBUG_ASSERT(procs.size());

    bool set_is_in_module = getScope()->is_module_scope;

    std::vector<const Type *> arg_types;
    if (args)
        for (ASTNode * arg : ((ArgList *)args)->getExpressions())
            arg_types.push_back(arg->getType());

    if (procs.size() == 1 && !procs.begin()->second->isTemplateProc()) {
        if (args) {
            ProcedureType * compare_type =
                (ProcedureType *)ProcedureType::get(arg_types, VoidType::get());
            if (!argMatch((ProcedureType*)procs.begin()->second->node()->getType(), compare_type)) {
                Procedure * from_global = try_global_set(scope, args, inst, context, fail);
                if (from_global)    { return from_global; }

                if (fail) {
                    printProcSetGetError(this, args, context, scope, set_is_in_module);
                } else {
                    return nullptr;
                }
            }
        }
        return (Procedure *)procs.begin()->second->node();
    }

    // If we know we are using a template (there is an inst) we go ahead and do
    // it immediately.
    // Just find the one with the matching number of args and instantiate it.
    if (inst) {
        Procedure * proc = getTemplate(arg_types, args, inst, context, fail);
        if (proc)
            return proc;
    }

    ProcedureType * compare_type = nullptr;
    // If we don't have args, we need to use the l-val to determing our type to
    // find the correct proc.
    bool uses_l_val = false;

    if (args) {
        // @bad
        compare_type =
            (ProcedureType *)ProcedureType::get(arg_types, VoidType::get());
    } else if (!compilation->frontEnd.lValStack.empty() &&
               compilation->frontEnd.lValStack.top()->isProcedure()) {
        compare_type = (ProcedureType *)compilation->frontEnd.lValStack.top();
        arg_types = compare_type->paramTypes;
        uses_l_val = true;
    }

    if (fail && !compare_type) {
        std::string help1 =
            "Note: overloads are selected based on current l-value type.";
        std::string help2 = "\te.g. 'var : <(int, int) : int> = sum' will be "
                            "able to select the correct overload if it exists";
        errorl(*context, "Reference to '" + name + "' is ambiguous.", true,
               help1, help2);
    }

    std::vector<Symbol *> candidates, resolved;
    candidates = getCandidates(scope, compare_type, args, inst, context, fail, set_is_in_module);
    resolve(candidates, resolved, compare_type, args, inst, context, fail);

    if (resolved.empty()) {
        Procedure * from_global = try_global_set(scope, args, inst, context, fail);
        if (from_global)    { return from_global; }

        if (fail) {
            BJOU_DEBUG_ASSERT(args);
            printProcSetGetError(this, args, context, scope, set_is_in_module);
        }
    } else {
        Symbol * sym = resolved[0];

        if (!sym->isTemplateProc())
            return (Procedure *)sym->node();

        TemplateProc * tproc = (TemplateProc *)sym->node();
        return makeTemplateProc(tproc, args, inst, context, fail);
    }

    return nullptr;
}

Procedure * ProcSet::getTemplate(std::vector<const Type *> & arg_types,
                                 ASTNode * args, ASTNode * inst,
                                 Context * context, bool fail) {
    std::vector<Symbol *> symbols;
    std::vector<TemplateProc *> matches;

    for (auto & p : procs) {
        if (p.second->isTemplateProc()) {
            TemplateProc * tproc = (TemplateProc *)p.second->node();

            if (checkTemplateProcInstantiation(tproc, args, inst, context,
                                               fail) != -1) {
                matches.push_back(tproc);
                symbols.push_back(p.second);
            }
        }
    }

    if (!matches.empty()) {
        if (matches.size() == 1) {
            return makeTemplateProc(matches[0], args, inst, context);
        } else if (fail) {
            showCandidatesError(symbols, context);
        }
    }

    return nullptr;
}

std::vector<Symbol *> ProcSet::getCandidates(Scope * scope, ProcedureType * compare_type,
                                             ASTNode * args, ASTNode * inst,
                                             Context * context, bool fail, bool set_is_in_module) {
    std::vector<Symbol *> candidates;

    if (compare_type) {
        for (auto & p : procs) {
            if (!set_is_in_module && !can_use_module_node(p.second->node(), scope)) {
                continue;
            }

            if (!p.second->isTemplateProc()) {
                if (argMatch(p.second->node()->getType(), compare_type)) {
                    if (!p.second->node()->getFlag(
                            Procedure::IS_TEMPLATE_DERIVED)) {
                        candidates.push_back(p.second);
                    }
                }
            } else {
                TemplateProc * tproc = (TemplateProc *)p.second->node();

                // don't fail with the check when we are just gathering
                // candidates
                if (checkTemplateProcInstantiation(tproc, args, inst, context,
                                                   false) != -1) {
                    candidates.push_back(p.second);
                }
            }
        }
    }

    return candidates;
}

bool ProcSet::resolve(std::vector<Symbol *> & candidates,
                      std::vector<Symbol *> & resolved,
                      ProcedureType * compare_type, ASTNode * args,
                      ASTNode * inst, Context * context, bool fail) {
    typedef std::multimap<int, Symbol *> Map;
    Map _concrete, _template;

    if (compare_type) {
        for (auto sym : candidates) {
            if (sym->isTemplateProc()) {
                TemplateProc * tproc = (TemplateProc *)sym->node();
                _template.insert(
                    std::make_pair(checkTemplateProcInstantiation(
                                       tproc, args, inst, context, fail),
                                   sym));
            } else {
                ProcedureType * candidate_type =
                    (ProcedureType *)sym->node()->getType();
                _concrete.insert(std::make_pair(
                    countConversions(candidate_type, compare_type), sym));
            }
        }

        if (!_concrete.empty() || !_template.empty()) {
            typedef Map::iterator It;
            std::pair<It, It> take;

            if (_template.empty()) {
                take = _concrete.equal_range(_concrete.begin()->first);
            } else if (_concrete.empty()) {
                take = _template.equal_range(_template.begin()->first);
            } else if (_concrete.begin()->first <= _template.begin()->first) {
                take = _concrete.equal_range(_concrete.begin()->first);
            } else {
                take = _template.equal_range(_template.begin()->first);
            }

            for (It it = take.first; it != take.second; it++) {
                resolved.push_back(it->second);
            }
        }
    }

    if (resolved.size() > 1) {
        if (fail)
            showCandidatesError(resolved, context);
        return false;
    }

    return true;
}

void ProcSet::showCandidatesError(std::vector<Symbol *> & candidates,
                                  Context * context) {
    errorl(*context, "Reference to '" + name + "' is ambiguous.", false);
    for (Symbol *& sym : candidates) {
        errorl(sym->node()->getNameContext(), "'" + sym->unmangled + "' is a candidate.",
               (&sym == &candidates.back()));
    }
}

ProcSet::~ProcSet() {  }

std::string mangledIdentifier(Identifier * ident) {
    Scope * s = ident->getScope();
    auto m_sym = s->getSymbol(s, ident, &ident->getContext(), true, true, false, false);

    Symbol * sym  = nullptr;
    m_sym.assignTo(sym);
    BJOU_DEBUG_ASSERT(sym);

    return sym->real_mangled;
}

std::string demangledIdentifier(Identifier * ident) {
    Scope * s = ident->getScope();
    auto m_sym = s->getSymbol(s, ident, &ident->getContext(), true, true, false, false);

    Symbol * sym  = nullptr;
    m_sym.assignTo(sym);
    BJOU_DEBUG_ASSERT(sym);

    return sym->unmangled;
}

Identifier * stringToIdentifier(std::string s) {
    Identifier * ident = new Identifier;
    ident->getContext().filename = "<identifier created internally>";

    ident->setSymName(s);

    return ident;
}

Identifier * stringToIdentifier(std::string m, std::string t, std::string n) {
    Identifier * ident = new Identifier;
    ident->getContext().filename = "<identifier created internally>";

    ident->setSymMod(m);
    ident->setSymType(t);
    ident->setSymName(n);

    return ident;
}

Maybe<std::string> get_mod_from_string(const std::string& s) {
    size_t p = 0,
           l = s.size();
    if (l > 2) {
        while (p < l - 2
        &&    (isalnum(s[p])
            || s[p] == '_'
            || s[p] == '\'')) {
            p += 1;
        }

        if (s[p] == ':' && s[p + 1] == ':') {
            return Maybe<std::string>(s.substr(0, p));
        }
    }

    return Maybe<std::string>();
}
 
std::string string_sans_mod(const std::string& s) {
    if (auto m_mod = get_mod_from_string(s)) {
        std::string mod;
        m_mod.assignTo(mod);
        return s.substr(mod.size() + 2);
    }

    return s;
}

Symbol::Symbol(ASTNode * __node)
    : _node(__node), initializedInScopes({}), referenced(false) { }
Symbol::~Symbol() { }
bool Symbol::isProc()         const { return false; }
bool Symbol::isTemplateProc() const { return false; }
bool Symbol::isProcSet()      const { return false; }
bool Symbol::isConstant()     const { return false; }
bool Symbol::isVar()          const { return false; }
bool Symbol::isType()         const { return false; }
bool Symbol::isTemplateType() const { return false; }
bool Symbol::isAlias()        const { return false; }
ASTNode * Symbol::node()      const { return _node; }

void Symbol::tablePrint(int indent) {
    static const char * pad = "................................................"
                              "............................";
    static const char * side = "\xE2\x95\x91";
    std::string indentation(indent * 2, ' ');

    std::string left, right;
   
    if (_node->getScope()->parent && !_node->getScope()->is_module_scope) {
        left = right = unmangled;
    } else {
        left = real_mangled;
        right = unmangled;
    }
    
    if (indentation.size() + left.size() + right.size() + 2 > strlen(pad))
        right = "<too long>";
    printf("%s %s", side, indentation.c_str());
    setColor(LIGHTRED);
    printf("%s", left.c_str());
    resetColor();
    printf(" %s ", pad + indentation.size() + left.size() + right.size() + 2);
    setColor(LIGHTGREEN);
    printf("%s", right.c_str());
    resetColor();
    printf(" %s\n", side);
}
}
