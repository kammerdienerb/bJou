//
//  Template.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "Template.hpp"
#include "FrontEnd.hpp"
#include "Symbol.hpp"

namespace bjou {
static void checkTypeTemplateInstantiation(ASTNode * _defs, ASTNode * _inst) {
    TemplateDefineList * defs = (TemplateDefineList *)_defs;
    TemplateInstantiation * inst = (TemplateInstantiation *)_inst;

    if (defs->getElements().size() != inst->getElements().size()) {
        errorl(
            inst->getContext(),
            "Number of tempalate arguments does not match template definition.",
            false,
            "expected " + std::to_string(defs->getElements().size()) +
                ", got " + std::to_string(inst->getElements().size()));
        errorl(defs->getContext(), "Template definition:");
    }
    for (int idx = 0; idx < (int)defs->getElements().size(); idx += 1) {
        ASTNode * d = defs->getElements()[idx];
        ASTNode * i = inst->getElements()[idx];

        i->analyze();
        switch (d->nodeKind) {
        case ASTNode::TEMPLATE_DEFINE_TYPE_DESCRIPTOR:
            if (!IS_DECLARATOR(i)) {
                if (IS_EXPRESSION(i))
                    errorl(i->getContext(),
                           "Template definition calls for type declarator.",
                           false, "got expression");
                else
                    errorl(i->getContext(),
                           "Template definition calls for type declarator.",
                           false);
                errorl(d->getContext(), "Type declarator specified here:");
            }
            break;
        case ASTNode::TEMPLATE_DEFINE_VARIADIC_TYPE_ARGS:
            // @incomplete
            BJOU_DEBUG_ASSERT(false);
            break;
        case ASTNode::TEMPLATE_DEFINE_EXPRESSION:
            // @incomplete
            BJOU_DEBUG_ASSERT(false);
            break;

        default:
            internalError("invalid node in template define list");
        }
    }
}

static bool terminalShouldReplace(ASTNode * term, ASTNode * d) {
    TemplateDefineElement * elem = (TemplateDefineElement *)d;
    if (IS_DECLARATOR(term)) {
        Declarator * decl = (Declarator *)term;
        Identifier * ident = (Identifier *)decl->getIdentifier();
        if (ident->getSymName() == elem->getName())
            return true;
    } else if (term->nodeKind == ASTNode::IDENTIFIER) { // @bad
        Identifier * ident = (Identifier *)term;
        if (ident->getSymName() == elem->getName())
            return true;
    } else if (IS_EXPRESSION(term)) {
        // @incomplete
    }
    return false;
}

static void checkTemplateExpressionSubstitution(ASTNode * term) {
    /* @hack -- this should be better and more robust */
    if (IS_EXPRESSION(term)) {
        bool okay = false;
        if (term->getParent()) {
            if (term->getParent()->nodeKind == ASTNode::MACRO_USE) {
                okay = true;
            } else if (term->getParent()->nodeKind == ASTNode::ACCESS_EXPRESSION) {
                Expression * e_p = (Expression*)term->getParent();
                if (e_p->getLeft() == term) {
                    okay = true;
                }
            }
        }

        if (!okay) {
            errorl(term->getContext(), "Invalid template substitution.", true, "Can't place type declarator where an expression is expected");
        }
    }
}

static void templateReplaceTerminals(ASTNode * _template, ASTNode * _def,
                                     ASTNode * _inst) {
    TemplateDefineList * def = (TemplateDefineList *)_def;
    TemplateInstantiation * inst = (TemplateInstantiation *)_inst;

    std::vector<ASTNode *> terminals;
    _template->unwrap(terminals);

    BJOU_DEBUG_ASSERT(def->getElements().size() == inst->getElements().size());

    for (ASTNode * term : terminals) {
        for (int idx = 0; idx < (int)def->getElements().size(); idx += 1) {
            ASTNode * d = def->getElements()[idx];
            ASTNode * i = inst->getElements()[idx];
            ASTNode * i_clone = i->clone();
            i_clone->setContext(term->getContext());
            if (terminalShouldReplace(term, d)) {
                checkTemplateExpressionSubstitution(term);
                (*term->replace)(term->parent, term, i_clone);
            }
        }
    }
}

static void templateReplaceTerminals(std::vector<ASTNode *> & terminals,
                                     ASTNode * _def, ASTNode * _inst) {
    TemplateDefineList * def = (TemplateDefineList *)_def;
    TemplateInstantiation * inst = (TemplateInstantiation *)_inst;

    BJOU_DEBUG_ASSERT(def->getElements().size() == inst->getElements().size());

    for (ASTNode * term : terminals) {
        for (int idx = 0; idx < (int)def->getElements().size(); idx += 1) {
            ASTNode * d = def->getElements()[idx];
            ASTNode * i = inst->getElements()[idx];
            ASTNode * i_clone = i->clone();
            if (terminalShouldReplace(term, d)) {
                checkTemplateExpressionSubstitution(term);
                (*term->replace)(term->parent, term, i_clone);
            }
        }
    }
}

Struct * makeTemplateStruct(ASTNode * _ttype, ASTNode * _inst) {
    TemplateStruct * ttype = (TemplateStruct *)_ttype;
    Struct * s = (Struct *)ttype->_template;
    Scope * scope = ttype->getScope();
    TemplateInstantiation * inst = (TemplateInstantiation *)_inst;

    checkTypeTemplateInstantiation(ttype->getTemplateDef(), inst);

    _Symbol<Struct> * newsym = new _Symbol<Struct>(s->getName(), _ttype->mod, "", s, inst, nullptr);
    std::string qual = newsym->unmangled;

    Maybe<Symbol *> m_sym =
        scope->getSymbol(scope, qual, nullptr, true, false, false);
    Symbol * sym = nullptr;
    if (m_sym.assignTo(sym)) {
        // found it
        BJOU_DEBUG_ASSERT(sym->isType());
        BJOU_DEBUG_ASSERT(sym->node()->nodeKind == ASTNode::STRUCT);

        return (Struct *)sym->node();
    }

    Struct * clone = (Struct *)s->clone();
    clone->parent = nullptr;
    clone->setName(s->getName());
    clone->setLookupName(s->getLookupName());
    clone->inst = inst;

    clone->setFlag(Struct::IS_TEMPLATE_DERIVED, true);

    clone->preDeclare(ttype->mod, scope);
    clone->addSymbols(ttype->mod, scope);

    templateReplaceTerminals(clone, ttype->getTemplateDef(), inst);

    // template members need to be expanded before we can complete the type
    // @bad is this the best way to do this?
    // does it cover things like member proc params? does it need to?
    for (ASTNode * mem : clone->getMemberVarDecls())
        mem->analyze(false);

    ((StructType *)clone->getType())->complete();
    ((StructType *)clone->getType())->checkForCycles();

    clone->analyze();//true);

    compilation->frontEnd.deferredAST.push_back(clone);

    return clone;
}

static ASTNode * replacement_policy_isolated_params(ASTNode * parent,
                                                    ASTNode * old_node,
                                                    ASTNode * new_node) {
    std::vector<ASTNode *> * terminals = (std::vector<ASTNode *> *)parent;

    for (ASTNode *& node : *terminals)
        if (node == old_node)
            node = new_node;

    return new_node;
}

static Declarator * patternMatchType(Declarator * pattern, Declarator * subject,
                                     std::string & name) {
    if (pattern->nodeKind != ASTNode::DECLARATOR) {
        if (pattern->nodeKind != ASTNode::POINTER_DECLARATOR &&
            subject->nodeKind != ASTNode::ARRAY_DECLARATOR) {

            if (pattern->nodeKind != subject->nodeKind)
                return nullptr;
        }
    }

    switch (pattern->nodeKind) {
    case ASTNode::DECLARATOR: {
        if (pattern->asString() == name)
            return (Declarator *)subject->clone();

        if (pattern->getTemplateInst()) {
            const Type * t = subject->getType();
            if (t->isStruct()) {
                StructType * s_t = (StructType *)t;
                if (s_t->inst) {
                    TemplateInstantiation * p_inst =
                        (TemplateInstantiation *)pattern->getTemplateInst();
                    TemplateInstantiation * s_inst =
                        (TemplateInstantiation *)s_t->inst;

                    if (p_inst->getElements().size() ==
                        s_inst->getElements().size()) {
                        for (int i = 0; i < (int)p_inst->getElements().size();
                             i += 1) {
                            Declarator * match = patternMatchType(
                                (Declarator *)p_inst->getElements()[i],
                                (Declarator *)s_inst->getElements()[i], name);
                            if (match)
                                return match;
                        }
                    }
                }
            }
        }
        break;
    }

    case ASTNode::ARRAY_DECLARATOR:
    case ASTNode::SLICE_DECLARATOR:
    case ASTNode::DYNAMIC_ARRAY_DECLARATOR:
    case ASTNode::POINTER_DECLARATOR:
    case ASTNode::REF_DECLARATOR:
    case ASTNode::MAYBE_DECLARATOR:
        return patternMatchType((Declarator *)pattern->under(),
                                (Declarator *)subject->under(), name);

    case ASTNode::SUM_DECLARATOR: {
        SumDeclarator * p_sum = (SumDeclarator *)pattern;
        SumDeclarator * s_sum = (SumDeclarator *)subject;

        if (p_sum->getSubDeclarators().size() !=
            s_sum->getSubDeclarators().size())
            return nullptr;

        for (int i = 0; i < (int)p_sum->getSubDeclarators().size(); i += 1) {
            Declarator * match = patternMatchType(
                (Declarator *)p_sum->getSubDeclarators()[i],
                (Declarator *)s_sum->getSubDeclarators()[i], name);
            if (match)
                return match;
        }
        break;
    }

    case ASTNode::TUPLE_DECLARATOR: {
        TupleDeclarator * p_tup = (TupleDeclarator *)pattern;
        TupleDeclarator * s_tup = (TupleDeclarator *)subject;

        if (p_tup->getSubDeclarators().size() !=
            s_tup->getSubDeclarators().size())
            return nullptr;

        for (int i = 0; i < (int)p_tup->getSubDeclarators().size(); i += 1) {
            Declarator * match = patternMatchType(
                (Declarator *)p_tup->getSubDeclarators()[i],
                (Declarator *)s_tup->getSubDeclarators()[i], name);
            if (match)
                return match;
        }
        break;
    }

    case ASTNode::PROCEDURE_DECLARATOR: {
        ProcedureDeclarator * p_proc = (ProcedureDeclarator *)pattern;
        ProcedureDeclarator * s_proc = (ProcedureDeclarator *)subject;

        if (p_proc->getParamDeclarators().size() !=
            s_proc->getParamDeclarators().size())
            return nullptr;

        Declarator * match = nullptr;

        for (int i = 0; i < (int)p_proc->getParamDeclarators().size(); i += 1) {
            match = patternMatchType(
                (Declarator *)p_proc->getParamDeclarators()[i],
                (Declarator *)s_proc->getParamDeclarators()[i], name);
            if (match)
                return match;
        }

        match =
            patternMatchType((Declarator *)p_proc->getRetDeclarator(),
                             (Declarator *)s_proc->getRetDeclarator(), name);

        if (match)
            return match;
        break;
    }
    default:
        BJOU_DEBUG_ASSERT(false &&
                          "Declarator kind not handled in patternMatchType()");
        return nullptr;
    }

    return nullptr;
}

int checkTemplateProcInstantiation(ASTNode * _tproc, ASTNode * _passed_args,
                                   ASTNode * _inst, Context * context,
                                   bool fail,
                                   TemplateInstantiation * new_inst) {
    bool delete_new_inst = false;
    TemplateProc * tproc = (TemplateProc *)_tproc;
    TemplateDefineList * def = (TemplateDefineList *)tproc->getTemplateDef();
    Procedure * proc = (Procedure *)tproc->_template;
    ArgList * passed_args = (ArgList *)_passed_args;
    TemplateInstantiation * inst = (TemplateInstantiation *)_inst;

    proc->desugarThis();

    std::vector<ASTNode *> & params = proc->getParamVarDeclarations();

    std::vector<std::pair<TemplateDefineElement *, Declarator *>> checklist;

    for (auto elem : def->getElements())
        checklist.push_back(
            std::make_pair((TemplateDefineElement *)elem, nullptr));

    if (inst) {
        if (inst->getElements().size() > checklist.size()) {
            if (fail)
                errorl(inst->getContext(),
                       "Too many template arguments."); // @bad error message
            else
                return -1;
        }
        for (int i = 0; i < (int)inst->getElements().size(); i += 1)
            checklist[i].second = (Declarator *)inst->getElements()[i];
    }

    if (passed_args) {
        if (passed_args->getExpressions().size() != params.size())
            return -1;

        for (int i = 0; i < (int)params.size(); i += 1) {
            Declarator * param_decl =
                (Declarator *)((VariableDeclaration *)params[i])
                    ->getTypeDeclarator();
            // unref
            if (param_decl->nodeKind == ASTNode::REF_DECLARATOR)
                param_decl = (Declarator *)param_decl->under();

            const Type * passed_type =
                passed_args->getExpressions()[i]->getType();

            Declarator * passed_decl = passed_type->getGenericDeclarator();

            // unref
            if (passed_decl->nodeKind == ASTNode::REF_DECLARATOR)
                passed_decl = (Declarator *)passed_decl->under();

            BJOU_DEBUG_ASSERT(passed_decl);
            passed_decl->setScope(passed_args->getExpressions()[i]->getScope());

            for (auto & check : checklist) {
                if (!check.second) {
                    Declarator * match = patternMatchType(
                        param_decl, passed_decl, check.first->name);
                    if (match)
                        check.second = match;
                }
            }

            delete passed_decl;
        }
    }

    if (!new_inst) {
        new_inst = new TemplateInstantiation;
        delete_new_inst = true;
    }

    std::vector<std::string> incomplete_errs;
    for (auto & check : checklist) {
        if (check.second) {
            if (new_inst)
                new_inst->addElement(check.second->clone());
        } else
            incomplete_errs.push_back("could not resolve '" +
                                      check.first->getName() + "'");
    }
    if (!incomplete_errs.empty()) {
        if (delete_new_inst)
            delete new_inst;
        if (fail)
            errorl(*context,
                   "Could not complete the template '" + proc->getName() + "'",
                   true, incomplete_errs);
        else
            return -1;
    }

    int nconv = 0;

    if (passed_args) {
        std::vector<const Type *> arg_types;
        for (ASTNode * expr : passed_args->getExpressions())
            arg_types.push_back(expr->getType());
        std::vector<const Type *> new_param_types;

        std::vector<VariableDeclaration *> var_clones;
        std::vector<ASTNode *> terms;
        for (ASTNode * _var : params) {
            VariableDeclaration *var_clone, *var = (VariableDeclaration *)_var;
            var_clone = (VariableDeclaration *)var->clone();
            var_clone->getTypeDeclarator()->addSymbols(tproc->mod, tproc->getScope());
            var_clone->unwrap(terms);
            var_clones.push_back(var_clone);
        }

        templateReplaceTerminals(terms, def, new_inst);

        for (ASTNode * _var : var_clones) {
            VariableDeclaration * var = (VariableDeclaration *)_var;

            // We need to analyze and THEN get the type, making sure to access
            // the declarator via getTypeDeclarator() because if the declarator
            // is a template, it will be replaced.

            /* var->getTypeDeclarator()->analyze(true); */
            var->getTypeDeclarator()->analyze();
            new_param_types.push_back(var->getTypeDeclarator()->getType());
            delete _var;
        }

        ProcedureType * candidate_type = (ProcedureType *)ProcedureType::get(
            new_param_types, VoidType::get());
        ProcedureType * compare_type =
            (ProcedureType *)ProcedureType::get(arg_types, VoidType::get());

        nconv = countConversions(candidate_type, compare_type);
    }

    if (delete_new_inst)
        delete new_inst;

    return nconv;
}

Procedure * makeTemplateProc(ASTNode * _tproc, ASTNode * _passed_args,
                             ASTNode * _inst, Context * context, bool fail) {
    TemplateProc * tproc = (TemplateProc *)_tproc;
    Scope * scope = tproc->getScope();
    Procedure * proc = (Procedure *)tproc->_template;
    TemplateDefineList * def = (TemplateDefineList *)tproc->getTemplateDef();

    TemplateInstantiation * new_inst = new TemplateInstantiation;
    checkTemplateProcInstantiation(_tproc, _passed_args, _inst, context, fail,
                                   new_inst);

    std::vector<ASTNode *> save_params, new_params, terms;
    for (ASTNode * param : proc->getParamVarDeclarations()) {
        save_params.push_back(param);

        ASTNode * param_clone = param->clone();
        new_params.push_back(param_clone);
        param_clone->unwrap(terms);
    }

    templateReplaceTerminals(terms, def, new_inst);

    proc->getParamVarDeclarations().clear();

    for (ASTNode * new_param : new_params)
        proc->addParamVarDeclaration(new_param);

    std::string t = "";
    if (tproc->getParent()) {
        ASTNode * p = tproc->getParent();
        if (p->nodeKind == ASTNode::STRUCT) {
            Struct * s = (Struct*)p;
            if (!p->getParent() || p->getParent()->nodeKind != ASTNode::TEMPLATE_STRUCT) {
                t = string_sans_mod(s->getLookupName());
            } else if (p->getParent() && p->getParent()->nodeKind == ASTNode::TEMPLATE_STRUCT) {
                t = s->getName();
            }
        } else if (p->nodeKind == ASTNode::TEMPLATE_STRUCT) {
            t         = ((Struct*)((TemplateStruct*)tproc->getParent())->_template)->getName();
        }
    }

    _Symbol<Procedure> * newsym =
        new _Symbol<Procedure>(proc->getName(), tproc->mod, t, proc, new_inst, nullptr);
    std::string qual = newsym->unmangled;
    
    Maybe<Symbol*> m_set_sym = scope->getSymbol(scope, newsym->proc_name, nullptr, true, false, false);
    Symbol * set_sym = nullptr;
    if (m_set_sym.assignTo(set_sym)) {
        ProcSet * set = (ProcSet*)set_sym->node();
        if (set->procs.count(qual)) {
            ASTNode * existing = set->procs[qual]->node();
            BJOU_DEBUG_ASSERT(existing->nodeKind == ASTNode::PROCEDURE);

            // restore params
            proc->getParamVarDeclarations().clear();

            for (ASTNode * p : save_params)
                proc->addParamVarDeclaration(p);

            return (Procedure *)existing;
        }
    }

    // restore params
    proc->getParamVarDeclarations().clear();

    for (ASTNode * p : save_params)
        proc->addParamVarDeclaration(p);

    for (ASTNode * new_param : new_params)
        delete new_param;

    Procedure * clone = (Procedure *)proc->clone();
    clone->setName(proc->getName()); // mangledName);
    clone->inst = new_inst;

    templateReplaceTerminals(clone, tproc->getTemplateDef(), new_inst);

    if (tproc->getFlag(TemplateProc::FROM_THROUGH_TEMPLATE))
        clone->setFlag(ASTNode::SYMBOL_OVERWRITE, true);

    clone->addSymbols(tproc->mod, scope);

    clone->analyze();//true);
    clone->setFlag(Procedure::IS_TEMPLATE_DERIVED, true);
    compilation->frontEnd.deferredAST.push_back(clone);

    return clone;
}
} // namespace bjou
