//
//  Symbol.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Symbol_hpp
#define Symbol_hpp

#include "Maybe.hpp"
#include "ASTNode.hpp"
#include "Context.hpp"
#include "Scope.hpp"

#include <string>
#include <unordered_map>
#include <set>
#include <iostream>

namespace bjou {
    struct Symbol;
    
    // ProcSet inherits from ASTNode only so that we can have symbols to it.
    // ProcSets are NOT part of the AST
    struct ProcSet : ASTNode {
        ProcSet();
        ProcSet(std::string _name);
        
        std::unordered_map<std::string, Symbol*> procs;
        std::string name;
        
        // Node interface
		void analyze(bool force = false);
        ASTNode * clone();
        void addSymbols(Scope * scope);
        ~ProcSet();
        //
        
        Procedure * get(ASTNode * args = nullptr, ASTNode * inst = nullptr, Context * context = nullptr, bool fail = true);
        Procedure * getTemplate(std::vector<const Type*>& arg_types, ASTNode * args, ASTNode * inst, Context * context, bool fail);
    };
    
    std::string demangledString(std::string mangled);
    std::string mangledIdentifier(Identifier * identifier);
    Identifier * mangledStringtoIdentifier(std::string mangled);
    
    struct Symbol {
        Symbol(std::string _name, ASTNode * __node, ASTNode * _inst = nullptr);
        virtual ~Symbol();
        
        std::string name;
        ASTNode * _node;
        ASTNode * inst;
        std::set<Scope*> initializedInScopes;
        bool referenced;

        virtual bool isProc() const = 0;
        virtual bool isTemplateProc() const = 0;
        virtual bool isProcSet() const = 0;
        virtual bool isConstant() const = 0;
        virtual bool isVar() const = 0;
        virtual bool isType() const = 0;
        virtual bool isTemplateType() const = 0;
        virtual bool isInterface() const = 0;
        
        ASTNode * node() const;
        virtual std::string mangledString(Scope * scope) = 0;
        std::string demangledString();
        virtual Symbol * mangled(Scope * scope) = 0;
        void tablePrint(int indent);
    };

    template <typename T>
    struct _Symbol : Symbol {
        _Symbol(std::string _name, ASTNode * __node, ASTNode * _inst = nullptr) : Symbol(_name, __node, _inst) {  }
        
        // default implementation..
        // other cases are specialized below
        std::string mangledString(Scope * scope) {
            std::string _name = name;
            std::string prefix = scope->mangledPrefix();
            if (prefix.size())
                _name = "_Z" + prefix + std::to_string(_name.size()) + _name;
            return _name;
        }
	
        bool isProc()           const { return false; }
        bool isTemplateProc()   const { return false; }
        bool isProcSet()        const { return false; }
        bool isConstant()       const { return false; }
        bool isVar()            const { return false; }
        bool isType()           const { return false; }
        bool isTemplateType()   const { return false; }
        bool isInterface()      const { return false; }

        Symbol * mangled(Scope * scope) {
            _Symbol<T> * mangledSymbol = new _Symbol<T>(name, _node);
            mangledSymbol->name = mangledString(scope);
            return mangledSymbol;
        }
    };
    
    std::string mangledParams(Procedure * proc);
    std::string mangledParams(Procedure * proc, TemplateDefineList * def);
    std::string mangledTypeMemberPrefix(Struct * s);
    std::string mangledInterfaceImplPrefix(InterfaceImplementation * impl);
    std::string mangledInst(ASTNode * _inst);

    template <>
    inline bool _Symbol<Procedure>::isProc()                const { return true; }
    template <>
    inline bool _Symbol<TemplateProc>::isTemplateProc()     const { return true; }
    template <>
    inline bool _Symbol<ProcSet>::isProcSet()               const { return true; }
    template <>
    inline bool _Symbol<Constant>::isConstant()             const { return true; }
    template <>
    inline bool _Symbol<VariableDeclaration>::isVar()       const { return true; }
    template <>
    inline bool _Symbol<Struct>::isType()                   const { return true; }
    template <>
    inline bool _Symbol<Alias>::isType()                    const { return true; }
    template <>
    inline bool _Symbol<Enum>::isType()                     const { return true; }
    template <>
    inline bool _Symbol<TemplateStruct>::isTemplateType()   const { return true; }
    template <>
    inline bool _Symbol<TemplateAlias>::isTemplateType()    const { return true; }
    template <>
    inline bool _Symbol<InterfaceDef>::isInterface()        const { return true; }
    
    template <>
    inline std::string _Symbol<Struct>::mangledString(Scope * scope) {
        std::string _name = name;
        
        if (inst && name.size() > 1 && name[0] == '_' && name[1] == 'Z')
            return name;
        
        std::string z, suffix, prefix = scope->mangledPrefix();
        if (inst)
            suffix += mangledInst(inst);
        
        if (prefix.size())
            z = "_Z";
        else if (inst)
            z = "_Z" + std::to_string(name.size());
        
        _name = z + prefix + _name + suffix;
        
        return _name;
    }
    template<>
    inline Symbol * _Symbol<Struct>::mangled(Scope * scope) {
        _Symbol<Struct> * mangledSymbol = new _Symbol<Struct>(name, _node, inst);
        mangledSymbol->name = mangledString(scope);
        return mangledSymbol;
    }
    
