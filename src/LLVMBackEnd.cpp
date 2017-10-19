//
//  LLVMBackEnd.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "LLVMBackEnd.hpp"

#include <algorithm>
#include <unordered_map>

#include <lld/Driver/Driver.h>

#include <llvm/CodeGen/CommandFlags.h>
#include <llvm/Target/TargetMachine.h>

#include "Compile.hpp"
#include "Global.hpp"
#include "CLI.hpp"
#include "LLVMGenerator.hpp"
#include "ASTNode.hpp"
#include "FrontEnd.hpp"
#include "Type.hpp"
#include "Misc.hpp"

#ifdef BJOU_DEBUG_BUILD
#define SAVE_BJOU_DEBUG_BUILD
#endif
#undef BJOU_DEBUG_BUILD
#include "tclap/CmdLine.h"
#ifdef SAVE_BJOU_DEBUG_BUILD
#define BJOU_DEBUG_BUILD
#endif

namespace bjou {
    LLVMBackEnd::LLVMBackEnd(FrontEnd& _frontEnd) : BackEnd(_frontEnd), generator(*this), builder(llContext) {
        defaultTarget = nullptr;
        defaultTargetMachine = nullptr;
        layout = nullptr;
        llModule = nullptr;
    }
    
    LLVMBackEnd::~LLVMBackEnd() {
        if (llModule)
            delete llModule;
        if (layout)
            delete layout;
    }
    
    milliseconds LLVMBackEnd::go() {
        auto start = Clock::now();
        
        auto cg_time = CodeGenStage();
        if (compilation->args.time_arg.getValue())
            prettyPrintTimeMin(cg_time, std::string("Code Generation") + (compilation->args.opt_arg.getValue() ? " and Optimization" : ""));
        
        auto l_time = LinkingStage();
        if (compilation->args.time_arg.getValue())
            prettyPrintTimeMin(l_time, "Linking");
        
        auto end = Clock::now();
        return duration_cast<milliseconds>(end - start);
    }
    
    milliseconds LLVMBackEnd::CodeGenStage() {
        auto start = Clock::now();
        
        // llvm initialization for object code gen
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmParsers();
        llvm::InitializeAllAsmPrinters();
        
        // set up target and triple
        defaultTriple = llvm::sys::getDefaultTargetTriple();
        std::string targetErr;
        defaultTarget = llvm::TargetRegistry::lookupTarget(defaultTriple, targetErr);
        if (!defaultTarget) {
            error(Context(), "Could not create llvm default Target.", false, targetErr);
            internalError("There was an llvm error.");
        }
        
        llvm::TargetOptions Options = InitTargetOptionsFromCodeGenFlags();
        
        llvm::CodeGenOpt::Level OLvl = (compilation->args.opt_arg.getValue() ? llvm::CodeGenOpt::Aggressive : llvm::CodeGenOpt::None);
    
        defaultTargetMachine = defaultTarget->createTargetMachine(defaultTriple, "generic", "", Options, getRelocModel(), getCodeModel(), OLvl);
        layout = new llvm::DataLayout(defaultTargetMachine->createDataLayout());
        
        // set up default target machine
        std::string CPU = "generic";
        std::string Features = "";
        llvm::TargetOptions opt;
        auto RM = llvm::Optional<llvm::Reloc::Model>();
        defaultTargetMachine = defaultTarget->createTargetMachine(defaultTriple, CPU, Features, opt, RM);
        
        createllModule();
        
        generator.generate();
        
        if (compilation->args.verbose_arg.getValue())
            llModule->dump();
        
        auto end = Clock::now();
        return duration_cast<milliseconds>(end - start);
    }
    
    milliseconds LLVMBackEnd::LinkingStage() {
        auto start = Clock::now();

	    std::string dest = compilation->outputpath + compilation->outputbasefilename;
	    std::string dest_o = dest + ".o";
	
		bool use_system_linker = true;
		
		if (compilation->args.lld_arg.getValue()) {
			std::string errstr;
	        llvm::raw_string_ostream errstream(errstr);
	        
	        std::vector<const char*> link_args = { "-demangle", "-dynamic", "-arch", "x86_64", "-macosx_version_min", "10.12.0", "-o", dest.c_str(), dest_o.c_str(), "-lSystem", "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/../lib/clang/8.1.0/lib/darwin/libclang_rt.osx.a" };
	        
	        bool success = lld::mach_o::link(link_args, errstream);
	        
	        if (!success) {
	            if (errstream.str().find("warning") == 0) {
	                warning(COMPILER_SRC_CONTEXT(), "LLD:", errstream.str());
	            } else {
	                error(COMPILER_SRC_CONTEXT(), "LLD:", false, errstream.str());
	                // internalError("There was an lld error.");
	            }
				warning(COMPILER_SRC_CONTEXT(), "lld failed. Falling back to system linker.");
				use_system_linker = true;
	        } else use_system_linker = false;
		}

		if (use_system_linker) {
			const char * cc = getenv("CC");
			if (!cc)
				cc = "cc";
			std::vector<const char*> args { cc, dest_o.c_str(), "-o", dest.c_str(), NULL };

			int fds[2];
			int status;
			if (pipe(fds) == -1)
				internalError("Error creating pipe to system linker.");

			pid_t childpid = fork();
			if (childpid == 0) {
				dup2(fds[1], STDERR_FILENO);
            	close(fds[0]);
            	execvp(args[0], (char**)args.data());
				internalError("Error executing system linker.");
			} else if (childpid == -1) {
				internalError("Error forking system linker.");
			}

			close(fds[1]);
			waitpid(childpid, &status, 0);

			if (WIFEXITED(status)) {
	            status = WEXITSTATUS(status);
	            if (status) {
					char errout[4096];
					int n = read(fds[0], errout, sizeof(errout));
					errout[n] = '\0';
					std::string s_errout(errout);
					error(COMPILER_SRC_CONTEXT(), "Linker error.", false, s_errout);
				}
	        }
		}
		
                
        auto end = Clock::now();
        return duration_cast<milliseconds>(end - start);
    }
    
    void LLVMBackEnd::namedValsPushFrame() {
        if (namedValsStack.empty())
            namedValsStack.push({  });
        else namedValsStack.push(namedValsStack.top());
    }
    
    void LLVMBackEnd::namedValsPopFrame() { namedValsStack.pop(); }
    
