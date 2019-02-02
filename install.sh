#!/usr/bin/env bash

if [ -d build ]; then
    cd build
    make install
else
    echo "'./build' directory not found -- did you run build.sh?"
fi
