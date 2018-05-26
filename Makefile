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
CC 			= clang++
INCLUDE		= include
TCLAP_INCLUDE = . 
# LLVM_CFG	= ~/Desktop/llvm/build/bin/llvm-config
LLVM_CFG	= llvm-config
R_CFLAGS 	= -I$(INCLUDE) -I$(TCLAP_INCLUDE) -DBJOU_USE_COLOR $(shell $(LLVM_CFG) --cxxflags) -fno-rtti -fno-exceptions -O3
D_CFLAGS 	= -I$(INCLUDE) -I$(TCLAP_INCLUDE) -DBJOU_USE_COLOR -DBJOU_DEBUG_BUILD $(shell $(LLVM_CFG) --cxxflags) -fno-rtti -w -g -O0
LLVM_LIBS	= $(shell $(LLVM_CFG) --libs all --system-libs)
TCMLC_PATH  = /usr/local/lib/libtcmalloc.a
JEMLC_PATH  = /usr/local/lib/libjemalloc.a
R_LFLAGS	= $(shell $(LLVM_CFG) --ldflags) $(LLVM_LIBS) -lffi
ifneq ("$(wildcard $(TCMLC_PATH))","")
	R_LFLAGS += -ltcmalloc
else ifneq ("$(wildcard $(JEMLC_PATH))", "")
	R_LFLAGS += -ljemalloc
endif
D_LFLAGS	= $(R_LFLAGS)
SRC			= $(wildcard src/*.cpp)
TCLAP_SRC	= $(wildcard tclap/*.cpp)
ALL_SRC		= $(SRC)
ALL_SRC 	+= $(TCLAP_SRC)
R_OBJ 		= $(patsubst %.cpp,obj/release/%.o,$(ALL_SRC))
D_OBJ 		= $(patsubst %.cpp,obj/debug/%.o,$(ALL_SRC))

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
_debug: $(D_OBJ)
	@mkdir -p bin
ifneq ("$(wildcard $(TCMLC_PATH))","")
	$(PROGRESS) "Building Debug Target $(TARGET) (with tcmalloc)"
else ifneq ("$(wildcard $(JEMLC_PATH))", "")
	$(PROGRESS) "Building Debug Target $(TARGET) (with jemalloc)"
else
	$(PROGRESS) "Building Debug Target $(TARGET)"
endif
	$(PROGRESS) ""
	@$(CC) -o bin/$(TARGET) $(D_LFLAGS) $? 

.PHONY: debug _debug

release:; @$(MAKE) _release -j$(CORES)
_release: $(R_OBJ)
	@mkdir -p bin
	$(PROGRESS) "Building Release Target $(TARGET)"
	$(PROGRESS) ""
	@$(CC) -o bin/$(TARGET) $? $(R_LFLAGS) 

.PHONY: release _release

obj/debug/%.o: %.cpp
	@mkdir -p obj/debug/src
	@mkdir -p obj/debug/tclap
	$(PROGRESS) "Compiling $<"
	@$(CC) $(D_CFLAGS) -c -o $@ $<	

obj/release/%.o: %.cpp
	@mkdir -p obj/release/src
	@mkdir -p obj/release/tclap
	$(PROGRESS) "Compiling $<"
	@$(CC) $(R_CFLAGS) -c -o $@ $<

install: bin/$(TARGET)
	@mkdir -p /usr/local/lib/bjou
	@mkdir -p /usr/local/lib/bjou/modules
	@cp bin/$(TARGET) /usr/local/bin/$(TARGET)
	@rsync -a modules/ /usr/local/lib/bjou/modules/	
	@cp bjou.h /usr/local/include

clean:
	@\rm -f obj/debug/src/*.o
	@\rm -f obj/debug/tclap/*.o
	@\rm -f obj/release/src/*.o
	@\rm -f obj/release/tclap/*.o
	@\rm -f bin/$(TARGET)
#######################################################################
endif # endif for progress bar
