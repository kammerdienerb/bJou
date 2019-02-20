cmake_minimum_required(VERSION 3.0.1)

project(bJou)

#######################################################################
# Begin RPATH settings

# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# End RPATH settings
#######################################################################


add_subdirectory(gen_sys)

# stolen from ldc https://github.com/ldc-developers/ldc/blob/master/cmake/Modules/FindLLVM.cmake
set(llvm_config_names llvm-config-8.0 llvm-config80
                      llvm-config-7.0 llvm-config70
                      llvm-config-6.0 llvm-config60
                      llvm-config-5.0 llvm-config50
                      llvm-config-4.0 llvm-config40
                      llvm-config-3.9 llvm-config39
                      llvm-config)

find_program(LLVM_CONFIG_SEARCH
    NAMES ${llvm_config_names})

set(LLVM_CONFIG ${LLVM_CONFIG_SEARCH}
    CACHE PATH "The path the llvm-config executable")

execute_process(COMMAND "${LLVM_CONFIG}" "--prefix"
    OUTPUT_VARIABLE LLVM_ROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND "${LLVM_CONFIG}" "--version"
    OUTPUT_VARIABLE LLVM_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE)

message(STATUS "Using LLVM version ${LLVM_VER} from '${LLVM_ROOT}'")

execute_process(COMMAND "${LLVM_CONFIG}" "--cmakedir"
    OUTPUT_VARIABLE LLVM_CMAKE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)

include("${LLVM_CMAKE_DIR}/LLVMConfig.cmake")

execute_process(COMMAND "${LLVM_CONFIG}" "--cxxflags"
    OUTPUT_VARIABLE LLVM_CXX_FLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND "${LLVM_CONFIG}" "--ldflags"
    OUTPUT_VARIABLE LLVM_LD_FLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND "${LLVM_CONFIG}" "--system-libs"
    OUTPUT_VARIABLE LLVM_SYSTEM_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE)

file(GLOB CPP_FILES src/*.cpp)
file(GLOB C_FILES src/*.c)
file(GLOB TCLAP_SOURCES tclap/*.cpp)

set(BJOU_SOURCES
    ${CPP_FILES}
    ${C_FILES}
    ${TCLAP_SOURCES})

set(BJOU_INCLUDE
    include
    tclap
    ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_CXX_STANDARD 11)

add_executable(bjou ${BJOU_SOURCES})

target_include_directories(bjou PRIVATE ${BJOU_INCLUDE})
target_include_directories(bjou PRIVATE ${LLVM_INCLUDE_DIRS})

target_compile_options(bjou PRIVATE -fno-rtti -fno-exceptions -w)

target_link_libraries(bjou
        ${LLVM_LD_FLAGS}
        ${PROJECT_SOURCE_DIR}/nolibc_syscall/nolibc_syscall.o
        LLVM
        ffi
        pthread
        ${LLVM_SYSTEM_LIBS})

target_compile_definitions(bjou PRIVATE BJOU_USE_COLOR)
target_compile_definitions(bjou PRIVATE BJOU_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")
target_compile_definitions(bjou PRIVATE $<$<CONFIG:Debug>:BJOU_DEBUG_BUILD>)

# installation
install(FILES nolibc_syscall/libnolibc_syscall.a
        DESTINATION lib)

install(DIRECTORY modules
        DESTINATION lib/bjou)

install(TARGETS bjou
        RUNTIME DESTINATION bin)

message(STATUS "Install prefix is '${CMAKE_INSTALL_PREFIX}'")
