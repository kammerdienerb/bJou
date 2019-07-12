cmake_minimum_required(VERSION 3.0.1)

project(bJou)


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


if(EXISTS "${LLVM_INCLUDE_DIR}/lld")
    set(BJOU_HAS_LLD TRUE)
    message(STATUS "Using lld")
else()
    message(STATUS "Did not find lld")
endif()


# - Find Tcmalloc
# Find the native Tcmalloc includes and library
#
#  Tcmalloc_INCLUDE_DIR - where to find Tcmalloc.h, etc.
#  Tcmalloc_LIBRARIES   - List of libraries when using Tcmalloc.
#  Tcmalloc_FOUND       - True if Tcmalloc found.

find_path(Tcmalloc_INCLUDE_DIR google/tcmalloc.h NO_DEFAULT_PATH PATHS
  ${HT_DEPENDENCY_INCLUDE_DIR}
  /usr/include
  /opt/local/include
  /usr/local/include
)

if (USE_TCMALLOC)
  set(Tcmalloc_NAMES tcmalloc)
else ()
  set(Tcmalloc_NAMES tcmalloc_minimal tcmalloc)
endif ()

find_library(Tcmalloc_LIBRARY NO_DEFAULT_PATH
  NAMES ${Tcmalloc_NAMES}
  PATHS ${HT_DEPENDENCY_LIB_DIR} /lib /usr/lib /usr/local/lib /opt/local/lib
)

if (Tcmalloc_INCLUDE_DIR AND Tcmalloc_LIBRARY)
  set(Tcmalloc_FOUND TRUE)
  set( Tcmalloc_LIBRARIES ${Tcmalloc_LIBRARY} )
else ()
  set(Tcmalloc_FOUND FALSE)
  set( Tcmalloc_LIBRARIES )
endif ()

if (Tcmalloc_FOUND)
  message(STATUS "Found Tcmalloc: ${Tcmalloc_LIBRARY}")
else ()
  message(STATUS "Not Found Tcmalloc: ${Tcmalloc_LIBRARY}")
  if (Tcmalloc_FIND_REQUIRED)
    message(STATUS "Looked for Tcmalloc libraries named ${Tcmalloc_NAMES}.")
    message(FATAL_ERROR "Could NOT find Tcmalloc library")
  endif ()
endif ()

mark_as_advanced(
  Tcmalloc_LIBRARY
  Tcmalloc_INCLUDE_DIR
)


#######################################################################
# Begin RPATH settings

# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

if(APPLE)
    SET(MACOSX_RPATH 1)
endif(APPLE)

SET(CMAKE_INSTALL_RPATH "${LLVM_ROOT}/lib;${CMAKE_INSTALL_PREFIX}/lib")
# End RPATH settings
#######################################################################

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

set(CMAKE_CXX_STANDARD 14)

add_executable(bjou ${BJOU_SOURCES})

target_include_directories(bjou PRIVATE ${BJOU_INCLUDE})
target_include_directories(bjou PRIVATE ${LLVM_INCLUDE_DIRS})

target_compile_options(bjou PRIVATE -fno-rtti -w)

target_link_libraries(bjou
        ${LLVM_LD_FLAGS}
        LLVM
        ${PROJECT_SOURCE_DIR}/nolibc_syscall/nolibc_syscall.o
        ffi
        pthread
        ${LLVM_SYSTEM_LIBS})
if(BJOU_HAS_LLD)
    target_link_libraries(bjou
        lldCOFF
        lldCommon
        lldCore
        lldDriver
        lldELF
        lldMachO
        lldMinGW
        lldReaderWriter
        lldWasm
        lldYAML)
endif()
target_link_libraries(bjou
        ${Tcmalloc_LIBRARIES})

target_compile_definitions(bjou PRIVATE BJOU_USE_COLOR)
target_compile_definitions(bjou PRIVATE BJOU_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")
target_compile_definitions(bjou PRIVATE $<$<CONFIG:Debug>:BJOU_DEBUG_BUILD>)
if(BJOU_HAS_LLD)
    target_compile_definitions(bjou PRIVATE BJOU_HAS_LLD)
endif()

# installation
install(FILES nolibc_syscall/libnolibc_syscall.a
        DESTINATION lib)

install(DIRECTORY modules
        DESTINATION lib/bjou)

install(TARGETS bjou
        RUNTIME DESTINATION bin)

message(STATUS "Install prefix is '${CMAKE_INSTALL_PREFIX}'")
