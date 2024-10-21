#!/bin/bash
set +xe

for file in src/*.cpp; do
    clang++ -std=gnu++20 -c $file -Isrc -o $(basename $file .cpp).o
done

mkdir -p build

clang++ -std=gnu++20 *.o -o build/lynx

rm *.o
