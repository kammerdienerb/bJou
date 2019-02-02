#! /usr/bin/env bash

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
mkdir -p nolibc_syscall
git clone https://www.github.com/kammerdienerb/nolibc_syscall.git
cd nolibc_syscall
make
cd ..

# build bJou
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE="Release" \
    ..
make -j$CORES
