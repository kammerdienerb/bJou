//  Symbol.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#include <map>

#include "CLI.hpp"
#include "Compile.hpp"
#include "FrontEnd.hpp"
#include "Global.hpp"
#include "Misc.hpp"
#include "Symbol.hpp"
#include "Template.hpp"

namespace bjou {
/*
SymbolNamespace::SymbolNamespace(std::string _name, SymbolNamespace * _parent,
bool _global) : global(_global), name(_name), parent(_parent) {  }

std::string SymbolNamespace::mangledPrefix() const {
    std::string mangled;
    if (global) return "";
    if (parent)
        mangled = parent->mangledPrefix();
    mangled += "N" + std::to_string(name.size()) + name;
    return mangled;
}

int SymbolNamespace::depth(int d) {
    if (!namespaces.empty())
        for (auto& sub : namespaces)
            d = B_MAX(d, sub.second->depth(d + 1));
    return d;
}

Maybe<Symbol*> SymbolNamespace::search(std::string unqualifiedIdentifier, int
depth) { if (symbols.count(unqualifiedIdentifier) > 0) return
Maybe<Symbol*>(symbols[unqualifiedIdentifier]); Maybe<Symbol*> symbol; if (depth
> 0) { for (auto& sub : namespaces) { symbol =
sub.second->search(unqualifiedIdentifier, depth - 1); if (symbol) return symbol;
        }
    }
    return Maybe<Symbol*>();
}

GlobalSymbolNamespace::GlobalSymbolNamespace() :
    SymbolNamespace("__bjou_global_namespace", nullptr, true) {  }
*/

ProcSet::ProcSet() {
    nodeKind = PROC_SET;
    setFlag(PROC_SET, true);
    // std::lock_guard<std::mutex> lock(compilation_mtx);
    // compilation->frontEnd.n_nodes--;
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

Procedure * ProcSet::get(ASTNode * args, ASTNode * inst, Context * context,
                         bool fail) {
    BJOU_DEBUG_ASSERT(procs.size());

    if (procs.size() == 1 && !procs.begin()->second->isTemplateProc())
        return (Procedure *)procs.begin()->second->node();

    std::vector<const Type *> arg_types;
    if (args)
        for (ASTNode * arg : ((ArgList *)args)->getExpressions())
            arg_types.push_back(arg->getType());

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

            for (auto & p : procs) {
                std::string option = "\t";
                option +=
                    p.second->isProc()
                        ? p.second->node()->getType()->getDemangledName()
                        : "template " + demangledString(
                                            ((Procedure *)((TemplateProc *)
                                                               p.second->node())
                                                 ->_template)
                                                ->getMangledName());
                help.push_back(option);
            }

            errorl(*context, "No matching call for '" + name + "' found.", true,
                   help);
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
        std::string show;

        if (sym->isTemplateProc())
            show = ((Procedure *)((TemplateProc *)sym->node())->_template)
                       ->getMangledName();
        else
            show = ((Procedure *)sym->node())->getMangledName();

        show = demangledString(show);

        errorl(sym->node()->getNameContext(), "'" + show + "' is a candidate.",
               (&sym == &candidates.back()));
    }
}

ProcSet::~ProcSet() {
    // std::lock_guard<std::mutex> lock(compilation_mtx);
    // compilation->frontEnd.n_nodes++;
}

static size_t demangleNamespace(std::string mangled, std::string & demangled) {
    size_t p = 0;
    std::string nspace_nchars;
    if (mangled[p] == 'N') {
        p += 1;
        while (isdigit(mangled[p])) {
            nspace_nchars += mangled[p];
            p += 1;
        }
        int n = atoi(nspace_nchars.c_str());
        demangled += mangled.substr(p, n) + "::";
        p += n;
    }
    return p;
}

static size_t demangleTypename(std::string mangled, std::string & demangled);

static size_t demangleTemplate(std::string mangled, std::string & demangled) {
    size_t p = 0;
    std::string nspace_nchars;
    if (mangled[p] == 'M') {
        p += 1;
        while (isdigit(mangled[p])) {
            nspace_nchars += mangled[p];
            p += 1;
        }
        int n = atoi(nspace_nchars.c_str());

        demangled += "!(";
        for (int i = 0; i < n; i += 1) {
            p += demangleTypename(mangled.substr(p), demangled);
            if (i < n - 1)
                demangled += ", ";
        }
        demangled += ")";
    }

    return p;
}

static size_t demangleInterfaceProc(std::string mangled,
                                    std::string & demangled) {
    std::string mangled_nchars;
    size_t p = 0;
    if (mangled[p] == 'I') {
        p += 1;
        while (isdigit(mangled[p])) {
            mangled_nchars += mangled[p];
            p += 1;
        }
        int n = atoi(mangled_nchars.c_str());
        demangled += demangledString(mangled.substr(p, n)) + ".";
        p += n;
    }
    return p;
}

static size_t demangleTypeMember(std::string mangled, std::string & demangled) {
    std::string mangled_nchars;
    size_t p = 0;
    if (mangled[p] == 'S') {
        p += 1;
        while (isdigit(mangled[p])) {
            mangled_nchars += mangled[p];
            p += 1;
        }
        int n = atoi(mangled_nchars.c_str());
        demangled += demangledString(mangled.substr(p, n)) + ".";
        p += n;

        if (mangled[p] == 'I')
            p += demangleInterfaceProc(mangled.substr(p), demangled);
    }
    return p;
}

static size_t demangleProcInfo(std::string mangled, std::string & demangled);

static size_t demangleTypename(std::string mangled, std::string & demangled) {
    std::string mangled_nchars;
    size_t p = 0;

    char which = mangled[p];
    p += 1;

    size_t specifiers_p = -1;
    unsigned int nspecifiers = 0;
    if (mangled[p] == 'p' || mangled[p] == 'a') {
        specifiers_p = p;
        while (mangled[p] == 'p' || mangled[p] == 'a') {
            nspecifiers += 1;
            p += 1;
        }
    }

    if (which == 'T') {
        std::string type_nchars;
        while (isdigit(mangled[p])) {
            type_nchars += mangled[p];
            p += 1;
        }
        int tn = atoi(type_nchars.c_str());
        std::string type_mangled = mangled.substr(p, tn);
        demangled += demangledString(type_mangled);
        p += tn;
    } else if (which == 'P') {
        demangled += "<";
        p += demangleProcInfo(mangled.substr(p), demangled);
        demangled += ">";
    }

    for (size_t sp = 0; sp < nspecifiers; sp += 1)
        if (mangled[specifiers_p + sp] == 'p')
            demangled += "*";
        else if (mangled[specifiers_p + sp] == 'a')
            demangled += "[]";

    return p;
}

static size_t demangleProcInfo(std::string mangled, std::string & demangled) {
    std::string mangled_nchars;
    size_t p = 0;

    if (mangled[p] == 'A') {
        demangled += "(";
        p += 1;
        std::string s_nparams;
        while (isdigit(mangled[p])) {
            s_nparams += mangled[p];
            p += 1;
        }
        int n = atoi(s_nparams.c_str());
        for (int i = 0; i < n; i += 1) {
            p += demangleTypename(mangled.substr(p), demangled);

            if (i < (n - 1))
                demangled += ", ";
        }
        demangled += ")";

        if (mangled[p] == 'R') {
            p += 1;
            std::string rt, colon = " : ";
            p += demangleTypename(mangled.substr(p), rt);
            if (rt != "void")
                demangled += colon + rt;
        }
    }

    return p;
}

std::string demangledString(std::string mangled) {
    std::string demangled;
    // bool _template = false;
    size_t p = 0;
    if (mangled[p] == '_' && mangled[p + 1] == 'Z') {
        p += 2;

        if (mangled[p] == 'N')
            p += demangleNamespace(mangled.substr(p), demangled);
        if (mangled[p] == 'M')
            p += demangleTemplate(mangled.substr(p), demangled);
        if (mangled[p] == 'S')
            p += demangleTypeMember(mangled.substr(p), demangled);

        // the identifier
        std::string mangled_nchars;
        while (isdigit(mangled[p])) {
            mangled_nchars += mangled[p];
            p += 1;
        }
        int n = atoi(mangled_nchars.c_str());
        demangled += mangled.substr(p, n);
        p += n;

        if (mangled[p] == 'M')
            p += demangleTemplate(mangled.substr(p), demangled);

        if (mangled[p] == 'A')
            p += demangleProcInfo(mangled.substr(p), demangled);

    } else
        demangled = mangled;
    return demangled;
}

std::string mangledIdentifier(Identifier * identifier) {
    if (identifier->qualified.size())
        return identifier->qualified;
    std::string & u = identifier->getUnqualified();
    if (identifier->namespaces.empty())
        return u;
    std::string qualified = "_Z";
    for (std::string & nspaceName : identifier->getNamespaces())
        qualified += "N" + std::to_string(nspaceName.size()) + nspaceName;
    qualified += std::to_string(u.size()) + u;
    return qualified;
}

Identifier * mangledStringtoIdentifier(std::string mangled) {
    size_t p = 0;

    Identifier * ident = new Identifier;
    ident->getContext().filename = "<identifier created internally>";

    if (mangled[p] == '_' && mangled[p + 1] == 'Z') {
        ident->qualified = mangled;
        p += 2;
        while (mangled[p] == 'N') {
            p += 1;
            std::string nspace_nchars;
            while (isdigit(mangled[p])) {
                nspace_nchars += mangled[p];
                p += 1;
            }
            int n = atoi(nspace_nchars.c_str());
            ident->addNamespace(mangled.substr(p, n));
            p += n;
        }
        if (mangled[p] == 'M') {
            // @incomplete
            // _template = true;
            p += 1;
        }
        std::string mangled_nchars;
        while (isdigit(mangled[p])) {
            mangled_nchars += mangled[p];
            p += 1;
        }
        int n = atoi(mangled_nchars.c_str());
        ident->setUnqualified(mangled.substr(p, n));
        p += n;
    } else
        ident->setUnqualified(mangled);

    return ident;
}

Symbol::Symbol(std::string _name, ASTNode * __node, ASTNode * _inst)
    : name(_name), _node(__node), inst(_inst), initializedInScopes({}),
      referenced(false) {}
Symbol::~Symbol() {}
bool Symbol::isProc() const { return false; }
bool Symbol::isTemplateProc() const { return false; }
bool Symbol::isProcSet() const { return false; }
bool Symbol::isConstant() const { return false; }
bool Symbol::isVar() const { return false; }
bool Symbol::isType() const { return false; }
bool Symbol::isTemplateType() const { return false; }
bool Symbol::isAlias() const { return false; }
bool Symbol::isInterface() const { return false; }
ASTNode * Symbol::node() const { return _node; }

std::string Symbol::demangledString() { return ::bjou::demangledString(name); }

void Symbol::tablePrint(int indent) {
    static const char * pad = "................................................"
                              "............................";
    static const char * side = "\xE2\x95\x91";
    std::string indentation(indent * 2, ' ');
    std::string d = demangledString();
    if (indentation.size() + name.size() + d.size() + 2 > strlen(pad))
        d = "<too long>";
    printf("%s %s", side, indentation.c_str());
    setColor(LIGHTRED);
    printf("%s", name.c_str());
    resetColor();
    printf(" %s ", pad + indentation.size() + name.size() + d.size() + 2);
    setColor(LIGHTGREEN);
    printf("%s", d.c_str());
    resetColor();
    printf(" %s\n", side);
}

std::string mangledParams(Procedure * proc) {
    std::string ret =
        "A" + std::to_string(proc->getParamVarDeclarations().size());

    for (ASTNode * _param : proc->getParamVarDeclarations()) {
        VariableDeclaration * param = (VariableDeclaration *)_param;
        Declarator * paramDecl = (Declarator *)param->getTypeDeclarator();
        ret += paramDecl->mangleAndPrefixSymbol();
    }

    return ret;
}

std::string mangledParams(Procedure * proc, TemplateDefineList * def) {
    std::unordered_map<std::string, int> posmap;

    int i = 0;
    for (ASTNode * _elem : def->getElements()) {
        TemplateDefineElement * elem = (TemplateDefineElement *)_elem;
        posmap[elem->getName()] = i;
        i += 1;
    }

    std::string ret =
        "A" + std::to_string(proc->getParamVarDeclarations().size());

    for (ASTNode * _param : proc->getParamVarDeclarations()) {
        VariableDeclaration * param = (VariableDeclaration *)_param;
        Declarator * clone = nullptr;
        Declarator * paramDecl = (Declarator *)param->getTypeDeclarator();
        Declarator * paramDecl_base = (Declarator *)paramDecl->getBase();

        // @incomplete
        // What about tuples??

        if (posmap.count(
                ((Identifier *)paramDecl_base->identifier)->getUnqualified())) {
            clone = (Declarator *)paramDecl->clone();
            Declarator * clone_base = (Declarator *)clone->getBase();
            Identifier * ident = (Identifier *)clone_base->identifier;

            ident->qualified =
                "_" + std::to_string(posmap[ident->getUnqualified()]);

            // @bad
            std::string pre = clone->mangleAndPrefixSymbol();
            size_t p = 0;
            while (!isdigit(pre[p]))
                p += 1;
            ret += pre.substr(0, p);
            while (isdigit(pre[p]))
                p += 1;
            ret += pre.substr(p);

            delete clone;
        } else
            ret += paramDecl->mangleAndPrefixSymbol();
    }

    return ret;
}

std::string mangledTypeMemberPrefix(Struct * s) {
    std::string mangledName = s->getMangledName();

    // @bad!!!
    if (mangledName.size() >= 2 && mangledName[0] == '_' &&
        mangledName[1] == 'Z')
        mangledName = mangledName.substr(2);
    else
        mangledName = std::to_string(mangledName.size()) + mangledName;

    return "S" + mangledName;
}

std::string mangledInterfaceImplPrefix(InterfaceImplementation * impl) {
    Identifier * impl_ident = (Identifier *)impl->getIdentifier();
    Maybe<Symbol *> m_sym =
        impl->getScope()->getSymbol(impl->getScope(), impl_ident);
    Symbol * sym = nullptr;

    m_sym.assignTo(sym);

    BJOU_DEBUG_ASSERT(sym);
    BJOU_DEBUG_ASSERT(sym->isInterface());

    InterfaceDef * idef = (InterfaceDef *)sym->node();

    std::string mangledName = idef->getMangledName();

    // @bad!!!
    if (mangledName.size() >= 2 && mangledName[0] == '_' &&
        mangledName[1] == 'Z')
        mangledName = mangledName.substr(2);
    else
        mangledName = std::to_string(mangledName.size()) + mangledName;

    return "I" + mangledName;
}

std::string mangledInst(ASTNode * _inst) {
    TemplateInstantiation * inst = (TemplateInstantiation *)_inst;
    std::string ret = "M" + std::to_string(inst->elements.size());
    for (ASTNode * node : inst->getElements()) {
        if (IS_DECLARATOR(node)) {
            Declarator * d = (Declarator *)node;
            ret += d->mangleAndPrefixSymbol();
        } else if (IS_EXPRESSION(node)) {
            // @imcomplete

            Expression * e = (Expression *)node;
            BJOU_DEBUG_ASSERT(e->isConstant());
            Declarator * d = e->getType()->getGenericDeclarator();

            std::string e_val_str, e_type_str = d->mangleAndPrefixSymbol();
            e_type_str[0] = 'X';

            Val val = e->eval();

            delete d;
        } else
            internalError("invalid node type in template inst");
    }
    return ret;
}
} // namespace bjou
