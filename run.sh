#!/bin/bash

rm -rf build
mkdir -p build
cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-Ofast"
ninja
./montecarlo
