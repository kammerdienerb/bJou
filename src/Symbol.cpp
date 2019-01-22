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
void ProcSet::addSymbols(Scope * scope) { BJOU_DEBUG_ASSERT(false); }

static void printProcSetGetError(ProcSet * set, ASTNode * args, Context * context) {
    std::vector<std::string> help;

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

    for (auto & p : set->procs) {
        std::string option = "\t";
        option += p.second->unmangled;
        help.push_back(option);
    }

    errorl(*context, "No matching call for '" + set->name + "' found.", true,
            help);
}

Procedure * ProcSet::get(ASTNode * args, ASTNode * inst, Context * context,
                         bool fail) {
    BJOU_DEBUG_ASSERT(procs.size());

    std::vector<const Type *> arg_types;
    if (args)
        for (ASTNode * arg : ((ArgList *)args)->getExpressions())
            arg_types.push_back(arg->getType());

    if (procs.size() == 1 && !procs.begin()->second->isTemplateProc()) {
        if (args) {
            ProcedureType * compare_type =
                (ProcedureType *)ProcedureType::get(arg_types, VoidType::get());
            if (!argMatch((ProcedureType*)procs.begin()->second->node()->getType(), compare_type)) {
                if (fail) {
                    printProcSetGetError(this, args, context);
                } else
                    return nullptr;
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
    candidates = getCandidates(compare_type, args, inst, context, fail);
    resolve(candidates, resolved, compare_type, args, inst, context, fail);

    if (resolved.empty()) {
        if (fail) {
            BJOU_DEBUG_ASSERT(args);
            printProcSetGetError(this, args, context);
        }
    } else {
        Symbol * sym = resolved[0];

        if (!sym->isTemplateProc())
            return (Procedure *)sym->node();

        TemplateProc * tproc = (TemplateProc *)sym->node();
        return makeTemplateProc(tproc, args, inst, context);
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

std::vector<Symbol *> ProcSet::getCandidates(ProcedureType * compare_type,
                                             ASTNode * args, ASTNode * inst,
                                             Context * context, bool fail) {
    std::vector<Symbol *> candidates;

    if (compare_type) {
        for (auto & p : procs) {
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
                                                   false) != -1)
                    candidates.push_back(p.second);
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
   
    if (_node->getScope()->parent) {
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