    template <>
    inline std::string _Symbol<Procedure>::mangledString(Scope * scope) {
        std::string _name = name;
        Procedure * proc = (Procedure*)node();
        
        if (!proc->getFlag(Procedure::IS_EXTERN)) {
            if (inst && name.size() > 1 && name[0] == '_' && name[1] == 'Z')
                return name;
            
            _name = "_Z" + scope->mangledPrefix();
            if (proc->getFlag(Procedure::IS_TYPE_MEMBER)) {
                Struct * parent = proc->getParentStruct();
                
                BJOU_DEBUG_ASSERT(parent);
                _name += mangledTypeMemberPrefix(parent);
                
                if (proc->getFlag(Procedure::IS_INTERFACE_IMPL)) {
                    BJOU_DEBUG_ASSERT(proc->parent->nodeKind == ASTNode::INTERFACE_IMPLEMENTATION);
                    InterfaceImplementation * impl = (InterfaceImplementation*)proc->parent;
                    _name += mangledInterfaceImplPrefix(impl);
                }
            }
            std::string m_inst;
            if (inst) m_inst = mangledInst(inst);
            _name += std::to_string(name.size()) + name + m_inst + mangledParams(proc);
        }
        
        return _name;
    }
    template <>
    inline Symbol * _Symbol<Procedure>::mangled(Scope * scope) {
        _Symbol<Procedure> * mangledSymbol = new _Symbol<Procedure>(name, _node, inst);
        mangledSymbol->name = mangledString(scope);
        return mangledSymbol;
    }
    
    template <>
    inline std::string _Symbol<TemplateProc>::mangledString(Scope * scope) {
        std::string _name = name;
        TemplateProc * template_proc = (TemplateProc*)node();
        TemplateDefineList * def = (TemplateDefineList*)template_proc->getTemplateDef();
        Procedure * proc = (Procedure*)template_proc->_template;
        
        _name = "_Z" + scope->mangledPrefix();
        if (template_proc->getFlag(TemplateProc::IS_TYPE_MEMBER))
            _name += mangledTypeMemberPrefix((Struct*)template_proc->parent);
        _name   += std::to_string(name.size()) + name
                + "M" + std::to_string(def->getElements().size())
                + mangledParams(proc, def);
        
        // @incomplete
        // what about type member prefix?
        return _name;
    }
    template <>
    inline Symbol * _Symbol<TemplateProc>::mangled(Scope * scope) {
        _Symbol<TemplateProc> * mangledSymbol = new _Symbol<TemplateProc>(name, _node);
        mangledSymbol->name = mangledString(scope);
        return mangledSymbol;
    }
    
    template <>
    inline std::string _Symbol<ProcSet>::mangledString(Scope * scope) {
        std::string _name = name;
        std::string prefix = "_Z" + scope->mangledPrefix() + std::to_string(_name.size());
        if (prefix.size() > std::strlen("_Z"))
            _name = prefix + _name;
        return _name;
    }
    template <>
    inline Symbol * _Symbol<ProcSet>::mangled(Scope * scope) {
        _Symbol<ProcSet> * mangledSymbol = new _Symbol<ProcSet>(name, _node);
        mangledSymbol->mangledString(scope);
        return mangledSymbol;
    }
    
    template <>
    inline std::string _Symbol<Constant>::mangledString(Scope * scope) {
        std::string _name = name;
        std::string prefix = scope->mangledPrefix();
        if (prefix.size() && scope->nspace)
            _name = "_Z" + prefix + std::to_string(_name.size()) + _name;
        return _name;
    }
    template <>
    inline Symbol * _Symbol<Constant>::mangled(Scope * scope) {
        _Symbol<Constant> * mangledSymbol = new _Symbol<Constant>(name, _node);
        mangledSymbol->name = mangledString(scope);
        return mangledSymbol;
    }
    
    template <>
    inline std::string _Symbol<VariableDeclaration>::mangledString(Scope * scope) {
        std::string _name = name;
        std::string prefix = scope->mangledPrefix();
        if (prefix.size() && scope->nspace)
            _name = "_Z" + prefix + std::to_string(_name.size()) + _name;
        return _name;
    }
    template <>
    inline Symbol * _Symbol<VariableDeclaration>::mangled(Scope * scope) {
        _Symbol<VariableDeclaration> * mangledSymbol = new _Symbol<VariableDeclaration>(name, _node);
        mangledSymbol->name = mangledString(scope);
        return mangledSymbol;
    }
}

#endif /* Symbol_hpp */