    llvm::Value * LLVMBackEnd::namedVal(std::string name, llvm::Type * t) {
        BJOU_DEBUG_ASSERT(!namedValsStack.empty());
        
        if (namedValsStack.top().count(name))
            return namedValsStack.top()[name];
        
        if (!t)
            internalError("namedVal() could not find the value specified, '" + name + "'");
        
        llvm::Value * alloca = builder.CreateAlloca(t, 0, name);
        
        if (t->isArrayTy()) {
            llvm::Value * ptr = builder.CreateAlloca(t->getArrayElementType()->getPointerTo());
            builder.CreateStore(
                    builder.CreateInBoundsGEP(alloca,
                                { llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llContext)),
                                  llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llContext)) }),
                    ptr);
            namedValsStack.top()[name] = ptr;
            return ptr;
        }
        
        namedValsStack.top()[name] = alloca;
        return alloca;
    }
    
    llvm::Value * LLVMBackEnd::namedVal(std::string name, llvm::Value * val) {
        BJOU_DEBUG_ASSERT(!namedValsStack.empty());
        
        namedValsStack.top()[name] = val;
        return val;
    }
    
    llvm::Type * LLVMBackEnd::bJouTypeToLLVMType(const bjou::Type * t) {
        switch (t->kind) {
            case Type::INVALID:
            case Type::PLACEHOLDER:
                internalError("Invalid type in llvm generation.");
                break;
            case Type::PRIMATIVE:
                if (t->isFP()) {
                    return llvm::Type::getDoubleTy(llContext);
                    /*if (t->size == 32)
                        return llvm::Type::getFloatTy(llContext);
                    else if (t->size == 64)
                        return llvm::Type::getDoubleTy(llContext);*/
                } else if (t->size == -1) {
                    return llvm::Type::getVoidTy(llContext);
                } else {
                    return llvm::IntegerType::get(llContext, t->size);
                }
                break;
            case Type::BASE:
            case Type::STRUCT:
                // return createOrLookupDefinedType(t);
                return definedTypes[t->code];
                break;
            case Type::ENUM:
                // @incomplete
                break;
            case Type::ALIAS:
                return bJouTypeToLLVMType(((AliasType*)t)->getOriginal());
                break;
            case Type::ARRAY: {
                ArrayType * array_t = (ArrayType*)t;
                return llvm::ArrayType::get(bJouTypeToLLVMType(array_t->array_of), array_t->size);
                break;
            } case Type::DYNAMIC_ARRAY:
                BJOU_DEBUG_ASSERT(false && "Dynamic arrays not implemented yet.");
            case Type::POINTER: {
                const PointerType * p_t = (const PointerType*)t;
                if (p_t->pointer_of->isPrimative() && p_t->pointer_of->size == -1) {
                    const PointerType * u8ptr = (const PointerType*)compilation->frontEnd.typeTable["u8"]->pointerOf();
                    llvm::Type * r_t = bJouTypeToLLVMType(u8ptr);
                    delete u8ptr;
                    return r_t;
                }
                return bJouTypeToLLVMType(((PointerType*)t)->pointer_of)->getPointerTo();
                break;
            } case Type::MAYBE:
                // @incomplete
                break;
            case Type::TUPLE:
                if (createdTupleStructTypes.count(t->code) == 0)
                    return createTupleStructType(t);
                else return createdTupleStructTypes[t->code];
                break;
            case Type::PROCEDURE: {
                ProcedureType * proc_t = (ProcedureType*)t;
                std::vector<llvm::Type*> paramTypes;
                for (const Type * pt : proc_t->paramTypes)
                    paramTypes.push_back(bJouTypeToLLVMType(pt));
                return llvm::FunctionType::get(bJouTypeToLLVMType(proc_t->retType), paramTypes, proc_t->isVararg)->getPointerTo();
                break;
            }
            case Type::TEMPLATE_STRUCT:
            case Type::TEMPLATE_ALIAS:
                internalError("Template type in llvm generation.");
                break;
        }
        
        BJOU_DEBUG_ASSERT(false && "Did not convert bJou type to LLVM type.");
        return nullptr;
    }
    
    llvm::Type * LLVMBackEnd::createOrLookupDefinedType(const bjou::Type * t) {
        // @incomplete
        return nullptr;
    }
    
    llvm::StructType * LLVMBackEnd::createTupleStructType(const bjou::Type * t) {
        BJOU_DEBUG_ASSERT(t->isTuple());
        TupleType * t_t = (TupleType*)t;
        std::string name = compilation->frontEnd.makeUID("tuple");
        llvm::StructType * ll_t = llvm::StructType::create(llContext, name);
        std::vector<llvm::Type*> sub_types;
        for (const Type * sub_t : t_t->subTypes)
            sub_types.push_back(bJouTypeToLLVMType(sub_t));
        ll_t->setBody(sub_types);
        createdTupleStructTypes[t_t->code] = ll_t;
        return ll_t;
    }
    
    static void * GenerateGlobalVariable(VariableDeclaration * var, BackEnd& backEnd, bool flag = false);
    
    void LLVMBackEnd::createllModule() {
        llModule = new llvm::Module(compilation->outputbasefilename, llContext);
        
        namedValsPushFrame();
        
        // moved to preload
        // createPrintfProto();
        
        // do the actual bjou AST -> LLVM IR
        std::vector<ASTNode*> nodes;
        for (ASTNode * node : compilation->frontEnd.AST) {
            if (!node->getFlag(ASTNode::IGNORE_GEN)) {
                if (node->nodeKind == ASTNode::NAMESPACE) {
                    std::vector<ASTNode*> nspacenodes = ((Namespace*)node)->gather();
                    std::vector<ASTNode*> filtered;
                    std::copy_if(nspacenodes.begin(), nspacenodes.end(), std::back_inserter(filtered), [](ASTNode * node) {
                        return !node->getFlag(ASTNode::IGNORE_GEN);
                    });
                    nodes.insert(nodes.end(), filtered.begin(), filtered.end());
                } else nodes.push_back(node);
            }
        }
        
        // type forwards
        // type aliases
        // procedure types
        // type definitions
        // proc declarations
        // other nodes??
        // interface initializers
        // global vars and constants
        // procedure definitions
        
        // @bad: don't iterate this much
        // type forwards
        for (ASTNode * node : nodes)
            if (node->nodeKind == ASTNode::STRUCT)
                ((Struct*)node)->Struct::generate(*this);
        // type aliases
        // procedure types
        // type definitions
        typeinfo_struct->generate(*this);
        for (Struct * s : structsGenerateDefinitions)
            s->generate(*this);
        // member proc declarations (including interfaces)
        const StructType * typeinfo_t = (const StructType*)typeinfo_struct->getType();
        for (auto& set : typeinfo_t->memberProcs) {
            for (auto& _proc : set.second->procs) {
                Procedure * proc = (Procedure*)_proc.second->node();
                if (!_proc.second->isTemplateProc() && proc->getParentStruct() == typeinfo_struct && !proc->getFlag(Procedure::IS_INTERFACE_DECL)) {
                    proc->generate(*this);
                }
            }
        }
        for (Struct * s : structsGenerateDefinitions) {
            const StructType * s_t = (const StructType*)s->getType();
            for (auto& set : s_t->memberProcs) {
                for (auto& _proc : set.second->procs) {
                    Procedure * proc = (Procedure*)_proc.second->node();
                    if (!_proc.second->isTemplateProc() && proc->getParentStruct() == s && !proc->getFlag(Procedure::IS_INTERFACE_DECL)) {
                        proc->generate(*this);
                    }
                }
            }
        }
        // proc declarations
        for (ASTNode * node : nodes)
            if (node->nodeKind == ASTNode::PROCEDURE)
                ((Procedure*)node)->Procedure::generate(*this);
        // create typeinfos
        createGlobaltypeinfo(typeinfo_struct);
        for (Struct * s : structsGenerateDefinitions)
            createGlobaltypeinfo(s);
        // other nodes??
        for (ASTNode * node : nodes) {
            if (node->nodeKind == ASTNode::CONSTANT)
                ((Constant*)node)->Constant::generate(*this);
            else if (node->nodeKind == ASTNode::VARIABLE_DECLARATION)
                GenerateGlobalVariable((VariableDeclaration*)node, *this);
        }
        // interface initializers
        // global vars and constants
        // procedure definitions
        for (Procedure * proc : procsGenerateDefinitions)
            proc->generate(*this);
        
        createMainEntryPoint();
        
        namedValsPopFrame();
        
        std::string errstr;
        llvm::raw_string_ostream errstream(errstr);
        if (llvm::verifyModule(*llModule, &errstream)) {
            llModule->dump();
            error(COMPILER_SRC_CONTEXT(), "LLVM:", true, errstream.str());
            internalError("There was an llvm error.");
        }
        
        llModule->setDataLayout(defaultTargetMachine->createDataLayout());
        llModule->setTargetTriple(defaultTriple);
    }
    
    void LLVMBackEnd::createMainEntryPoint() {
        std::vector<llvm::Type*> main_arg_types = {
            llvm::Type::getInt32Ty(llContext),
            llvm::PointerType::get(llvm::Type::getInt8PtrTy(llContext), 0)
        };
        
        llvm::FunctionType * main_t = llvm::FunctionType::get(llvm::Type::getInt32Ty(llContext), main_arg_types, false);
        llvm::Function * func = llvm::Function::Create(main_t, llvm::Function::ExternalLinkage, "main", llModule);
        
        llvm::BasicBlock * bblock = llvm::BasicBlock::Create(llContext, "entry", func);
        builder.SetInsertPoint(bblock);
        
        // namedValsPushFrame();
        
        int i = 0;
        for (auto& arg : func->args())
            namedVal("__bjou_main_arg" + std::to_string(i), &arg);
        
        for (ASTNode * node : compilation->frontEnd.AST)
            if (node->isStatement())
                node->generate(*this);
        
        builder.CreateRet(llvm::ConstantInt::get(llContext, llvm::APInt(32, 0, true)));
        
        std::string errstr;
        llvm::raw_string_ostream errstream(errstr);
        if (llvm::verifyFunction(*func, &errstream)) {
            func->dump();
            error(COMPILER_SRC_CONTEXT(), "LLVM:", true, errstream.str());
            internalError("There was an llvm error.");
        }
    }
    
    void LLVMBackEnd::createPrintfProto() {
        std::vector<llvm::Type*> printf_arg_types = { llvm::Type::getInt8PtrTy(llContext) };
        
        llvm::FunctionType* printf_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(llContext), printf_arg_types, true);
        
        llvm::Function *func = llvm::Function::Create(printf_type, llvm::Function::ExternalLinkage, "printf", llModule);
        func->setCallingConv(llvm::CallingConv::C);
        
        std::string errstr;
        llvm::raw_string_ostream errstream(errstr);
        if (llvm::verifyFunction(*func, &errstream)) {
            func->dump();
            error(COMPILER_SRC_CONTEXT(), "LLVM:", true, errstream.str());
            internalError("There was an llvm error.");
        }
    }
    
    
    llvm::Value * LLVMBackEnd::createGlobalStringVariable(std::string str) {
        llvm::ArrayType * var_t = llvm::ArrayType::get(llvm::Type::getInt8Ty(llContext), str.size() + 1);
        llvm::GlobalVariable * global_var = new llvm::GlobalVariable(/*Module=*/*llModule,
                                                             /*Type=*/var_t,
                                                             /*isConstant=*/true,
                                                                         /*Linkage=*/llvm::GlobalValue::PrivateLinkage,
                                                             /*Initializer=*/0, // has initializer, specified below
                                                             /*Name=*/".str");
        global_var->setAlignment(1);
        global_var->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
        
        llvm::Constant * str_constant = llvm::ConstantDataArray::getString(llContext, str.c_str(), true);
        global_var->setInitializer(str_constant);
        
        llvm::Value * indices[2] = {
            llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llContext)),
            llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llContext))
        };
        
        return llvm::ConstantExpr::getGetElementPtr(var_t, global_var, indices, true);
    }
    
    llvm::ConstantArray * LLVMBackEnd::create_v_table_constant_array(Struct * s) {
        StructType * s_t = (StructType*)s->getType();
        llvm::PointerType * ll_byte_ptr_t = llvm::Type::getInt8Ty(llContext)->getPointerTo();
        std::vector<llvm::Constant*> vals(compilation->max_interface_procs, llvm::ConstantPointerNull::get(ll_byte_ptr_t));
    
        int i = 0;
        
        for (ASTNode * _impl : s->getAllInterfaceImplsSorted()) {
            InterfaceImplementation * impl = (InterfaceImplementation*)_impl;
            
            for (auto& procs : impl->getProcs()) {
                for (auto& _proc : procs.second) {
                    if (!impl->getFlag(InterfaceImplementation::PUNT_TO_EXTENSION)) {
                        Procedure * proc = (Procedure*)_proc;
                        llvm::Constant * fn_val = (llvm::Constant*)namedVal(proc->getMangledName());
                        BJOU_DEBUG_ASSERT(fn_val);
                        llvm::Constant * cast = (llvm::Constant*)builder.CreateBitCast(fn_val, ll_byte_ptr_t);
                        vals[i] = cast;
                    }
                    i += 1;
                }
            }
        }
    
        StructType * typeinfo_t = (StructType*)compilation->frontEnd.typeTable["typeinfo"];
        
        return (llvm::ConstantArray*)llvm::ConstantArray::get((llvm::ArrayType*)bJouTypeToLLVMType(typeinfo_t->memberTypes[typeinfo_t->memberIndices["_v_table"]]), vals);
    }
    
    llvm::Value * LLVMBackEnd::createGlobaltypeinfo(Struct * s) {
        StructType * typeinfo_t = (StructType*)compilation->frontEnd.typeTable["typeinfo"];
        llvm::StructType * ll_typeinfo_t = (llvm::StructType*)bJouTypeToLLVMType(typeinfo_t);
        BJOU_DEBUG_ASSERT(typeinfo_t);
        llvm::GlobalVariable * global_var = new llvm::GlobalVariable(/*Module=*/*llModule,
                                                                     /*Type=*/ll_typeinfo_t,
                                                                     /*isConstant=*/true,
                                                                     /*Linkage=*/llvm::GlobalValue::PrivateLinkage,
                                                                     /*Initializer=*/0, // has initializer, specified below
                                                                     /*Name=*/"__bjou_typeinfo_" + s->getMangledName());
        
        global_var->setAlignment(layout->getPreferredAlignment(global_var));
        global_var->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
        
        std::vector<llvm::Constant*> vals;
        
        // _v_table
        BJOU_DEBUG_ASSERT(typeinfo_t->memberIndices.count("_v_table"));
        BJOU_DEBUG_ASSERT(typeinfo_t->memberIndices["_v_table"] == 0);
        const Type * _v_table_t = typeinfo_t->memberTypes[0];
        BJOU_DEBUG_ASSERT(_v_table_t->isArray() && ((ArrayType*)_v_table_t)->array_of->isPointer());
        
        vals.push_back(create_v_table_constant_array(s));
        
        // _typename
        BJOU_DEBUG_ASSERT(typeinfo_t->memberIndices.count("_typename"));
        BJOU_DEBUG_ASSERT(typeinfo_t->memberIndices["_typename"] == 1);
        const Type * _typename_t = typeinfo_t->memberTypes[1];
        BJOU_DEBUG_ASSERT(_typename_t->isPointer() && ((PointerType*)_typename_t)->pointer_of->size == 8);
        
        vals.push_back((llvm::Constant*)createGlobalStringVariable(demangledString(s->getMangledName())));
        
        
        llvm::Constant * init = llvm::ConstantStruct::get(ll_typeinfo_t, vals);
        global_var->setInitializer(init);
        
        globaltypeinfos[s->getMangledName()] = global_var;
        
        return global_var;
    }
    
    llvm::Value * LLVMBackEnd::getGlobaltypeinfo(Struct * s) {
        std::string key = s->getMangledName();
        BJOU_DEBUG_ASSERT(globaltypeinfos.count(key));
        return globaltypeinfos[key];
    }
    
    
    
    void * ASTNode::generate(BackEnd& backEnd, bool flag) {
        errorl(getContext(), "unimplemented generate()", false);
        internalError("Code generation failed.");
        return nullptr;
    }
    
    void * BinaryExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        // typedef llvm::Value* (llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>::*instrCreateFn)(llvm::Value*, llvm::Value*, const llvm::Twine&, bool, bool);
        
        const Type * lt = getLeft()->getType();
        const Type * rt = getRight()->getType();
        
        llvm::Value * lval = (llvm::Value*)getLeft()->generate(backEnd);
        llvm::Value * rval = (llvm::Value*)getRight()->generate(backEnd);
        
        // VERY @incomplete
        
        if (lt->isPrimative() && lt->equivalent(rt)) {
            if (lt->enumerableEquivalent()) {
                switch (getContents()[0]) {
                    case '+':
                        return llbe->builder.CreateAdd(lval, rval);
                        break;
                    case '-':
                        return llbe->builder.CreateSub(lval, rval);
                        break;
                    case '*':
                        return llbe->builder.CreateMul(lval, rval);
                        break;
                    case '/':
                        return llbe->builder.CreateSDiv(lval, rval);
                        break;
                    case '%':
                        return llbe->builder.CreateSRem(lval, rval);
                        break;
                    case '<':
                        return llbe->builder.CreateICmpSLT(lval, rval);
                        break;
                    case '>':
                        return llbe->builder.CreateICmpSGT(lval, rval);
                        break;
                }
            } else {
                switch (getContents()[0]) {
                    case '+':
                        return llbe->builder.CreateFAdd(lval, rval);
                        break;
                    case '-':
                        return llbe->builder.CreateFSub(lval, rval);
                        break;
                    case '*':
                        return llbe->builder.CreateFMul(lval, rval);
                        break;
                    case '/':
                        return llbe->builder.CreateFDiv(lval, rval);
                        break;
                    case '%':
                        return llbe->builder.CreateFRem(lval, rval);
                        break;
                    case '<':
                        return llbe->builder.CreateFCmpOLT(lval, rval);
                        break;
                    case '>':
                        return llbe->builder.CreateFCmpOGT(lval, rval);
                        break;
                }
            }
        }
        BJOU_DEBUG_ASSERT(false);
        return nullptr;
    }
   
    void * AddExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * myt = getType();
        const Type * lt = getLeft()->getType();
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd);
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        
        if (myt->isPointer()) {
            std::vector<llvm::Value*> indices;
            if (lt->isPointer()) {
                indices.push_back(rv);
                return llbe->builder.CreateInBoundsGEP(lv, indices);
            } else {
                indices.push_back(lv);
                return llbe->builder.CreateInBoundsGEP(rv, indices);
            }
        } else if (myt->enumerableEquivalent()) {
            return llbe->builder.CreateAdd(lv, rv);
        } else if (myt->isFP()) {
            return llbe->builder.CreateFAdd(lv, rv);
        }
        
        BJOU_DEBUG_ASSERT(false);
        return nullptr;
    }
    
    void * SubExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * myt = getType();
        const Type * lt = getLeft()->getType();
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd);
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        if (myt->isPointer()) {
            if (lt->isPointer()) {
                std::vector<llvm::Value*> indices;
                indices.push_back(llbe->builder.CreateNeg(rv));
                return llbe->builder.CreateInBoundsGEP(lv, indices);
            } else BJOU_DEBUG_ASSERT(false);
        } else if (myt->enumerableEquivalent()) {
            return llbe->builder.CreateSub(lv, rv);
        } else if (myt->isFP()) {
            return llbe->builder.CreateFSub(lv, rv);
        }
        
        BJOU_DEBUG_ASSERT(false);
        return nullptr;
    }
   
    void * MultExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * myt = getType();
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd);
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        if (myt->enumerableEquivalent()) {
            return llbe->builder.CreateMul(lv, rv);
        } else if (myt->isFP()) {
            return llbe->builder.CreateFMul(lv, rv);
        }
        
        BJOU_DEBUG_ASSERT(false);
        return nullptr;
    }
    
    void * DivExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * myt = getType();
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd);
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        if (myt->enumerableEquivalent()) {
            if (myt->sign == Type::SIGNED)
                return llbe->builder.CreateSDiv(lv, rv);
            else return llbe->builder.CreateUDiv(lv, rv);
        } else if (myt->isFP()) {
            return llbe->builder.CreateFDiv(lv, rv);
        }
        
        BJOU_DEBUG_ASSERT(false);
        return nullptr;
    }
    
    void * ModExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * myt = getType();
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd);
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        if (myt->enumerableEquivalent()) {
            if (myt->sign == Type::SIGNED)
                return llbe->builder.CreateSRem(lv, rv);
            else return llbe->builder.CreateURem(lv, rv);
        } else if (myt->isFP()) {
            return llbe->builder.CreateFRem(lv, rv);
        }
        
        BJOU_DEBUG_ASSERT(false);
        return nullptr;
    }
    
    void * AssignmentExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd, true);
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        return llbe->builder.CreateStore(rv, lv);
    }
    
    void * AddAssignExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd, true);
        llvm::Value * addv = (llvm::Value*)((AddExpression*)this)->AddExpression::generate(backEnd);
        
        return llbe->builder.CreateStore(addv, lv);
    }
    
    void * SubAssignExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd, true);
        
        return llbe->builder.CreateStore((llvm::Value*)((SubExpression*)this)->SubExpression::generate(backEnd), lv);
    }
    
    void * MultAssignExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd, true);
        
        return llbe->builder.CreateStore((llvm::Value*)((MultExpression*)this)->MultExpression::generate(backEnd), lv);
    }
    
    void * DivAssignExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd, true);
        
        return llbe->builder.CreateStore((llvm::Value*)((DivExpression*)this)->DivExpression::generate(backEnd), lv);
    }
    
    void * ModAssignExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd, true);
        
        return llbe->builder.CreateStore((llvm::Value*)((ModExpression*)this)->ModExpression::generate(backEnd), lv);
    }
    
    void * MaybeAssignExpression::generate(BackEnd& backEnd, bool flag) {
        // @incomplete
        return nullptr;
    }
    
    void * LssExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * myt = primativeConversionResult(getLeft()->getType(), getRight()->getType());
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd);
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        if (myt->enumerableEquivalent()) {
            if (myt->sign == Type::SIGNED)
                return llbe->builder.CreateICmpSLT(lv, rv);
            else return llbe->builder.CreateICmpULT(lv, rv);
        } else if (myt->isFP()) {
            return llbe->builder.CreateFCmpULT(lv, rv);
        }
        
        BJOU_DEBUG_ASSERT(false);
        return nullptr;
    }
    
    void * LeqExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * myt = primativeConversionResult(getLeft()->getType(), getRight()->getType());
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd);
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        if (myt->enumerableEquivalent()) {
            if (myt->sign == Type::SIGNED)
                return llbe->builder.CreateICmpSLE(lv, rv);
            else return llbe->builder.CreateICmpULE(lv, rv);
        } else if (myt->isFP()) {
            return llbe->builder.CreateFCmpULE(lv, rv);
        }
        
        BJOU_DEBUG_ASSERT(false);
        return nullptr;
    }
    
    void * GtrExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * myt = primativeConversionResult(getLeft()->getType(), getRight()->getType());
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd);
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        if (myt->enumerableEquivalent()) {
            if (myt->sign == Type::SIGNED)
                return llbe->builder.CreateICmpSGT(lv, rv);
            else return llbe->builder.CreateICmpUGT(lv, rv);
        } else if (myt->isFP()) {
            return llbe->builder.CreateFCmpUGT(lv, rv);
        }
        
        BJOU_DEBUG_ASSERT(false);
        return nullptr;
    }
    
    void * GeqExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * myt = primativeConversionResult(getLeft()->getType(), getRight()->getType());
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd);
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        if (myt->enumerableEquivalent()) {
            if (myt->sign == Type::SIGNED)
                return llbe->builder.CreateICmpSGE(lv, rv);
            else return llbe->builder.CreateICmpUGE(lv, rv);
        } else if (myt->isFP()) {
            return llbe->builder.CreateFCmpUGE(lv, rv);
        }
        
        BJOU_DEBUG_ASSERT(false);
        return nullptr;
    }
    
    void * EquExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * lt = getLeft()->getType();
        // const Type * rt = getRight()->getType();
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd);
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        if (lt->enumerableEquivalent()) {
            return llbe->builder.CreateICmpEQ(lv, rv);
        } else if (lt->isFP()) {
            return llbe->builder.CreateFCmpUEQ(lv, rv);
        } else if (lt->isPointer()) {
            return llbe->builder.CreateICmpEQ(lv, rv);
        }
        
        BJOU_DEBUG_ASSERT(false);
        return nullptr;
    }
    
    void * NeqExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * lt = getLeft()->getType();
        // const Type * rt = getRight()->getType();
        
        llvm::Value * lv = (llvm::Value*)getLeft()->generate(backEnd);
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        if (lt->enumerableEquivalent()) {
            return llbe->builder.CreateICmpNE(lv, rv);
        } else if (lt->isFP()) {
            return llbe->builder.CreateFCmpUNE(lv, rv);
        } else if (lt->isPointer()) {
            return llbe->builder.CreateICmpNE(lv, rv);
        }
        
        BJOU_DEBUG_ASSERT(false);
        return nullptr;
    }
    
    void * LogAndExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Function * func = llbe->builder.GetInsertBlock()->getParent();

        llvm::Value * cond1 = (llvm::Value*)getLeft()->generate(backEnd);
        cond1 = llbe->builder.CreateICmpEQ(cond1, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)), "lhs_cond");
        
        // Create blocks for the then and else cases.  Insert the 'then' block at the
        // end of the function.
        llvm::BasicBlock * then = llvm::BasicBlock::Create(llbe->llContext, "then", func);
        llvm::BasicBlock * condfalse = llvm::BasicBlock::Create(llbe->llContext, "condfalse", func);
        llvm::BasicBlock * mergephi = llvm::BasicBlock::Create(llbe->llContext, "mergephi", func);
        
        llbe->builder.CreateCondBr(cond1, then, mergephi);
        
        llvm::BasicBlock * origin_exit = llbe->builder.GetInsertBlock();
        
        // Emit then block.
        llbe->builder.SetInsertPoint(then);
        
        llvm::Value * cond2 = (llvm::Value*)getRight()->generate(backEnd);
        cond2 = llbe->builder.CreateICmpEQ(cond2, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)), "rhs_cond");
        
        llbe->builder.CreateCondBr(cond2, mergephi, condfalse);
        
        llvm::BasicBlock * then_exit = llbe->builder.GetInsertBlock();
        
        // Emit condfalse block.
        llbe->builder.SetInsertPoint(condfalse);
        
        llbe->builder.CreateBr(mergephi);
        
        // Emit mergephi block.
        llbe->builder.SetInsertPoint(mergephi);
        
        llvm::PHINode * phi = llbe->builder.CreatePHI(llvm::Type::getInt1Ty(llbe->llContext), 3, "lazyphi");
        phi->addIncoming(llvm::ConstantInt::getFalse(llbe->llContext), origin_exit);
        phi->addIncoming(llvm::ConstantInt::getTrue(llbe->llContext), then_exit);
        phi->addIncoming(llvm::ConstantInt::getFalse(llbe->llContext), condfalse);
        
        return phi;
    }
   
   
    void * LogOrExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Function * func = llbe->builder.GetInsertBlock()->getParent();
        
        llvm::Value * cond1 = (llvm::Value*)getLeft()->generate(backEnd);
        cond1 = llbe->builder.CreateICmpEQ(cond1, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)), "lhs_cond");
        
        // Create blocks for the then and else cases.  Insert the 'then' block at the
        // end of the function.
        llvm::BasicBlock * _else = llvm::BasicBlock::Create(llbe->llContext, "_else", func);
        llvm::BasicBlock * condfalse = llvm::BasicBlock::Create(llbe->llContext, "condfalse", func);
        llvm::BasicBlock * mergephi = llvm::BasicBlock::Create(llbe->llContext, "mergephi", func);
        
        llbe->builder.CreateCondBr(cond1, mergephi, _else);
        
        llvm::BasicBlock * origin_exit = llbe->builder.GetInsertBlock();
        
        // Emit _else block.
        llbe->builder.SetInsertPoint(_else);
        
        llvm::Value * cond2 = (llvm::Value*)getRight()->generate(backEnd);
        cond2 = llbe->builder.CreateICmpEQ(cond2, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)), "rhs_cond");
        
        llbe->builder.CreateCondBr(cond2, mergephi, condfalse);
        
        llvm::BasicBlock * _else_exit = llbe->builder.GetInsertBlock();
        
        // Emit condfalse block.
        llbe->builder.SetInsertPoint(condfalse);
        
        llbe->builder.CreateBr(mergephi);
        
        // Emit mergephi block.
        llbe->builder.SetInsertPoint(mergephi);
        
        llvm::PHINode * phi = llbe->builder.CreatePHI(llvm::Type::getInt1Ty(llbe->llContext), 3, "lazyphi");
        phi->addIncoming(llvm::ConstantInt::getTrue(llbe->llContext), origin_exit);
        phi->addIncoming(llvm::ConstantInt::getTrue(llbe->llContext), _else_exit);
        phi->addIncoming(llvm::ConstantInt::getFalse(llbe->llContext), condfalse);
        
        return phi;
    }
    
    static llvm::Value * generateInterfaceFn(BackEnd& backEnd, CallExpression * call_expr, llvm::Value * obj_ptr) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        llvm::PointerType * ll_byte_ptr_t = llvm::Type::getInt8Ty(llbe->llContext)->getPointerTo();
        
        BJOU_DEBUG_ASSERT(call_expr->resolved_proc);
        Procedure * proc = call_expr->resolved_proc;
        
        Struct * s = proc->getParentStruct();
        
        BJOU_DEBUG_ASSERT(s && s->getFlag(Struct::IS_ABSTRACT));
        
        StructType * s_t = (StructType*)s->getType();
        
        BJOU_DEBUG_ASSERT(s_t->interfaceIndexMap.count(proc));
        unsigned int idx = s_t->interfaceIndexMap[proc];
        llvm::Constant * idx_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), idx);
        
        llvm::Value * typeinfo = (llvm::GlobalVariable*)llbe->getGlobaltypeinfo(s);
        BJOU_DEBUG_ASSERT(typeinfo);
        StructType * typeinfo_t = (StructType*)compilation->frontEnd.typeTable["typeinfo"];
        llvm::StructType * ll_typeinfo_t = (llvm::StructType*)llbe->bJouTypeToLLVMType(typeinfo_t);
        
        
        llvm::Value * typeinfo_cast = llbe->builder.CreateBitCast(obj_ptr, ll_typeinfo_t->getPointerTo()->getPointerTo());
        llvm::Value * typeinfo_addr = llbe->builder.CreateLoad(typeinfo_cast, "typeinfo_load");
        llvm::Value * v_table_cast = llbe->builder.CreateBitCast(typeinfo_addr, ll_byte_ptr_t->getPointerTo());
        
        std::vector<llvm::Value*> gep_indices { idx_val };
        llvm::Value * gep = llbe->builder.CreateInBoundsGEP(v_table_cast, gep_indices);
        
        llvm::Value * callee = llbe->builder.CreateLoad(gep, "v_table_load");
        
        
        BJOU_DEBUG_ASSERT(callee);
        
        llvm::Value * fn = llbe->builder.CreateBitCast(callee, llbe->bJouTypeToLLVMType(proc->getType()));
        
        return fn;
    }
    
    void * CallExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        ArgList * arglist = (ArgList*)getRight();
        std::vector<llvm::Value*> args;
        
        for (ASTNode * arg : arglist->getExpressions())
            args.push_back((llvm::Value*)arg->generate(backEnd));
        
        llvm::Value * callee = nullptr;
        
        if (getFlag(CallExpression::INTERFACE_CALL)) {
            BJOU_DEBUG_ASSERT(arglist->getExpressions().size() > 0);
            Expression * first_arg = (Expression*)arglist->getExpressions()[0];
            Struct * s = resolved_proc->getParentStruct();
            BJOU_DEBUG_ASSERT(s);
            PointerType * ptr_t = (PointerType*)s->getType()->pointerOf();
            BJOU_DEBUG_ASSERT(first_arg->getType()->equivalent(ptr_t, /*exact_match =*/ false));
            delete ptr_t;
            BJOU_DEBUG_ASSERT(args.size() > 0);
            callee = generateInterfaceFn(backEnd, this, args[0]);
            
            llvm::Type * first_arg_t = args[0]->getType();
            llvm::FunctionType * callee_t = (llvm::FunctionType*)callee->getType()->getPointerElementType();
            llvm::Type * first_param_t = callee_t->getParamType(0);
            if (first_arg_t != first_param_t)
                args[0] = llbe->builder.CreateBitCast(args[0], first_param_t);
        } else callee = (llvm::Value*)getLeft()->generate(backEnd);
        
        BJOU_DEBUG_ASSERT(callee);
        
        return llbe->builder.CreateCall(callee, args);
    }
    
    static llvm::Value * gep_by_index(BackEnd& backEnd, llvm::Type * t, llvm::Value * val, int index) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        std::vector<llvm::Value*> indices;
        
        indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), 0));
        indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), index));
        
        return llbe->builder.CreateInBoundsGEP(t, val, indices);
    }
    
    void * SubscriptExpression::generate(BackEnd& backEnd, bool getAddr) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * lt = getLeft()->getType();
        
        llvm::Value * lv = nullptr;
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        std::vector<llvm::Value*> indices;
        
        if (lt->isPointer()) {
            lv = (llvm::Value*)getLeft()->generate(backEnd);
            /*
            const Type * sub_t = ((PointerType*)lt)->pointer_of;
            uint64_t size = llbe->layout->getTypeAllocSize(llbe->bJouTypeToLLVMType(sub_t));
            llvm::Value * size_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), size);
            rv = llbe->builder.CreateMul(size_val, rv);
             */
        } else if (lt->isArray()) {
            lv = (llvm::Value*)getLeft()->generate(backEnd);
            /*
             if (llvm::isa<llvm::Constant>(lv)) {
                BJOU_DEBUG_ASSERT(!getAddr);
                Expression * r = (Expression*)getRight();
                BJOU_DEBUG_ASSERT(r->isConstant());
                return llbe->builder.CreateExtractValue(lv, { static_cast<unsigned int>(r->eval().as_i64) });
            }
            
            */
            // indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), 0));
        } else { BJOU_DEBUG_ASSERT(false); return nullptr; }
        
        indices.push_back(rv);
        
        llvm::Value * gep = llbe->builder.CreateInBoundsGEP(lv, indices);
        if (getAddr)
            return gep;
        return llbe->builder.CreateLoad(gep, "subscript");
    }
    
    void * AccessExpression::generate(BackEnd& backEnd, bool getAddr) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        std::vector<llvm::Value*> indices;
        llvm::Value * left_val = nullptr;
        int elem = -1;
        std::string name;
        
        const Type * t = getLeft()->getType();
        StructType * s_t = nullptr;
        TupleType * t_t = nullptr;
        Identifier * r_id = (Identifier*)getRight();
        IntegerLiteral * r_elem = (IntegerLiteral*)getRight();
        
        if (t->isPointer()) {
            PointerType * pt = (PointerType*)t;
            left_val = (llvm::Value*)getLeft()->generate(backEnd);
            if (pt->pointer_of->isStruct()) {
                s_t = (StructType*)((PointerType*)t)->pointer_of;
                // @refactor type member constants
                // should this be here or somewhere in the frontend?
                
                if (s_t->constantMap.count(r_id->getUnqualified()))
                    return s_t->constantMap[r_id->getUnqualified()]->generate(backEnd);
                
                // regular structure access
                
                name = "structure_access";
                elem = s_t->memberIndices[r_id->getUnqualified()] + 1; // for typeinfo
                if (s_t->_struct->getMangledName() == "typeinfo")
                    elem -= 1;
            } else if (pt->pointer_of->isTuple()) {
                name = "tuple_access";
                t_t = (TupleType*)((PointerType*)t)->pointer_of;
                elem = (int)r_elem->eval().as_i64;
            }
        } else if (t->isStruct()) {
            s_t = (StructType*)t;
            // @refactor type member constants
            // should this be here or somewhere in the frontend?
            
            if (s_t->constantMap.count(r_id->getUnqualified()))
                return s_t->constantMap[r_id->getUnqualified()]->generate(backEnd);
            
            // regular structure access
            
            name = "structure_access";
            left_val = (llvm::Value*)getLeft()->generate(backEnd, true);
            elem = s_t->memberIndices[r_id->getUnqualified()] + 1; // +1 for typeinfo
            if (s_t->_struct->getMangledName() == "typeinfo")
                elem -= 1;
        } else if (t->isTuple()) {
            name = "tuple_access";
            left_val = (llvm::Value*)getLeft()->generate(backEnd, true);
            t_t = (TupleType*)t;
            elem = (int)r_elem->eval().as_i64;
        } else BJOU_DEBUG_ASSERT(false);
        
        indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), 0));
        indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), elem));
        
        llvm::Value * access = llbe->builder.CreateInBoundsGEP(left_val, indices);
        
        BJOU_DEBUG_ASSERT(access->getType()->isPointerTy());
        
        if (access->getType()->getPointerElementType()->isArrayTy())
            access = llbe->createPointerToArrayElementsOnStack(access);
        
        if (getAddr)
            return access;
        return llbe->builder.CreateLoad(access, name);
    }
    
    void * NewExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Function * func = llbe->llModule->getFunction("malloc");
        BJOU_DEBUG_ASSERT(func);
        
        const Type * r_t = getRight()->getType();
        uint64_t size;
        std::vector<llvm::Value*> args;
        llvm::Value * size_val = nullptr;
        
        if (r_t->isArray()) {
            const ArrayType * array_t = (const ArrayType*)r_t;
            const Type * sub_t = array_t->array_of;
            ArrayDeclarator * array_decl = (ArrayDeclarator*)getRight();
            size = llbe->layout->getTypeAllocSize(llbe->bJouTypeToLLVMType(sub_t));
            size_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), size);
            size_val = llbe->builder.CreateMul(size_val, (llvm::Value*)array_decl->getExpression()->generate(backEnd));
        } else {
            size = llbe->layout->getTypeAllocSize(llbe->bJouTypeToLLVMType(r_t));
            size_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), size);
        }
        
        args.push_back(size_val);
        
        llvm::Value * val = llbe->builder.CreateBitCast(llbe->builder.CreateCall(func, args), llbe->bJouTypeToLLVMType(getType()));
        
        if (r_t->isArray()) {
            // @incomplete
            // initialize struct typeinfos in arrays
        } else {
            // store typeinfo
            if (r_t->isStruct()) {
                Struct * s = ((StructType*)r_t)->_struct;
                llbe->builder.CreateStore(llbe->getGlobaltypeinfo(s),
                                          llbe->builder.CreateInBoundsGEP(val,
                                                                          {   llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llbe->llContext)),
                                                                              llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llbe->llContext)) }));
            }
            //
        }
        
        return val;
    }
    
    void * DeleteExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Function * func = llbe->llModule->getFunction("free");
        BJOU_DEBUG_ASSERT(func);
        
        std::vector<llvm::Value*> args = { llbe->builder.CreateBitCast((llvm::Value*)getRight()->generate(backEnd), llvm::Type::getInt8PtrTy(llbe->llContext)) };
        
        return llbe->builder.CreateCall(func, args);
    }
    
    void * SizeofExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        uint64_t size = llbe->layout->getTypeAllocSize(llbe->bJouTypeToLLVMType(getRight()->getType()));
        
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(llbe->llContext), size);
    }
    
    void * NotExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        return llbe->builder.CreateNot((llvm::Value*)getRight()->generate(backEnd));
    }
    
    void * DerefExpression::generate(BackEnd& backEnd, bool getAddr) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Value * rv = (llvm::Value*)getRight()->generate(backEnd);
        
        if (getAddr)
            return rv;
        return llbe->builder.CreateLoad(rv, "deref");
    }
    
    void * AddressExpression::generate(BackEnd& backEnd, bool flag) {
        return getRight()->generate(backEnd, true);
    }
    
