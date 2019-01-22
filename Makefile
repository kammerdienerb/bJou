#######################################################################
# bJou Makefile
# #####################################################################
#
#
# Progress indicator stuff
# ########################

BLACK   =\033[30m
RED     =\033[31m
GREEN   =\033[32m
YELLOW  =\033[33m
BLUE    =\033[34m
MAGENTA =\033[35m
CYAN    =\033[36m
WHITE   =\033[37m
RESET   =\033[0m

ifneq ($(words $(MAKECMDGOALS)),1) # if no argument was given to make...
.DEFAULT_GOAL = all # set the default goal to all
%:                   # define a last resort default rule
	@$(MAKE) $@ --no-print-directory -rRf $(firstword $(MAKEFILE_LIST)) # recursive make call, 
else
ifndef PROGRESS
T := $(shell $(MAKE) $(MAKECMDGOALS) --no-print-directory \
      -nrRf $(firstword $(MAKEFILE_LIST)) \
      PROGRESS="COUNTTHIS" | grep -c "COUNTTHIS")
N := x
C = $(words $N)$(eval N := x $N)
PROGRESS = @printf "$(GREEN)[%3s%%]$(RESET) $(CYAN)%s$(RESET)\n" `expr $C '*' 100 / $T`
endif

#######################################################################
# main makefile
###############

TARGET		= bjou
CC          = clang
CXX 		= clang++
INCLUDE		= include
TCLAP_INCLUDE = . 
# LLVM_CFG	= ~/Documents/Programming/llvm-4.0.1/bin/llvm-config
LLVM_CFG	= llvm-config
R_CFLAGS 	= -I$(INCLUDE) -I$(TCLAP_INCLUDE) -DBJOU_USE_COLOR $(shell $(LLVM_CFG) --cxxflags) -fno-rtti -fno-exceptions -O3
D_CFLAGS 	= -I$(INCLUDE) -I$(TCLAP_INCLUDE) -DBJOU_USE_COLOR -DBJOU_DEBUG_BUILD $(shell $(LLVM_CFG) --cxxflags) -fno-rtti -w -g -O0
LLVM_LIBS	= $(shell $(LLVM_CFG) --libs all --system-libs)
TCMLC_PATH  = /usr/local/lib/libtcmalloc.a
JEMLC_PATH  = /usr/local/lib/libjemalloc.a
R_LFLAGS	= $(shell $(LLVM_CFG) --ldflags) $(LLVM_LIBS) -lffi -lpthread -Llib nolibc_syscall/nolibc_syscall.o
ifneq ("$(wildcard $(TCMLC_PATH))","")
	R_LFLAGS += -ltcmalloc
else ifneq ("$(wildcard $(JEMLC_PATH))", "")
	R_LFLAGS += -ljemalloc
endif
D_LFLAGS	= $(R_LFLAGS)
CXX_SRC		= $(wildcard src/*.cpp)
CXX_SRC     += $(wildcard tclap/*.cpp)
C_SRC		= $(wildcard src/*.c)
R_CXX_OBJ 	= $(patsubst %.cpp,obj/release/%.o,$(CXX_SRC))
D_CXX_OBJ 	= $(patsubst %.cpp,obj/debug/%.o,$(CXX_SRC))
R_C_OBJ 	= $(patsubst %.c,obj/release/%.o,$(C_SRC))
D_C_OBJ 	= $(patsubst %.c,obj/debug/%.o,$(C_SRC))

GETCONF    := $(shell command -v getconf 2> /dev/null)
ifdef GETCONF
CORES = $(shell getconf _NPROCESSORS_ONLN 2> /dev/null ||\
				getconf  NPROCESSORS_ONLN 2> /dev/null ||\
				echo 1)
else
CORES = 1
endif

all:; @$(MAKE) _all -j$(CORES)
_all: debug release
.PHONY: all _all

debug:; @$(MAKE) _debug -j$(CORES)
_debug: $(D_CXX_OBJ) $(D_C_OBJ) nolibc_syscall libclangextras
	@mkdir -p bin
ifneq ("$(wildcard $(TCMLC_PATH))","")
	$(PROGRESS) "Building Debug Target $(TARGET) (with tcmalloc)"
else ifneq ("$(wildcard $(JEMLC_PATH))", "")
	$(PROGRESS) "Building Debug Target $(TARGET) (with jemalloc)"
else
	$(PROGRESS) "Building Debug Target $(TARGET)"
endif
	@$(CXX) -o bin/$(TARGET) $(D_CXX_OBJ) $(D_C_OBJ) $(D_LFLAGS)
	$(PROGRESS) "Creating .clang_complete"
	@echo "$(D_CFLAGS)" | tr " " "\n" > ".clang_complete"

.PHONY: debug _debug

release:; @$(MAKE) _release -j$(CORES)
_release: $(R_CXX_OBJ) $(R_C_OBJ) nolibc_syscall libclangextras
	@mkdir -p bin
	$(PROGRESS) "Building Release Target $(TARGET)"
	$(PROGRESS) ""
	@$(CXX) -o bin/$(TARGET) $(R_CXX_OBJ) $(R_C_OBJ) $(R_LFLAGS) 

.PHONY: release _release

obj/debug/%.o: %.cpp
	@mkdir -p obj/debug/src
	@mkdir -p obj/debug/tclap
	$(PROGRESS) "Compiling $<"
	@$(CXX) $(D_CFLAGS) -c -o $@ $<	

obj/release/%.o: %.cpp
	@mkdir -p obj/release/src
	@mkdir -p obj/release/tclap
	$(PROGRESS) "Compiling $<"
	@$(CXX) $(R_CFLAGS) -c -o $@ $<

obj/debug/%.o: %.c
	@mkdir -p obj/debug/src
	$(PROGRESS) "Compiling $<"
	@$(CC) -g -O0 -c -o $@ $<	

obj/release/%.o: %.c
	@mkdir -p obj/release/src
	$(PROGRESS) "Compiling $<"
	@$(CC) -O3 -c -o $@ $<

nolibc_syscall:
	$(PROGRESS) "Building nolibc_syscall"
	@$(MAKE) -C nolibc_syscall

.PHONY: nolibc_syscall

libclangextras:
	$(PROGRESS) "Building libclangextras"
	@mkdir -p lib
	@$(CXX) $(shell $(LLVM_CFG) --ldflags) -L$(shell $(LLVM_CFG) --prefix)/lib -lclangAST -lclangLex -lclangBasic -lLLVMSupport -lLLVMDemangle -lLLVMCore -lLLVMBinaryFormat -lcurses -shared -fPIC -O3 libclangextras.cpp -o lib/libclangextras.so

install: bin/$(TARGET)
	@mkdir -p /usr/local/lib/bjou
	@mkdir -p /usr/local/lib/bjou/modules
	@cp bin/$(TARGET) /usr/local/bin/$(TARGET)
	@cp -r modules/* /usr/local/lib/bjou/modules
	@cp bjou.h /usr/local/include
	@cp nolibc_syscall/nolibc_syscall.h /usr/local/include
	@cp nolibc_syscall/nolibc_syscall.o /usr/local/lib
	@cp nolibc_syscall/libnolibc_syscall.a /usr/local/lib
	@cp lib/libclangextras.so /usr/local/lib

clean:
	@$(MAKE) clean -C nolibc_syscall
	@\rm -f obj/debug/src/*.o
	@\rm -f obj/debug/tclap/*.o
	@\rm -f obj/release/src/*.o
	@\rm -f obj/release/tclap/*.o
	@\rm -f bin/$(TARGET)
	@\rm -f lib/libclangextras.so
#######################################################################
endif # endif for progress bar
