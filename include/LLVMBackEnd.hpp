//
//  LLVMBackEnd.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef LLVMBackEnd_hpp
#define LLVMBackEnd_hpp

#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "ASTNode.hpp"
#include "BackEnd.hpp"
#include "LLVMGenerator.hpp"
#include "Type.hpp"

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

namespace bjou {
struct StackFrame {
	struct FrameVal {
		llvm::Value * val;
		const Type * type;
	};

	std::vector<FrameVal> vals;
	std::unordered_map<std::string, size_t> namedVals;
};
	
struct LLVMBackEnd : BackEnd {
    LLVMBackEnd(FrontEnd & _frontEnd);
    ~LLVMBackEnd();

    LLVMGenerator generator;

    std::string defaultTriple;
    const llvm::Target * defaultTarget;
    llvm::TargetMachine * defaultTargetMachine;
    llvm::DataLayout * layout;

    llvm::LLVMContext llContext;
    llvm::Module * llModule;
    llvm::IRBuilder<> builder;

	std::vector<StackFrame> frames;
    // this is kind of weird, but we map on TupleType->code as a key
    std::unordered_map<std::string, llvm::StructType *> createdTupleStructTypes;
    std::unordered_map<std::string, llvm::Type *> definedTypes;
    std::unordered_map<std::string, llvm::GlobalVariable *> globaltypeinfos;
    std::unordered_map<Constant *, llvm::Value *> generatedTypeMemberConstants;
    std::vector<Procedure *> procsGenerateDefinitions;
    std::vector<Struct *> structsGenerateDefinitions;
    Struct * typeinfo_struct;
    std::stack<llvm::BasicBlock *> loop_break_stack;
    std::stack<llvm::BasicBlock *> loop_continue_stack;

    milliseconds go();
	
	void pushFrame();
	void popFrame();
	StackFrame& curFrame();
	llvm::Value * addNamedVal(std::string name, llvm::Value * val, const Type * type);
	llvm::Value * addUnnamedVal(llvm::Value * val, const Type * type);
	llvm::Value * allocNamedVal(std::string name, const Type * type);
	llvm::Value * getNamedVal(std::string name);
	// llvm::Value * unnamedVal(llvm::Value * val, const Type * type);
	// llvm::Value * namedVal(std::string name, const Type * type = nullptr);
    // llvm::Value * namedVal(std::string name, llvm::Value * val, const Type * type);
    llvm::Type * bJouTypeToLLVMType(const bjou::Type * t);
    llvm::Type * createOrLookupDefinedType(const bjou::Type * t);
    llvm::StructType * createTupleStructType(const bjou::Type * t);

    void createllModule();
    void createMainEntryPoint();
    void createPrintfProto();
    llvm::Value * getPointerToArrayElements(llvm::Value * array);
    llvm::Value * createPointerToArrayElementsOnStack(llvm::Value * array);
    llvm::Constant * createConstantInitializer(InitializerList * ilist);
    llvm::Value *
    copyConstantInitializerToStack(llvm::Constant * constant_init);
    llvm::Value * createGlobalStringVariable(std::string str);
    llvm::ConstantArray * create_v_table_constant_array(Struct * s);
    llvm::Value * createGlobaltypeinfo(Struct * s);
    llvm::Value * getGlobaltypeinfo(Struct * s);

    milliseconds CodeGenStage();
    milliseconds LinkingStage();
};
} // namespace bjou

#endif /* LLVMBackEnd_hpp */
