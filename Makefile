TARGET		= bjou
CC 			= g++
INCLUDE		= include
TCLAP_INCLUDE = . 
R_CFLAGS 	= -I$(INCLUDE) -I$(TCLAP_INCLUDE) -DBJOU_USE_COLOR $(shell llvm-config --cxxflags)
D_CFLAGS 	= -I$(INCLUDE) -I$(TCLAP_INCLUDE) -DBJOU_USE_COLOR -DBJOU_DEBUG_BUILD $(shell llvm-config --cxxflags) -w -g
LLVM_LIBS	= $(shell llvm-config --libs all --system-libs)
R_LFLAGS	= $(shell llvm-config --ldflags) $(LLVM_LIBS) -llldYAML -llldReaderWriter -llldMachO -llldELF -llldDriver -llldCore -llldConfig -llldCOFF 
D_LFLAGS	= $(R_LFLAGS)
SRC			= $(wildcard src/*.cpp)
TCLAP_SRC	= $(wildcard tclap/*.cpp)
ALL_SRC		= $(SRC)
ALL_SRC 	+= $(TCLAP_SRC)
R_OBJ 		= $(patsubst %.cpp,obj/release/%.o,$(ALL_SRC))
D_OBJ 		= $(patsubst %.cpp,obj/debug/%.o,$(ALL_SRC))

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
	@$(CC) -o bin/$(TARGET) $(D_LFLAGS) $? 

.PHONY: debug _debug

release:; @$(MAKE) _release -j$(CORES)
_release: $(R_OBJ)
	@mkdir -p bin
	@echo "------------------------------"
	@echo "Building Release Target $(TARGET)"
	@echo
	@$(CC) -o bin/$(TARGET) $? $(R_LFLAGS) 

.PHONY: release _release

obj/debug/%.o: %.cpp
	@mkdir -p obj/debug/src
	@mkdir -p obj/debug/tclap
	@echo "Compiling $<"
	@$(CC) $(D_CFLAGS) -c -o $@ $<	

obj/release/%.o: %.cpp
	@mkdir -p obj/release/src
	@mkdir -p obj/release/tclap
	@echo "Compiling $<"
	@$(CC) $(R_CFLAGS) -c -o $@ $<

clean:
	@\rm -f obj/debug/src/*.o
	@\rm -f obj/debug/tclap/*.o
	@\rm -f obj/release/src/*.o
	@\rm -f obj/release/tclap/*.o
	@\rm -f bin/$(TARGET)
