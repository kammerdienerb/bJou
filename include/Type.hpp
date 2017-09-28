//
//  Type.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/8/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef Type_hpp
#define Type_hpp

#include "Maybe.hpp"

#include <string>
#include <unordered_map>
#include <vector>
#include <unordered_map>
#include <set>

namespace bjou {
    struct ASTNode;
    struct Procedure;
    struct Constant;
    struct Struct;
    struct Alias;
    struct Expression;
    struct Identifier;
    struct Declarator;
    struct TemplateInstantiation;
    struct Symbol;
    struct ProcSet;

    struct Type {
        enum Kind {
			INVALID,
            PLACEHOLDER,
            PRIMATIVE,
			BASE,
            STRUCT,
            ENUM,
            ALIAS,
            ARRAY,
            DYNAMIC_ARRAY,
            POINTER,
            MAYBE,
            TUPLE,
            PROCEDURE,
            TEMPLATE_STRUCT,
			TEMPLATE_ALIAS
		};
        
        enum Sign {
            NA,
            SIGNED,
            UNSIGNED
        };
        
        Kind kind;
        Sign sign;
        int size;
        std::string code;
        
        Type();
        Type (Kind _kind);
        Type(std::string _name);
        Type(Kind _kind, std::string _name, Sign _sign = NA, int _size = -1);
     	
		bool isValid() const;
        bool isPrimative() const;
        bool isFP() const;
        bool isStruct() const;
        bool isEnum() const;
        bool isAlias() const;
        bool isArray() const;
        bool isPointer() const;
        bool isMaybe() const;
        bool isTuple() const;
        bool isProcedure() const;
        bool isTemplateStruct() const;
		bool isTemplateAlias() const;
        
        bool enumerableEquivalent() const;
        
        // Type interface
        virtual const Type * getBase() const;
        virtual const Type * getOriginal() const;
        virtual bool equivalent(const Type * other, bool exactMatch = false) const;
        
        virtual Declarator * getGenericDeclarator() const;
        
        virtual std::string getDemangledName() const;
        
        virtual bool opApplies(std::string& op) const;
        virtual bool isValidOperand(const Type * operand, std::string& op) const;
        virtual const Type * binResultType(const Type * operand, std::string& op) const;
        virtual const Type * unResultType(std::string& op) const;
        
        virtual const Type * arrayOf() const;
        virtual const Type * pointerOf() const;
        virtual const Type * maybeOf() const;
        
        virtual const Type * replacePlaceholders(const Type * t) const;
        
        virtual ~Type();
        //
    };
   
	struct InvalidType : Type {
		InvalidType();
	};
    
    struct PlaceholderType : Type {
        PlaceholderType();
        
        // Type interface
        virtual bool equivalent(const Type * other, bool exactMatch = false) const;
        virtual Declarator * getGenericDeclarator() const;
        
        virtual const Type * replacePlaceholders(const Type * t) const;
        //
    };

    struct StructType : Type {
        bool isAbstract;
        Struct * _struct;
        TemplateInstantiation * inst;
        Type * extends;
        std::unordered_map<std::string, int> memberIndices;
        std::vector<const Type*> memberTypes;
        std::unordered_map<std::string, Constant*> constantMap;
        // std::unordered_map<std::string, std::vector<ASTNode*> > memberProcs;
        std::unordered_map<std::string, ProcSet*> memberProcs;
        std::set<std::string> interfaces;
        std::unordered_map<Procedure*, unsigned int> interfaceIndexMap;
        
        StructType();
        StructType(std::string& name, Struct * __struct = nullptr, TemplateInstantiation * _inst = nullptr);

        void complete();
        
		void setMemberType(int idx, const Type * t);

     	// Type interface
		bool opApplies(std::string& op) const;
        bool isValidOperand(const Type * operand, std::string& op) const;
        const Type * binResultType(const Type * operand, std::string& op) const;
        const Type * unResultType(std::string& op) const;
        
        virtual const Type * replacePlaceholders(const Type * t) const;
		//
    };
    
    struct EnumType : Type {
        EnumType();
        EnumType(std::string& name, ASTNode * __enum);

        std::unordered_map<std::string, int> valMap;
        
        int getVal(std::string identifier);
        
        // @incomplete
    };
    
    struct AliasType : Type {
        AliasType();
        AliasType(std::string& name, Alias * _alias);
		
        Alias * alias;
		const Type * alias_of;

        // Type interface
		const Type * getOriginal() const;
        bool equivalent(const Type * other, bool exactMatch = false) const;
        
        bool opApplies(std::string& op) const;
        bool isValidOperand(const Type * operand, std::string& op) const;
        const Type * binResultType(const Type * operand, std::string& op) const;
        const Type * unResultType(std::string& op) const;
        
        virtual const Type * replacePlaceholders(const Type * t) const;
		//
        
        void complete();
    };
    
    struct ArrayType : Type {
        ArrayType();
        ArrayType(const Type * _array_of);
        ArrayType(const Type * _array_of, Expression * _expression);

        const Type * array_of;
        Expression * expression;
        int size;
        
		// Type interface
        bool equivalent(const Type * other, bool exactMatch = false) const;
        
        const Type * getBase() const;
        Declarator * getGenericDeclarator() const;