#if 0
    const Type * primativeConversionResult(const Type * l, const Type * r) {
    // one of l and r is fp, then we choose the fp type
    if (l->isFP() && !r->isFP())
        return l;
    if (!l->isFP() && r->isFP())
        return r;
    // if both, choose the larger
    if (l->isFP() && r->isFP()) {
        if (l->size >= r->size)
            return l;
        else return r;
    }
    
    // both integer types
    Type::Sign sign = Type::Sign::UNSIGNED;
    if (l->sign == Type::Sign::SIGNED || r->sign == Type::Sign::SIGNED)
        sign = Type::Sign::SIGNED;
        int size = l->size >= r->size ? l->size : r->size;
        
        if (size == -1)
            return compilation->frontEnd.typeTable["void"];
    
    char buf[32];
    sprintf(buf, "%c%d", (sign == Type::Sign::SIGNED ? 'i' : 'u'), size);
    
    BJOU_DEBUG_ASSERT(compilation->frontEnd.typeTable.count(buf) > 0);
    
    return compilation->frontEnd.typeTable[buf];
}
#endif

    void * AsExpression::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * lt = getLeft()->getType();
        const Type * rt = getRight()->getType();
        
        llvm::Value * val = (llvm::Value*)getLeft()->generate(backEnd);
        llvm::Type * ll_rt = llbe->bJouTypeToLLVMType(rt);
        
        if (lt->enumerableEquivalent() && rt->enumerableEquivalent()) {
            return llbe->builder.CreateIntCast(val, ll_rt, rt->sign == Type::SIGNED);
        } else if (lt->enumerableEquivalent() && rt->isFP()) {
            if (lt->sign == Type::SIGNED)
                return llbe->builder.CreateSIToFP(val, ll_rt);
            else return llbe->builder.CreateSIToFP(val, ll_rt);
        } else if (lt->isFP() && rt->enumerableEquivalent()) {
            if (rt->sign == Type::SIGNED)
                return llbe->builder.CreateFPToSI(val, ll_rt);
            else return llbe->builder.CreateFPToUI(val, ll_rt);
        } else if (lt->isFP() && rt->isFP()) {
            if (lt->size < rt->size)
                return llbe->builder.CreateFPExt(val, ll_rt);
            else return llbe->builder.CreateFPTrunc(val, ll_rt);
        }
        
        // @incomplete
        if (lt->isPointer() && rt->isPointer())
            return llbe->builder.CreateBitCast(val, ll_rt);
        return nullptr;
    }
    
    void * BooleanLiteral::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        if (getContents() == "true")
            return llvm::ConstantInt::getTrue(llbe->llContext);
        else if (getContents() == "false")
            return llvm::ConstantInt::getFalse(llbe->llContext);
        return nullptr;
    }
    
    void * IntegerLiteral::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), getContents(), 10); // base 10
    }
    
    void * FloatLiteral::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(llbe->llContext), getContents());
    }
    
    void * CharLiteral::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        std::string& contents = getContents();
        char ch = get_ch_value(contents);
        return llvm::ConstantInt::get(llvm::Type::getInt8Ty(llbe->llContext), ch);
    }
    
    void * StringLiteral::generate(BackEnd& backEnd, bool flag) {
        std::string str = getContents().substr(1, getContents().size() - 2);
        str = str_escape(str);
        return ((LLVMBackEnd*)&backEnd)->createGlobalStringVariable(str);
    }
    
    void * TupleLiteral::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::StructType * ll_t = (llvm::StructType*)llbe->bJouTypeToLLVMType(getType());
        
        llvm::AllocaInst * alloca = llbe->builder.CreateAlloca(ll_t);
        std::vector<ASTNode*>& subExpressions = getSubExpressions();
        for (size_t i = 0; i < subExpressions.size(); i += 1) {
            llvm::Value * elem = llbe->builder.CreateInBoundsGEP(ll_t, alloca, {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), i)
            });
            llbe->builder.CreateStore((llvm::Value*)subExpressions[i]->generate(*llbe), elem);
        }
        
        return llbe->builder.CreateLoad(alloca, "tupleliteral");
    }
    
    void * ProcLiteral::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        (void)llbe; // shut up compiler
        Procedure * proc = (Procedure*)getRight();
        llvm::Function * func = (llvm::Function*)proc->generate(backEnd);
        BJOU_DEBUG_ASSERT(func);
        return func;
    }

    void * ExternLiteral::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        (void)llbe; // shut up compiler
        Procedure * proc = (Procedure*)getRight();
        llvm::Function * func = (llvm::Function*)proc->generate(backEnd);
        BJOU_DEBUG_ASSERT(func);
        return func;
    }
    
    void * Identifier::generate(BackEnd& backEnd, bool getAddr) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Value * ptr = llbe->namedVal(qualified);
        
        // if the named value is not a stack allocation or global variable (i.e. constant, function, etc.)
        // we don't want the load instruction ever
        if (!llvm::isa<llvm::AllocaInst>(ptr) && !llvm::isa<llvm::GlobalVariable>(ptr))
            getAddr = true;
        // we shouldn't load direct function references
        else if (llvm::isa<llvm::Function>(ptr))
            getAddr = false;
        
        if (getAddr) return ptr;
        return llbe->builder.CreateLoad(ptr, unqualified);
    }
    
    llvm::Value * LLVMBackEnd::getPointerToArrayElements(llvm::Value * array) {
        std::vector<llvm::Value*> indices {
            llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llContext)),
            llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llContext))
        };
        return builder.CreateInBoundsGEP(array, indices);
    }
    
    llvm::Value * LLVMBackEnd::createPointerToArrayElementsOnStack(llvm::Value * array) {
        BJOU_DEBUG_ASSERT(array->getType()->isPointerTy() && array->getType()->getPointerElementType()->isArrayTy());
        llvm::Value * ptr = builder.CreateAlloca(array->getType()->getPointerElementType()->getArrayElementType()->getPointerTo());
        builder.CreateStore(getPointerToArrayElements(array), ptr);
        return ptr;
    }
    
    llvm::Value * LLVMBackEnd::copyConstantInitializerToStack(llvm::Constant * constant_init) {
        llvm::AllocaInst * alloca = builder.CreateAlloca(constant_init->getType());
        builder.CreateStore(constant_init, alloca);
        return alloca;
    }
    
    llvm::Constant * LLVMBackEnd::createConstantInitializer(InitializerList * ilist) {
        const Type * t = ilist->getType();
        llvm::Type * ll_t = bJouTypeToLLVMType(t);
        std::vector<ASTNode*>& expressions = ilist->getExpressions();
        
        if (t->isStruct()) {
            StructType * s_t = (StructType*)t;
            std::vector<std::string>& names = ilist->getMemberNames();
            std::vector<llvm::Constant*> vals(s_t->memberTypes.size() + 1, nullptr); // +1 for typeinfo
            
            vals[0] = (llvm::Constant*)getGlobaltypeinfo(s_t->_struct);
            
            for (int i = 0; i < (int)names.size(); i += 1) {
                // we will assume everything works out since it passed analysis
                int index = 1 + s_t->memberIndices[names[i]]; // +1 for typeinfo
                vals[index] = (llvm::Constant*)expressions[i]->generate(*this);
            }
            for (int i = 1; i < (int)vals.size(); i += 1) // starts at 1 for typeinfo
                if (!vals[i])
                    vals[i] = llvm::Constant::getNullValue(bJouTypeToLLVMType(s_t->memberTypes[i - 1]));
            return llvm::ConstantStruct::get((llvm::StructType*)ll_t, vals);
        } else if (t->isArray()) {
            std::vector<llvm::Constant*> vals;
            for (ASTNode * expr : expressions)
                vals.push_back((llvm::Constant*)expr->generate(*this));
            llvm::Constant * c = llvm::ConstantArray::get((llvm::ArrayType*)ll_t, vals);
            return c;
        }
        
        BJOU_DEBUG_ASSERT(false);
        return nullptr;
    }
    
    void * InitializerList::generate(BackEnd& backEnd, bool getAddr) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * myt = getType();
        std::vector<ASTNode*>& expressions = getExpressions();
        llvm::Type * ll_t = (llvm::Type*)llbe->bJouTypeToLLVMType(myt);
        llvm::AllocaInst * alloca = nullptr;
        llvm::Value * ptr = nullptr;
        llvm::Value * ldptr = nullptr;
        
        if (isConstant()) {
            llvm::Constant * constant_init = llbe->createConstantInitializer(this);
            if (myt->isStruct()) {
                if (getAddr)
                    return llbe->copyConstantInitializerToStack(constant_init);
                else return constant_init;
            } else if (myt->isArray()) {
                llvm::Value * onStack = llbe->copyConstantInitializerToStack(constant_init);
                ptr = llbe->createPointerToArrayElementsOnStack(onStack);
                if (getAddr)
                    return ptr;
                return llbe->builder.CreateLoad(ptr, "array_ptr");
            }
            BJOU_DEBUG_ASSERT(false);
        }
        
        if (myt->isStruct()) {
            StructType * s_t = (StructType*)myt;
            std::vector<std::string>& names = getMemberNames();
            
            alloca = llbe->builder.CreateAlloca(ll_t);
            ptr = llbe->builder.CreateInBoundsGEP(alloca, { llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llbe->llContext)) });
            alloca->setName("structinitializer");
            
            BJOU_DEBUG_ASSERT(names.size() == expressions.size());
            
            std::vector<llvm::Value*> vals(s_t->memberTypes.size() + 1, nullptr); // +1 for typeinfo
            
            for (int i = 0; i < (int)names.size(); i += 1) {
                int index = s_t->memberIndices[names[i]] + 1;
                vals[index] = (llvm::Value*)expressions[i]->generate(*llbe);
            }
            
            vals[0] = llbe->getGlobaltypeinfo(s_t->_struct);
            
            for (size_t i = 0; i < vals.size(); i += 1) {
                if (!vals[i]) continue;
                
                llvm::Value * elem = llbe->builder.CreateInBoundsGEP(
                                                                     ptr,
                                                                     { llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llbe->llContext)),
                                                                         llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), i)}
                                                                     );
                llbe->builder.CreateStore(vals[i], elem);
            }
            
            if (getAddr)
                return ptr;
            
            ldptr = llbe->builder.CreateLoad(alloca, "structinitializer_ld");
        } else if (myt->isArray()) {
            alloca = llbe->builder.CreateAlloca(ll_t);
            alloca->setName("arrayinitializer");
            ptr = llbe->createPointerToArrayElementsOnStack(alloca);
            ldptr = llbe->builder.CreateLoad(ptr, "arrayptr");
            
            for (size_t i = 0; i < expressions.size(); i += 1) {
                llvm::Value * idx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(llbe->llContext), i);
                llvm::Value * elem = llbe->builder.CreateInBoundsGEP(ldptr, idx);
                llbe->builder.CreateStore((llvm::Value*)expressions[i]->generate(*llbe), elem);
            }
            
            if (getAddr)
                return ptr;
        }
        
        BJOU_DEBUG_ASSERT(ldptr);
        return ldptr;
    }
    
    void * Constant::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        if (getFlag(IS_TYPE_MEMBER)) {
            auto c = llbe->generatedTypeMemberConstants.find(this);
            if (c != llbe->generatedTypeMemberConstants.end())
                return c->second;
            llvm::Value * v = (llvm::Value*)getInitialization()->generate(backEnd);
            BJOU_DEBUG_ASSERT(v);
            llbe->generatedTypeMemberConstants[this] = v;
            return v;
        }
        return llbe->namedVal(getMangledName(), (llvm::Value*)getInitialization()->generate(backEnd));
    }
    
    static void * GenerateGlobalVariable(VariableDeclaration * var, BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        const Type * t = var->getType();
        llvm::Type * ll_t = llbe->bJouTypeToLLVMType(t);
        size_t allign = llbe->layout->getTypeAllocSize(ll_t);
        llvm::GlobalVariable * gvar = nullptr;
        
        if (t->isArray()) {
            PointerType * ptr_t = (PointerType*)((ArrayType*)t)->array_of->pointerOf();
            llvm::Type * ll_ptr_t = llbe->bJouTypeToLLVMType(ptr_t);
            gvar = new llvm::GlobalVariable(*llbe->llModule,
                                            ll_ptr_t,
                                            false,
                                            llvm::GlobalVariable::ExternalLinkage,
                                            0,
                                            var->getMangledName());
            gvar->setAlignment((unsigned int)llbe->layout->getTypeAllocSize(ll_ptr_t));
            
            llvm::GlobalVariable * under = new llvm::GlobalVariable(*llbe->llModule,
                                                                    ll_t,
                                                                    false,
                                                                    llvm::GlobalVariable::ExternalLinkage,
                                                                    0,
                                                                    "__bjou_array_under_" + var->getMangledName());
            under->setAlignment(llbe->layout->getPreferredAlignment(under));
            
            if (var->getInitialization()) {
                if (var->getInitialization()->nodeKind == ASTNode::INITIALZER_LIST)
                    under->setInitializer(llbe->createConstantInitializer((InitializerList*)var->getInitialization()));
            } else {
                under->setInitializer(llvm::Constant::getNullValue(ll_t));
            }
            
            gvar->setInitializer((llvm::Constant*)llbe->builder.CreateInBoundsGEP(under,
                                                                                  { llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llbe->llContext)),
                                                                                      llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llbe->llContext)) }));
        } else {
            gvar = new llvm::GlobalVariable(/*Module=*/*llbe->llModule,
                                                                   /*Type=*/ll_t,
                                                                   /*isConstant=*/false,
                                                                   /*Linkage=*/llvm::GlobalValue::ExternalLinkage,
                                                                   /*Initializer=*/0, // has initializer, specified below
                                                                   /*Name=*/var->getMangledName());
            gvar->setAlignment((unsigned int)allign);
            if (var->getInitialization())
                gvar->setInitializer((llvm::Constant*)var->getInitialization()->generate(backEnd));
            else gvar->setInitializer(llvm::Constant::getNullValue(ll_t));
        }
        
        llbe->namedVal(var->getMangledName(), gvar);
        return gvar;
    }

    void * VariableDeclaration::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Type * t = llbe->bJouTypeToLLVMType(getType());
        llvm::Value * val = llbe->namedVal(getMangledName(), t);
        BJOU_DEBUG_ASSERT(val);
        
        if (getType()->isArray()) {
            // @incomplete
            // initialize struct typeinfos for arrays
        } else {
            // store typeinfo
            if (getType()->isStruct()) {
                Struct * s = ((StructType*)getType())->_struct;
                llbe->builder.CreateStore(llbe->getGlobaltypeinfo(s),
                                          llbe->builder.CreateInBoundsGEP(val,
                                                                          {   llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llbe->llContext)),
                                                                              llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(llbe->llContext)) }));
            }
            //
        }
        
        if (getInitialization()) {
            llvm::Value * init_v = (llvm::Value*)getInitialization()->generate(backEnd);
            llbe->builder.CreateStore(init_v, val);
        }
        
        return val;
    }
    
    static void gen_printf_fmt(BackEnd& backEnd, llvm::Value * val, const Type * t, std::string& fmt, std::vector<llvm::Value*>& vals) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        if (t->isStruct()) {
            // val should be a ptr!!
            
            StructType * s_t = (StructType*)t;
            
            fmt += "{ ";
            for (int i = 0; i < (int)s_t->memberTypes.size(); i += 1) {
                fmt += ".";
                for (auto& mem : s_t->memberIndices) {
                    if (mem.second == i) {
                        fmt += mem.first;
                        break;
                    }
                }
                fmt += " = ";
                const Type * mem_t = s_t->memberTypes[i];
                llvm::Value * mem_val = gep_by_index(backEnd, llbe->bJouTypeToLLVMType(s_t), val, i + 1); // +1 for typeinfo
                if (!mem_t->isStruct())
                    mem_val = llbe->builder.CreateLoad(mem_val);
                gen_printf_fmt(backEnd, mem_val, s_t->memberTypes[i], fmt, vals);
                if (i < (int)s_t->memberTypes.size() - 1)
                    fmt += ", ";
            }
            fmt += " }";
        } else {
            if (t->isPrimative()) {
                if (t->isFP()) {
                    if (t->size == 32)
                        fmt += "%f";
                    else fmt += "%g";
                } else {
                    if (t->code == "char")
                        fmt += "%c";
                    else {
                        if (t->size <= 32) {
                            if (t->sign == Type::UNSIGNED)
                                fmt += "%u";
                            else fmt += "%d";
                        } else {
                            if (t->sign == Type::UNSIGNED)
                                fmt += "%llu";
                            else fmt += "%lld";
                        }
                    }
                }
            } else if (t->isPointer()) {
                if (((PointerType*)t)->pointer_of->code == "char")
                    fmt += "%s";
                else fmt += "%p";
            } else if (t->isProcedure()) {
                fmt += "%p";
            } else internalError("Can't print type."); // @temporary
            
            vals.push_back(val);
        }
    }
    
    void * Print::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        std::string fmt;
        
        ArgList * args = (ArgList*)getArgs();
        std::string pre_fmt = ((Expression*)args->getExpressions()[0])->contents;
        
        pre_fmt = pre_fmt.substr(1, pre_fmt.size() - 2); // get rid of quotes
        pre_fmt = str_escape(pre_fmt);
        
        std::vector<llvm::Value*> vals = { nullptr };
        
        int arg = 1;
        for (int c = 0; c < (int)pre_fmt.size(); c++) {
            if (pre_fmt[c] == '%') {
                Expression * expr = (Expression*)args->expressions[arg];
                const Type * t = expr->getType();
                llvm::Value * val = t->isStruct() ? (llvm::Value*)expr->generate(backEnd, true) : (llvm::Value*)expr->generate(backEnd);
                
                gen_printf_fmt(backEnd, val, t, fmt, vals);
                
                arg++;
            } else fmt += pre_fmt[c];
        }
        
        fmt += "\n";
        
        vals[0] = llbe->createGlobalStringVariable(fmt);
        
        llvm::Function * pf = llbe->llModule->getFunction("printf");
        BJOU_DEBUG_ASSERT(pf && "Did not find printf()!");
        llvm::Value * val = llbe->builder.CreateCall(pf, vals);
        
        return val;
    }
    
    void * If::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        bool hasElse = (bool)getElse();
        
        llvm::Value * cond = (llvm::Value*)getConditional()->generate(backEnd);
        // Convert condition to a bool by comparing equal to 1.
        cond = llbe->builder.CreateICmpEQ(cond, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)), "ifcond");
        
        
        llvm::Function * func = llbe->builder.GetInsertBlock()->getParent();
        
        // Create blocks for the then and else cases.  Insert the 'then' block at the
        // end of the function.
        llvm::BasicBlock * then = llvm::BasicBlock::Create(llbe->llContext, "then", func);
        llvm::BasicBlock * _else = nullptr;
        if (hasElse) _else = llvm::BasicBlock::Create(llbe->llContext, "else");
        llvm::BasicBlock * merge = llvm::BasicBlock::Create(llbe->llContext, "merge");
        
        if (hasElse)
            llbe->builder.CreateCondBr(cond, then, _else);
        else llbe->builder.CreateCondBr(cond, then, merge);
        
        // Emit then value.
        llbe->builder.SetInsertPoint(then);
        
        for (ASTNode * statement : getStatements())
            statement->generate(backEnd);
        
        if (!getFlag(HAS_TOP_LEVEL_RETURN))
            llbe->builder.CreateBr(merge);
        
        if (hasElse) {
            // Emit else block.
            func->getBasicBlockList().push_back(_else);
            llbe->builder.SetInsertPoint(_else);
            
            Else * _Else = (Else*)getElse();
            for (ASTNode * statement : _Else->getStatements())
                statement->generate(backEnd);
            
            if (!_Else->getFlag(HAS_TOP_LEVEL_RETURN))
                llbe->builder.CreateBr(merge);
            
            _else = llbe->builder.GetInsertBlock();
        }
        
        // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
        then = llbe->builder.GetInsertBlock();
        
        // Emit merge block.
        func->getBasicBlockList().push_back(merge);
        llbe->builder.SetInsertPoint(merge);
        
        
        return merge; // what does this accomplish?
    }
    
    void * For::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        for (ASTNode * init : getInitializations())
            init->generate(backEnd);
        
        llvm::Function * func = llbe->builder.GetInsertBlock()->getParent();
        
        llvm::BasicBlock * check = llvm::BasicBlock::Create(llbe->llContext, "forcheckcond", func);
        
        llbe->builder.CreateBr(check);
        
        llbe->builder.SetInsertPoint(check);
        
        llvm::Value * cond = (llvm::Value*)getConditional()->generate(backEnd);
        // Convert condition to a bool by comparing equal to 1.
        cond = llbe->builder.CreateICmpEQ(cond, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)), "forcond");
        
        
        llvm::BasicBlock * then = llvm::BasicBlock::Create(llbe->llContext, "then", func);
        llvm::BasicBlock * merge = llvm::BasicBlock::Create(llbe->llContext, "merge");
        
        llbe->builder.CreateCondBr(cond, then, merge);
        
        // Emit then value.
        llbe->builder.SetInsertPoint(then);
        
        for (ASTNode * statement : getStatements())
            statement->generate(backEnd);
        
        for (ASTNode * at : getAfterthoughts())
            at->generate(backEnd);
        
        if (!getFlag(HAS_TOP_LEVEL_RETURN))
            llbe->builder.CreateBr(check);
        
        // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
        then = llbe->builder.GetInsertBlock();
        
        // Emit merge block.
        func->getBasicBlockList().push_back(merge);
        llbe->builder.SetInsertPoint(merge);
        
        
        return merge; // what does this accomplish?
    }
    
    void * While::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Function * func = llbe->builder.GetInsertBlock()->getParent();
        
        llvm::BasicBlock * check = llvm::BasicBlock::Create(llbe->llContext, "whilecheckcond", func);
        
        llbe->builder.CreateBr(check);
        
        llbe->builder.SetInsertPoint(check);
        
        llvm::Value * cond = (llvm::Value*)getConditional()->generate(backEnd);
        // Convert condition to a bool by comparing equal to 1.
        cond = llbe->builder.CreateICmpEQ(cond, llvm::ConstantInt::get(llbe->llContext, llvm::APInt(1, 1, true)), "whilecond");
        
        
        llvm::BasicBlock * then = llvm::BasicBlock::Create(llbe->llContext, "then", func);
        llvm::BasicBlock * merge = llvm::BasicBlock::Create(llbe->llContext, "merge");
        
        llbe->builder.CreateCondBr(cond, then, merge);
        
        // Emit then value.
        llbe->builder.SetInsertPoint(then);
        
        for (ASTNode * statement : getStatements())
            statement->generate(backEnd);
        
        if (!getFlag(HAS_TOP_LEVEL_RETURN))
            llbe->builder.CreateBr(check);
        
        // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
        then = llbe->builder.GetInsertBlock();
        
        // Emit merge block.
        func->getBasicBlockList().push_back(merge);
        llbe->builder.SetInsertPoint(merge);
        
        
        return merge; // what does this accomplish?
    }
    
    void * Procedure::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        
        llvm::Function * func = llbe->llModule->getFunction(getMangledName());
        
        if (!func) {
            // generate proto and queue up the proc to be defined later
            
            const ProcedureType * my_real_t = (const ProcedureType*)getType();
            
            // quick swap of array args to equivalent ptr args
            ProcedureType * my_t = new ProcedureType(*my_real_t);
            for (const Type * &argt : my_t->paramTypes)
                if (argt->isArray())
                    argt = ((ArrayType*)argt)->array_of->pointerOf();
            
            llvm::FunctionType * func_type = (llvm::FunctionType*)llbe->bJouTypeToLLVMType(my_t)->getPointerElementType();
            
            delete my_t;
            
            llvm::Function::LinkageTypes linkage = getFlag(Procedure::IS_EXTERN)// || getFlag(Procedure::IS_ANONYMOUS)
                ? llvm::Function::ExternalLinkage
                : llvm::Function::InternalLinkage;
            
            func = llvm::Function::Create(func_type, linkage, getMangledName(), llbe->llModule);
            
            BJOU_DEBUG_ASSERT(func);
            
            if (linkage == llvm::Function::LinkageTypes::InternalLinkage)
                llbe->procsGenerateDefinitions.push_back(this);
            
            llbe->namedVal(getMangledName(), func);
        } else {
            // generate the definition
            
            int i = 0;
            for (auto& arg : func->args()) {
                arg.setName(((VariableDeclaration*)(getParamVarDeclarations()[i]))->getName());
                i += 1;
            }
            
            BJOU_DEBUG_ASSERT(func->getBasicBlockList().empty());
            
            BasicBlock * bblock = BasicBlock::Create(llbe->llContext, getMangledName() + "_entry", func);
            llbe->builder.SetInsertPoint(bblock);
            
            llbe->namedValsPushFrame();
            
            for (auto &Arg : func->args()) {
                // Create an alloca for this variable.
                llvm::Value * alloca = llbe->namedVal(Arg.getName(), Arg.getType());
                // Store the initial value into the alloca.
                llbe->builder.CreateStore(&Arg, alloca);
            }
            
            for (ASTNode * statement : getStatements())
                statement->generate(backEnd);
           
            if (!getFlag(HAS_TOP_LEVEL_RETURN)) {
                // dangerous to assume void ret type..  @bad
                // but we hope that semantic analysis will have caught
                // a missing explicit typed return
                llbe->builder.CreateRetVoid();
            }
            
            std::string errstr;
            llvm::raw_string_ostream errstream(errstr);
            if (llvm::verifyFunction(*func, &errstream)) {
                func->dump();
                error(COMPILER_SRC_CONTEXT(), "LLVM:", false, errstream.str());
                internalError("There was an llvm error.");
            }
            
            llbe->namedValsPopFrame();
        }
        
        return func;
    }
    
    void * Return::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        if (getExpression())
            return llbe->builder.CreateRet((llvm::Value*)getExpression()->generate(backEnd));
        return llbe->builder.CreateRetVoid();
    }
    
    void * Namespace::generate(BackEnd& backEnd, bool flag) {
        for (ASTNode * node : getNodes())
            node->generate(backEnd);
        return nullptr;
    }
    
    void * Struct::generate(BackEnd& backEnd, bool flag) {
        LLVMBackEnd * llbe = (LLVMBackEnd*)&backEnd;
        std::string& mname = getMangledName();
        llvm::StructType * ll_s_ty = nullptr;
        StructType * bjou_s_ty = nullptr;
        
        if (llbe->definedTypes.count(mname) == 0) {
            ll_s_ty = llvm::StructType::create(llbe->llContext, mname);
            llbe->definedTypes[mname] = ll_s_ty;
            
            if (mname == "typeinfo")
                llbe->typeinfo_struct = this;
            else llbe->structsGenerateDefinitions.push_back(this);
        } else {
            ll_s_ty = (llvm::StructType*)llbe->definedTypes[mname];
            std::vector<llvm::Type *> member_types;
            if (mname != "typeinfo")
                member_types.push_back(llbe->definedTypes["typeinfo"]->getPointerTo());
            bjou_s_ty = (StructType*)getType();
            for (const Type * bjou_mem_t : bjou_s_ty->memberTypes) {
                llvm::Type * ll_mem_t = llbe->bJouTypeToLLVMType(bjou_mem_t);
                BJOU_DEBUG_ASSERT(ll_mem_t);
                member_types.push_back(ll_mem_t);
            }
            ll_s_ty->setBody(member_types);
        }
        return ll_s_ty;
    }
}
