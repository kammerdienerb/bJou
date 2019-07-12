#! /usr/bin/env bash

BUILD_TYPE="Release"

while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -i|--install-prefix)
    INSTALL_PREFIX="$2"
    shift # past argument
    shift # past value
    ;;
    -b|--build-type)
    BUILD_TYPE="$2"
    shift # past argument
    shift # past value
    ;;
    -l|--llvm-config)
    LLVM_CONFIG="$2"
    shift # past argument
    shift # past value
    ;;
    *)    # unknown option
    echo "unknown option $1"
    exit 1
    ;;
esac
done

if [ "${BUILD_TYPE}" != "Debug" ] && [ "${BUILD_TYPE}" != "Release" ]; then
    echo "build type '${BUILD_TYPE}' is invalid -- must be 'Debug' or 'Release'"
    exit 1
fi

function has_cmd() {
    HAS=$(command -v "$1" 2> /dev/null)
    if [[ "$HAS" != "" ]]; then
        return 0
    fi

    return 1
}

if has_cmd getconf ; then
    CORES=$(getconf _NPROCESSORS_ONLN 2> /dev/null || \
            getconf  NPROCESSORS_ONLN 2> /dev/null || \
            echo 1)
else
    CORES=1
fi

# clean
./clean.sh

# prepare nolibc_syscall
if [ ! -d nolibc_syscall ]; then
    mkdir -p nolibc_syscall
    git clone https://www.github.com/kammerdienerb/nolibc_syscall.git
    cd nolibc_syscall
    make
    cd ..
fi

# build bJou
mkdir -p build
cd build

if [ -z ${INSTALL_PREFIX+x} ]; then
    if [ -z ${LLVM_CONFIG+x} ]; then
        cmake                                        \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=YES      \
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE}         \
            ..
    else
        cmake                                        \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=YES      \
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE}         \
            -DLLVM_CONFIG=${LLVM_CONFIG}             \
            ..
    fi
else
    if [ -z ${LLVM_CONFIG+x} ]; then
        cmake                                        \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=YES      \
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE}         \
            -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
            ..
    else
        cmake                                        \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=YES      \
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE}         \
            -DLLVM_CONFIG=${LLVM_CONFIG}             \
            -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
            ..
    fi
fi

time make -j$CORES

cp compile_commands.json ..
