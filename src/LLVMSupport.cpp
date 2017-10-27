#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/None.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SetVector.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/Comdat.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalAlias.h>
#include <llvm/IR/GlobalIFunc.h>
#include <llvm/IR/GlobalIndirectSymbol.h>
#include <llvm/IR/GlobalObject.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/ModuleSlotTracker.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/Statepoint.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/TypeFinder.h>
#include <llvm/IR/Use.h>
#include <llvm/IR/UseListOrder.h>
#include <llvm/IR/User.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/AtomicOrdering.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Compiler.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#if !defined(LLVM_DUMP_ENABLED)

namespace llvm {
// Value::dump - allow easy printing of Values from the debugger.
LLVM_DUMP_METHOD
void Value::dump() const {
    print(dbgs(), /*IsForDebug=*/true);
    dbgs() << '\n';
}

// Type::dump - allow easy printing of Types from the debugger.
LLVM_DUMP_METHOD
void Type::dump() const {
    print(dbgs(), /*IsForDebug=*/true);
    dbgs() << '\n';
}

//  Module::dump() - Allow printing of Modules from the debugger.
LLVM_DUMP_METHOD
void Module::dump() const {
    print(dbgs(), nullptr,
          /*ShouldPreserveUseListOrder=*/false, /*IsForDebug=*/true);
}
//\brief Allow printing of Comdats from the debugger.
LLVM_DUMP_METHOD
void Comdat::dump() const { print(dbgs(), /*IsForDebug=*/true); }
// NamedMDNode::dump() - Allow printing of NamedMDNodes from the debugger.
LLVM_DUMP_METHOD
void NamedMDNode::dump() const { print(dbgs(), /*IsForDebug=*/true); }
LLVM_DUMP_METHOD
void Metadata::dump() const { dump(nullptr); }
LLVM_DUMP_METHOD
void Metadata::dump(const Module * M) const {
    print(dbgs(), M, /*IsForDebug=*/true);
    dbgs() << '\n';
}
} // namespace llvm
#endif