        virtual std::string getDemangledName() const;
        
        bool opApplies(std::string& op) const;
        bool isValidOperand(const Type * operand, std::string& op) const;
        const Type * binResultType(const Type * operand, std::string& op) const;
        const Type * unResultType(std::string& op) const;
        
        virtual const Type * replacePlaceholders(const Type * t) const;
		//
    };
    
    struct DynamicArrayType : Type {
        DynamicArrayType();
        DynamicArrayType(const Type * _array_of);
        
        const Type * array_of;
        
        // Type interface
        bool equivalent(const Type * other, bool exactMatch = false) const;
        
        const Type * getBase() const;
        Declarator * getGenericDeclarator() const;
        
        virtual std::string getDemangledName() const;
        
        bool opApplies(std::string& op) const;
        bool isValidOperand(const Type * operand, std::string& op) const;
        const Type * binResultType(const Type * operand, std::string& op) const;
        const Type * unResultType(std::string& op) const;
        
        virtual const Type * replacePlaceholders(const Type * t) const;
        //
    };
    
    struct PointerType : Type {
        PointerType();
        PointerType(const Type * _pointer_of);

        const Type * pointer_of;
        
		// Type interface
        bool equivalent(const Type * other, bool exactMatch = false) const;
        
        const Type * getBase() const;
        Declarator * getGenericDeclarator() const;
       
        virtual std::string getDemangledName() const;
        
        bool opApplies(std::string& op) const;
        bool isValidOperand(const Type * operand, std::string& op) const;
        const Type * binResultType(const Type * operand, std::string& op) const;
        const Type * unResultType(std::string& op) const;
        
        virtual const Type * replacePlaceholders(const Type * t) const;
		//
    };
    
    struct MaybeType : Type {
        MaybeType();
        MaybeType(const Type * _maybe_of);

        const Type * maybe_of;
        
		// Type interface
        const Type * getBase() const;
        Declarator * getGenericDeclarator() const;
       
        virtual std::string getDemangledName() const;
        
        bool opApplies(std::string& op) const;
        bool isValidOperand(const Type * operand, std::string& op) const;
        const Type * binResultType(const Type * operand, std::string& op) const;
        const Type * unResultType(std::string& op) const;
        
        virtual const Type * replacePlaceholders(const Type * t) const;
		//
    };
    
    struct TupleType : Type {
        TupleType();
        TupleType(std::vector<const Type*> _subTypes);

        std::vector<const Type*> subTypes;
        
		// Type interface
        bool equivalent(const Type * other, bool exactMatch = false) const;
        
        Declarator * getGenericDeclarator() const;
       
        virtual std::string getDemangledName() const;
        
        bool opApplies(std::string& op) const;
        bool isValidOperand(const Type * operand, std::string& op) const;
        const Type * binResultType(const Type * operand, std::string& op) const;
        const Type * unResultType(std::string& op) const;
        
        virtual const Type * replacePlaceholders(const Type * t) const;
		// 
    };
    
    struct ProcedureType : Type {
        ProcedureType();
        ProcedureType(std::vector<const Type*> _paramTypes, const Type * _retType, bool _isVararg = false);
        
        std::vector<const Type*> paramTypes;
        bool isVararg;
        const Type * retType;
                
        bool argMatch(const Type * _other, bool exactMatch = false);

		// Type interface
        bool equivalent(const Type * other, bool exactMatch = false) const;
        Declarator * getGenericDeclarator() const;
       
        virtual std::string getDemangledName() const;
        
        bool opApplies(std::string& op) const;
        bool isValidOperand(const Type * operand, std::string& op) const;
        const Type * binResultType(const Type * operand, std::string& op) const;
        const Type * unResultType(std::string& op) const;
        
        virtual const Type * replacePlaceholders(const Type * t) const;
		// 
    };
    
    struct TemplateStructType : Type {
        TemplateStructType();
        TemplateStructType(std::string& name);
        
        /*
        // Type interface
		const Type * getOriginal() const;
        bool equivalent(const Type * other, bool exactMatch = false) const;
        
        Declarator * getGenericDeclarator() const;
         
        bool opApplies(std::string& op) const;
        bool isValidOperand(const Type * operand, std::string& op) const;
        const Type * binResultType(const Type * operand, std::string& op) const;
        const Type * unResultType(std::string& op) const;
		// 
         */
        
        
    };
	
	struct TemplateAliasType : Type {
        TemplateAliasType();
        TemplateAliasType(std::string& name);
        
        /*
        // Type interface
		const Type * getOriginal() const;
        bool equivalent(const Type * other, bool exactMatch = false) const;
        
        Declarator * getGenericDeclarator() const;
         
        bool opApplies(std::string& op) const;
        bool isValidOperand(const Type * operand, std::string& op) const;
        const Type * binResultType(const Type * operand, std::string& op) const;
        const Type * unResultType(std::string& op) const;
		// 
         */
        
        
    };
    
    
    void compilationAddPrimativeTypes();
    
    const Type * primativeConversionResult(const Type * l, const Type * r);

	void createCompleteType(Symbol * sym);
    
    std::vector<const Type*> typesSortedByDepencencies(std::vector<const Type*> nonPrimatives);

}
#endif /* Type_hpp */
