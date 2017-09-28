TARGET		= bjou
CC 			= g++
INCLUDE		= include
TCLAP_INCLUDE = . 
R_CFLAGS 	= -I$(INCLUDE) -I$(TCLAP_INCLUDE) -DBJOU_USE_COLOR -fPIC -fvisibility-inlines-hidden -Wall -W -Wno-unused-parameter -Wwrite-strings -Wcast-qual -Wmissing-field-initializers -pedantic -Wno-long-long -Wcovered-switch-default -Wnon-virtual-dtor -Wdelete-non-virtual-dtor -Wstring-conversion -Werror=date-time -std=c++11 -O3 -DNDEBUG -flto -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
D_CFLAGS 	= -I$(INCLUDE) -I$(TCLAP_INCLUDE) -DBJOU_USE_COLOR -DBJOU_DEBUG_BUILD -g -fPIC -fvisibility-inlines-hidden -std=c++11 -DNDEBUG -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
LLVM_LIBS	= -lLLVMLTO -lLLVMPasses -lLLVMObjCARCOpts -lLLVMMIRParser -lLLVMSymbolize -lLLVMDebugInfoPDB -lLLVMDebugInfoDWARF -lLLVMCoverage -lLLVMTableGen -lLLVMDlltoolDriver -lLLVMOrcJIT -lLLVMXCoreDisassembler -lLLVMXCoreCodeGen -lLLVMXCoreDesc -lLLVMXCoreInfo -lLLVMXCoreAsmPrinter -lLLVMSystemZDisassembler -lLLVMSystemZCodeGen -lLLVMSystemZAsmParser -lLLVMSystemZDesc -lLLVMSystemZInfo -lLLVMSystemZAsmPrinter -lLLVMSparcDisassembler -lLLVMSparcCodeGen -lLLVMSparcAsmParser -lLLVMSparcDesc -lLLVMSparcInfo -lLLVMSparcAsmPrinter -lLLVMRISCVCodeGen -lLLVMRISCVAsmParser -lLLVMRISCVDesc -lLLVMRISCVInfo -lLLVMPowerPCDisassembler -lLLVMPowerPCCodeGen -lLLVMPowerPCAsmParser -lLLVMPowerPCDesc -lLLVMPowerPCInfo -lLLVMPowerPCAsmPrinter -lLLVMNVPTXCodeGen -lLLVMNVPTXDesc -lLLVMNVPTXInfo -lLLVMNVPTXAsmPrinter -lLLVMMSP430CodeGen -lLLVMMSP430Desc -lLLVMMSP430Info -lLLVMMSP430AsmPrinter -lLLVMMipsDisassembler -lLLVMMipsCodeGen -lLLVMMipsAsmParser -lLLVMMipsDesc -lLLVMMipsInfo -lLLVMMipsAsmPrinter -lLLVMLanaiDisassembler -lLLVMLanaiCodeGen -lLLVMLanaiAsmParser -lLLVMLanaiDesc -lLLVMLanaiAsmPrinter -lLLVMLanaiInfo -lLLVMHexagonDisassembler -lLLVMHexagonCodeGen -lLLVMHexagonAsmParser -lLLVMHexagonDesc -lLLVMHexagonInfo -lLLVMBPFDisassembler -lLLVMBPFCodeGen -lLLVMBPFDesc -lLLVMBPFInfo -lLLVMBPFAsmPrinter -lLLVMARMDisassembler -lLLVMARMCodeGen -lLLVMARMAsmParser -lLLVMARMDesc -lLLVMARMInfo -lLLVMARMAsmPrinter -lLLVMARMUtils -lLLVMAMDGPUDisassembler -lLLVMAMDGPUCodeGen -lLLVMAMDGPUAsmParser -lLLVMAMDGPUDesc -lLLVMAMDGPUInfo -lLLVMAMDGPUAsmPrinter -lLLVMAMDGPUUtils -lLLVMAArch64Disassembler -lLLVMAArch64CodeGen -lLLVMAArch64AsmParser -lLLVMAArch64Desc -lLLVMAArch64Info -lLLVMAArch64AsmPrinter -lLLVMAArch64Utils -lLLVMObjectYAML -lLLVMLibDriver -lLLVMOption -lLLVMWindowsManifest -lLLVMX86Disassembler -lLLVMX86AsmParser -lLLVMX86CodeGen -lLLVMGlobalISel -lLLVMSelectionDAG -lLLVMAsmPrinter -lLLVMDebugInfoCodeView -lLLVMDebugInfoMSF -lLLVMX86Desc -lLLVMMCDisassembler -lLLVMX86Info -lLLVMX86AsmPrinter -lLLVMX86Utils -lLLVMMCJIT -lLLVMLineEditor -lLLVMInterpreter -lLLVMExecutionEngine -lLLVMRuntimeDyld -lLLVMCodeGen -lLLVMTarget -lLLVMCoroutines -lLLVMipo -lLLVMInstrumentation -lLLVMVectorize -lLLVMScalarOpts -lLLVMLinker -lLLVMIRReader -lLLVMAsmParser -lLLVMInstCombine -lLLVMTransformUtils -lLLVMBitWriter -lLLVMAnalysis -lLLVMProfileData -lLLVMObject -lLLVMMCParser -lLLVMMC -lLLVMBitReader -lLLVMCore -lLLVMBinaryFormat -lLLVMSupport -lLLVMDemangle
R_LFLAGS	= -Wl,-search_paths_first -Wl,-headerpad_max_install_names $(LLVM_LIBS) -lcurses -lz -lm -llldYAML -llldReaderWriter -llldMachO -llldELF -llldDriver -llldCore -llldConfig -llldCOFF 
D_LFLAGS	= -Wl,-search_paths_first -Wl,-headerpad_max_install_names $(LLVM_LIBS) -lcurses -lz -lm -llldYAML -llldReaderWriter -llldMachO -llldELF -llldDriver -llldCore -llldConfig -llldCOFF
SRC			= $(wildcard src/*.cpp)
TCLAP_SRC	= $(wildcard tclap/*.cpp)
ALL_SRC		= $(SRC)
ALL_SRC 	+= $(TCLAP_SRC)
R_OBJ 		= $(patsubst %.cpp,release/obj/%.o,$(ALL_SRC))
D_OBJ 		= $(patsubst %.cpp,debug/obj/%.o,$(ALL_SRC))

CORES ?= $(shell sysctl -n hw.ncpu || echo 1)

all:; @$(MAKE) _all -j$(CORES)
_all: debug release
.PHONY: all _all

debug:; @$(MAKE) _debug -j$(CORES)
_debug: $(D_OBJ)
	@mkdir -p bin
	@echo "----------------------------"
	@echo "Building Debug Target $(TARGET)"
	@echo
	@$(CC) $(D_LFLAGS) -o bin/$(TARGET) $?

.PHONY: debug _debug

release:; @$(MAKE) _release -j$(CORES)
_release: $(R_OBJ)
	@mkdir -p bin
	@echo "------------------------------"
	@echo "Building Release Target $(TARGET)"
	@echo
	@$(CC) $(R_LFLAGS) -o bin/$(TARGET) $?

.PHONY: release _release

debug/obj/%.o: %.cpp
	@mkdir -p debug/obj/src
	@mkdir -p debug/obj/tclap
	@echo "Compiling $<"
	@$(CC) $(D_CFLAGS) -c -o $@ $<	

release/obj/%.o: %.cpp
	@mkdir -p release/obj/src
	@mkdir -p release/obj/tclap
	@echo "Compiling $<"
	@$(CC) $(R_CFLAGS) -c -o $@ $<

clean:
	@\rm debug/obj/*.o
	@\rm debug/obj/tclap/*.o
	@\rm release/obj/*.o
	@\rm release/obj/tclap/*.o
	@\rm $(TARGET)
